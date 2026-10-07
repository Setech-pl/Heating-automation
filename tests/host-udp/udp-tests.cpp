#include "udpmessengerservice.h"
#include "platform.h"
#include <cstdlib>
#include <iostream>

FakeTransport transport;
FakeSerial Serial;
FakeESP ESP;
FakeWiFi WiFi;

namespace
{
int checks = 0;
int publications = 0;
int executionAttempts = 0;
std::string context;

void require(bool condition, const char *description)
{
  ++checks;
  if (!condition)
  {
    std::cerr << "FAIL: " << context << ": " << description << "\n";
    std::exit(1);
  }
}

std::string message(const std::string &cmd = "\"ON\"",
                    const std::string &id = "\"1\"",
                    const std::string &actual = "\"18\"",
                    const std::string &target = "\"21\"")
{
  const char *names[] = {"cmd", "ID", "actualTEMP", "targetTEMP"};
  const std::string values[] = {cmd, id, actual, target};
  std::string result = "{";
  for (int i = 0; i < 4; ++i)
  {
    if (values[i] == "MISSING") continue;
    if (result.size() > 1) result += ',';
    result += std::string("\"") + names[i] + "\":" + values[i];
  }
  return result + '}';
}

void deliver(UDPMessengerService &service, const std::string &packet,
             int reported = -2, int readResult = -2)
{
  transport.packet = packet;
  transport.reportedSize = reported == -2 ? static_cast<int>(packet.size()) : reported;
  transport.readResult = readResult;
  service.listen();
  transport.packet.assign(1024, 'X'); // transport storage must not back the command
}

bool same(const tClientCommand &a, const tClientCommand &b)
{
  return a.ID == b.ID && std::memcmp(a.cmd, b.cmd, sizeof(a.cmd)) == 0 &&
    a.actualTEMP == b.actualTEMP && a.targetTEMP == b.targetTEMP &&
    a.actualHum == b.actualHum && a.isRunning == b.isRunning &&
    std::memcmp(a.serialID, b.serialID, sizeof(a.serialID)) == 0 &&
    std::memcmp(a.versionC, b.versionC, sizeof(a.versionC)) == 0;
}

// Observe the production handoff used by loop(). This is a counting consumer,
// not an implementation of parser/controller/scheduler logic.
bool consume(UDPMessengerService &service)
{
  if (!service.checkNewCommand()) return false;
  ++publications;
  tClientCommand command = service.getCurrentCommand();
  if (std::strcmp(command.cmd, "ON") == 0 || std::strcmp(command.cmd, "OFF") == 0)
    ++executionAttempts;
  return true;
}

void accept(UDPMessengerService &service, const std::string &packet,
            const char *cmd = "ON", int id = 1, float actual = 18, float target = 21)
{
  context = "accept " + packet.substr(0, 100);
  int before = publications;
  int sends = transport.sends;
  deliver(service, packet);
  tClientCommand expected = {};
  expected.ID = id;
  std::strcpy(expected.cmd, cmd);
  expected.actualTEMP = actual;
  expected.targetTEMP = target;
  require(same(service.getCurrentCommand(), expected), "owned, completely initialized command");
  require(consume(service), "one publication");
  require(publications == before + 1, "one handoff");
  require(!consume(service), "flag consumed exactly once");
  require(transport.sends == sends, "parser sends no response/control packet");
}

void reject(UDPMessengerService &service, const std::string &packet,
            int reported = -2, int readResult = -2)
{
  context = "reject " + packet.substr(0, 100);
  tClientCommand before = service.getCurrentCommand();
  int count = publications;
  int executions = executionAttempts;
  int sends = transport.sends;
  deliver(service, packet, reported, readResult);
  require(same(service.getCurrentCommand(), before), "rejection preserves every state field");
  require(!consume(service), "no published command/callback");
  require(count == publications && executions == executionAttempts, "no handoff/execution attempt");
  require(transport.sends == sends, "no transport side effect");
}
}

int main()
{
  UDPMessengerService service(3636);
  context = "initial state";
  require(same(service.getCurrentCommand(), tClientCommand{}), "initialized initial state");
  require(!consume(service), "no initial command");
  reject(service, "");
  for (const char *cmd : {"ON", "OFF", "SHOWSERVER", "SHOWSTATUS"})
  {
    accept(service, message(std::string("\"") + cmd + "\""), cmd);
    accept(service, message(std::string("\"") + cmd + "\"", "1", "18", "21"), cmd);
  }
  accept(service, message("\"ON\"", "\"11\"", "36.6", "38.6"), "ON", 11, 36.6f, 38.6f);
  accept(service, " \r\n\t" + message() + " \r\n\t");
  accept(service, message("\"ON\"", "\"+1\"", "\"-1.25e1\"", "\"+.5\""), "ON", 1, -12.5f, .5f);
  // ID domains/temperature policy remain in the controller; the parser checks
  // representation, not a newly invented thermostat/pump mapping.
  accept(service, message("\"ON\"", "2147483647"), "ON", 2147483647);
  accept(service, message("\"ON\"", "\"-2147483648\""), "ON", (-2147483647 - 1));
  accept(service, message("\"ON\"", "0"), "ON", 0);
  accept(service, message("\"ON\"", "\"1\"", "-0", "2.1e1"), "ON", 1, -0.0f, 21);
  accept(service, message("\"ON\"", "\"1\"", "-3.402823466e38", "3.402823466e38"),
         "ON", 1, -3.402823466e38f, 3.402823466e38f);
  for (size_t length : {size_t(510), size_t(511), size_t(512)})
    accept(service, message() + std::string(length - message().size(), ' '));
  for (size_t length : {size_t(513), size_t(514), size_t(1024)})
  {
    int reads = transport.reads;
    reject(service, message() + std::string(length - message().size(), ' '));
    require(transport.reads == reads, "oversize datagram not read as a prefix");
  }
  for (int result : {-100, -1, 0, 1, 20}) reject(service, message(), -2, result);
  reject(service, message() + "   ", -2, static_cast<int>(message().size()));
  reject(service, message(), 0);
  reject(service, message(), -1);
  reject(service, message(), static_cast<int>(message().size() - 1));
  reject(service, message(), static_cast<int>(message().size() + 1));
  for (size_t length = 0; length < message().size(); ++length)
    reject(service, message().substr(0, length));
  for (size_t index : {size_t(0), size_t(4), message().size() - 1})
  {
    std::string packet = message();
    packet[index] = '\0';
    reject(service, packet);
  }
  reject(service, message() + std::string("\0garbage", 8));
  reject(service, message() + '\0');
  for (const char *packet : {"{}", "[]", "null", "true", "42", "\"ON\"", "{", "}",
       "{cmd:'ON',ID:1,actualTEMP:18,targetTEMP:21}",
       "{\"cmd\":\"ON\",\"ID\":01,\"actualTEMP\":18,\"targetTEMP\":21}",
       "{\"cmd\":\"ON\",\"ID\":1,\"actualTEMP\":.5,\"targetTEMP\":21}",
       "{\"cmd\":\"ON\",\"ID\":1,\"actualTEMP\":1.,\"targetTEMP\":21}",
       "{\"cmd\":\"ON\",\"ID\":1,\"actualTEMP\":+18,\"targetTEMP\":21}"})
    reject(service, packet);
  for (const std::string &suffix : {std::string("garbage"), std::string("{}"),
       std::string(" /* comment */"), std::string("\v")}) reject(service, message() + suffix);
  for (const char *extra : {",", ",\"ignored\":tru", ",\"ignored\":1e+",
       ",\"ignored\":\"unterminated", ",\"ignored\":\"bad\\q\"",
       ",\"ignored\":\"bad\\u0000\"", ",ignored:18", ",\"ignored\":[1,]",
       ",\"ignored\":{\"key\":true,}"})
    reject(service, message().substr(0, message().size() - 1) + extra + '}');
  reject(service, message("\"ON\\u0000OFF\""));
  reject(service, message("\"ON\n\""));
  for (int field = 0; field < 4; ++field)
  {
    for (const char *bad : {"MISSING", "null", "true", "false", "[]", "{}", "\"\""})
    {
      std::string values[] = {"\"ON\"", "\"1\"", "\"18\"", "\"21\""};
      values[field] = bad;
      reject(service, message(values[0], values[1], values[2], values[3]));
    }
  }
  reject(service, message("18"));
  for (size_t length : {size_t(19), size_t(20), size_t(160)})
    reject(service, message('"' + std::string(length, 'A') + '"'));
  for (const char *cmd : {"UNKNOWN", "on", "ON ", "ONOFF"})
    reject(service, message(std::string("\"") + cmd + "\""));
  for (const char *id : {"\"1junk\"", "\" 1\"", "\"1 \"", "\"+\"", "\"--1\"",
       "1.0", "1e0", "\"1.5\"", "\"0x1\"", "2147483648", "-2147483649",
       "\"999999999999999999999999999999\"", "NaN", "Infinity"})
    reject(service, message("\"ON\"", id));
  for (const char *temp : {"\"nan\"", "\"NaN\"", "\"inf\"", "\"-inf\"",
       "\"Infinity\"", "NaN", "Infinity", "-Infinity", "1e1000", "-1e1000",
       "3.5e38", "1e-1000", "\"18junk\"", "\"18 19\"", "\" 18\"", "\"18 \"",
       "\"0x1p2\"", "\".\"", "\"+\"", "\"1e\"", "\"1e+\"", "\"1.2.3\""})
  {
    reject(service, message("\"ON\"", "1", temp));
    reject(service, message("\"ON\"", "1", "18", temp));
  }
  reject(service, message().substr(0, message().size() - 1) +
         ",\"ignored\":" + std::string(20, '[') + "0" + std::string(20, ']') + '}');
  // A valid grammar can still exhaust the real StaticJsonBuffer; no partial state.
  std::string manyFields = message().substr(0, message().size() - 1);
  for (int i = 0; i < 30; ++i) manyFields += ",\"x" + std::to_string(i) + "\":0";
  reject(service, manyFields + '}');
  for (size_t length : {size_t(31), size_t(32), size_t(33), size_t(300)})
    accept(service, message().substr(0, message().size() - 1) +
           ",\"serial\":\"" + std::string(length, 'S') + "\",\"versionC\":\"1.1\"}");
  accept(service, message().substr(0, message().size() - 1) +
         ",\"ignored\":{\"list\":[null,true,false,\"escaped\\\"\\\\\\/\\b\\f\\n\\r\\t\"]}}");
  accept(service, message("\"OFF\"", "2", "19.5", "22.25"), "OFF", 2, 19.5f, 22.25f);
  reject(service, "{}");
  accept(service, message());

  // Invalid input must not replace or clear an unconsumed valid command, nor
  // redirect its later reply to the sender of the rejected datagram.
  UDPMessengerService pending(3636);
  transport.sender = IPAddress(192, 0, 2, 10);
  transport.port = 1111;
  deliver(pending, message());
  tClientCommand saved = pending.getCurrentCommand();
  int sends = transport.sends;
  int executions = executionAttempts;
  transport.sender = IPAddress(192, 0, 2, 20);
  transport.port = 2222;
  deliver(pending, message("\"OFF\"", "\"2\"", "\"19\"", "null"));
  context = "pending command after rejected input";
  require(same(pending.getCurrentCommand(), saved), "pending value preserved atomically");
  require(transport.sends == sends && executionAttempts == executions, "rejection has no effects");
  require(consume(pending), "original pending publication retained");
  require(!consume(pending), "no second publication from rejected packet");
  pending.sendBackMessage(true, false);
  require(transport.destination == IPAddress(192, 0, 2, 10) &&
          transport.destinationPort == 1111, "reply still belongs to original valid sender");
  std::cout << "PASS: " << checks << " assertions; " << publications
            << " valid publications; " << executionAttempts << " observed ON/OFF handoffs\n";
  return 0;
}
