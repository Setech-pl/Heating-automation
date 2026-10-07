#pragma once
#include "udpmessengerservice.h"
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <WiFiUdp.h>
#include <cerrno>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace
{
// ArduinoJson 5 accepts prefixes, comments and non-JSON tokens. Check the
// complete input grammar first; ArduinoJson still owns decoding and storage.
// No allocation, and recursion is bounded by the library's nesting limit.
class JsonInput
{
  const char *_cursor;
  const char *_end;

  void spaces()
  {
    while (_cursor != _end && (*_cursor == ' ' || *_cursor == '\t' ||
           *_cursor == '\r' || *_cursor == '\n')) ++_cursor;
  }
  bool take(char c)
  {
    spaces();
    if (_cursor == _end || *_cursor != c) return false;
    ++_cursor;
    return true;
  }
  bool digit() const
  { return _cursor != _end && *_cursor >= '0' && *_cursor <= '9'; }
  bool string()
  {
    if (!take('"')) return false;
    while (_cursor != _end)
    {
      unsigned char c = static_cast<unsigned char>(*_cursor++);
      if (c == '"') return true;
      if (c < 0x20) return false;
      if (c == '\\')
      {
        if (_cursor == _end) return false;
        c = static_cast<unsigned char>(*_cursor++);
        // ArduinoJson 5.13.5 does not decode Unicode escapes.
        if (c != '"' && c != '\\' && c != '/' && c != 'b' && c != 'f' &&
            c != 'n' && c != 'r' && c != 't') return false;
      }
    }
    return false;
  }
  bool number()
  {
    if (_cursor != _end && *_cursor == '-') ++_cursor;
    if (!digit()) return false;
    if (*_cursor == '0') ++_cursor;
    else while (digit()) ++_cursor;
    if (_cursor != _end && *_cursor == '.')
    {
      ++_cursor;
      if (!digit()) return false;
      while (digit()) ++_cursor;
    }
    if (_cursor != _end && (*_cursor == 'e' || *_cursor == 'E'))
    {
      ++_cursor;
      if (_cursor != _end && (*_cursor == '+' || *_cursor == '-')) ++_cursor;
      if (!digit()) return false;
      while (digit()) ++_cursor;
    }
    return true;
  }
  bool literal(const char *text)
  {
    size_t length = std::strlen(text);
    if (static_cast<size_t>(_end - _cursor) < length ||
        std::memcmp(_cursor, text, length) != 0) return false;
    _cursor += length;
    return true;
  }
  bool collection(bool object, unsigned depth)
  {
    if (depth == 0 || !take(object ? '{' : '[')) return false;
    char close = object ? '}' : ']';
    if (take(close)) return true;
    do
    {
      if (object && (!string() || !take(':'))) return false;
      if (!value(depth - 1)) return false;
      if (take(close)) return true;
    } while (take(','));
    return false;
  }
  bool value(unsigned depth)
  {
    spaces();
    if (_cursor == _end) return false;
    switch (*_cursor)
    {
    case '{': return collection(true, depth);
    case '[': return collection(false, depth);
    case '"': return string();
    case 't': return literal("true");
    case 'f': return literal("false");
    case 'n': return literal("null");
    default: return number();
    }
  }
public:
  JsonInput(const char *input, size_t length) : _cursor(input), _end(input + length) {}
  bool completeObject()
  {
    if (!collection(true, ARDUINOJSON_DEFAULT_NESTING_LIMIT)) return false;
    spaces();
    return _cursor == _end;
  }
};

const char *numberText(JsonVariant value)
{
  // Keep both wire formats in the baseline: decimal strings and JSON numbers.
  if (!value.is<const char *>() && !value.is<float>()) return NULL;
  return value.as<const char *>();
}

bool readID(JsonVariant value, int &result)
{
  const char *text = numberText(value);
  if (!text || !*text) return false;
  const char *digits = text;
  if (*digits == '+' || *digits == '-') ++digits;
  if (!*digits) return false;
  for (const char *p = digits; *p; ++p)
    if (*p < '0' || *p > '9') return false;
  errno = 0;
  char *end = NULL;
  long parsed = std::strtol(text, &end, 10);
  if (errno == ERANGE || *end || parsed < INT_MIN || parsed > INT_MAX) return false;
  result = static_cast<int>(parsed);
  return true;
}

bool readTemperature(JsonVariant value, float &result)
{
  const char *text = numberText(value);
  if (!text || !*text) return false;
  // Require a decimal token, excluding whitespace, hex and atof's prefixes.
  for (const char *p = text; *p; ++p)
    if ((*p < '0' || *p > '9') && *p != '+' && *p != '-' && *p != '.' &&
        *p != 'e' && *p != 'E') return false;
  errno = 0;
  char *end = NULL;
  // The baseline newlib declares strtof globally; its libstdc++ does not
  // enable the C99 forwarding declarations in namespace std.
  float parsed = ::strtof(text, &end);
  if (end == text || *end || errno == ERANGE || !std::isfinite(parsed)) return false;
  result = parsed;
  return true;
}
}

UDPMessengerService::UDPMessengerService(uint16_t port)
{
  _udp.begin(port);
  _listenPort = port;
}

void UDPMessengerService::listen()
{
  int packetSize = _udp.parsePacket();
  if (packetSize > 0 && packetSize <= _MAX_PACKET_SIZE)
  {
    char incomingPacket[_MAX_PACKET_SIZE + 1];
    int len = _udp.read(incomingPacket, _MAX_PACKET_SIZE);
    // Never interpret a failed/short read or a prefix ending at an embedded NUL.
    if (len != packetSize || std::memchr(incomingPacket, '\0', len)) return;
    incomingPacket[len] = '\0';
    processMessage(_udp.remoteIP(), _udp.remotePort(), incomingPacket, len);
  }
}

void UDPMessengerService::getDeviceInfo(JsonObject &result)
{
  result["serialNumber"] = ESP.getChipId();
}

void UDPMessengerService::sendPacket(IPAddress ip, bool broadcast, uint16_t port, const char *content)
{
  //_udp.beginPacket(_udp.remoteIP(), _udp.remotePort());
  if (!broadcast)
  {
    _udp.beginPacket(ip, port);
  }
  else
  {
    _udp.beginPacketMulticast(ip, port, WiFi.localIP());
  }
  _udp.write(content);
  _udp.endPacket();
  Serial.println("Sending packet");
  Serial.println(content);
}

void UDPMessengerService::processMessage(IPAddress senderIp, uint16_t senderPort, char *message, size_t length)
{
  if (!message || length == 0 || length > _MAX_PACKET_SIZE ||
      std::memchr(message, '\0', length) || !JsonInput(message, length).completeObject()) return;
  StaticJsonBuffer<_MAX_PACKET_SIZE> jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(message);
  Serial.println("process msg");
  if (!root.success() || !root["cmd"].is<const char *>()) return;
  const char *cmd = root["cmd"];
  tClientCommand command = {};
  if (!cmd || std::strlen(cmd) >= sizeof(command.cmd) ||
      (std::strcmp(cmd, "ON") != 0 && std::strcmp(cmd, "OFF") != 0 &&
       std::strcmp(cmd, "SHOWSERVER") != 0 && std::strcmp(cmd, "SHOWSTATUS") != 0) ||
      !readID(root["ID"], command.ID) ||
      !readTemperature(root["actualTEMP"], command.actualTEMP) ||
      !readTemperature(root["targetTEMP"], command.targetTEMP)) return;
  std::memcpy(command.cmd, cmd, std::strlen(cmd) + 1);

  // Publish only a fully validated, owned value. Rejections preserve both a
  // pending command and its reply address; no pointer escapes the JSON buffer.
  _currentCommand = command;
  _lastSenderIp = senderIp;
  _lastSenderPort = senderPort;
  _commandFlag = true;
}

//Now i have to send back OK or NO message

void UDPMessengerService::sendBackMessage(bool status, bool runningStatus)
{
  char resultBuffer[_MAX_PACKET_SIZE] = "";
  StaticJsonBuffer<_MAX_PACKET_SIZE> jsonBuffer;
  JsonObject &backmsg = jsonBuffer.createObject();
  if (status)
  {
    backmsg["cmd"] = "OK";
  }
  else
  {
    backmsg["cmd"] = "NO";
  }
  if (runningStatus)
  {
    backmsg["RUNNING"] = "YES";
  }
  else
  {
    backmsg["RUNNING"] = "NO";
  }
  char hr[21];
  sprintf(hr, "%d", now());
  backmsg["TIME"] = hr;
  sprintf(hr, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  backmsg["SERVERIP"] = hr;
  backmsg.printTo(resultBuffer, _MAX_PACKET_SIZE);
  Serial.println("sendBackMessage");
  Serial.println(_lastSenderIp);
  sendPacket(_lastSenderIp, false, _lastSenderPort, resultBuffer);
}

void UDPMessengerService::discoverDevices()
{
  IPAddress broadcastIP = WiFi.localIP();
  char hr[21];
  char resultBuffer[_MAX_PACKET_SIZE] = "";
  sprintf(hr, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  broadcastIP[3] = 255;
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject &result = jsonBuffer.createObject();
  result["cmd"] = "SHOW";
  result["SERVERIP"] = hr;
  sprintf(hr, "%d", now());
  result["TIME"] = hr;
  result.printTo(resultBuffer, _MAX_PACKET_SIZE);
  sendPacket(broadcastIP, true, _listenPort, resultBuffer);
}

tClientCommand UDPMessengerService::getCurrentCommand()
{
  return _currentCommand;
}
bool UDPMessengerService::checkNewCommand()
{
  if (_commandFlag)
  {
    _commandFlag = false;
    return true;
  }
  else
    return false;
}

void UDPMessengerService::setTempFromMQTT(tClientCommand mqttCommand)
{
  IPAddress broadcastIP = WiFi.localIP();
  char hr[21];
  char resultBuffer[_MAX_PACKET_SIZE] = "";
  sprintf(hr, "%d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  broadcastIP[3] = 255;
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject &result = jsonBuffer.createObject();
  result["cmd"] = "MQTTSET";
  result["ID"] = mqttCommand.ID;
  result["SERVERIP"] = hr;
  sprintf(hr, "%d", now());
  result["TIME"] = hr;
  sprintf(hr,  "%f", mqttCommand.targetTEMP);
  result["targetTEMP"] = hr;
  result.printTo(resultBuffer, _MAX_PACKET_SIZE);
  sendPacket(broadcastIP, true, _listenPort, resultBuffer);
}
