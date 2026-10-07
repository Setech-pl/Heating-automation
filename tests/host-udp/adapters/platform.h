#pragma once

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>

class IPAddress
{
  uint8_t bytes[4];
public:
  IPAddress(uint8_t a = 192, uint8_t b = 0, uint8_t c = 2, uint8_t d = 1)
    : bytes{a, b, c, d} {}
  uint8_t &operator[](int i) { return bytes[i]; }
  bool operator==(const IPAddress &other) const
  { return std::memcmp(bytes, other.bytes, sizeof(bytes)) == 0; }
};

struct FakeTransport
{
  std::string packet;
  int reportedSize = 0;
  int readResult = -2; // -2: normal read; -1/0/positive: injected result
  int reads = 0;
  int sends = 0;
  IPAddress sender;
  uint16_t port = 1234;
  IPAddress destination;
  uint16_t destinationPort = 0;
  std::string response;
};
extern FakeTransport transport;

class WiFiUDP
{
public:
  void begin(uint16_t) {}
  int parsePacket() { return transport.reportedSize; }
  int read(char *buffer, size_t capacity)
  {
    ++transport.reads;
    if (transport.readResult != -2 && transport.readResult <= 0)
      return transport.readResult;
    size_t count = std::min(capacity, transport.packet.size());
    if (transport.readResult >= 0)
      count = std::min(count, static_cast<size_t>(transport.readResult));
    std::memcpy(buffer, transport.packet.data(), count);
    return static_cast<int>(count);
  }
  IPAddress remoteIP() { return transport.sender; }
  uint16_t remotePort() { return transport.port; }
  void beginPacket(IPAddress ip, uint16_t port)
  { transport.destination = ip; transport.destinationPort = port; }
  void beginPacketMulticast(IPAddress ip, uint16_t port, IPAddress)
  { beginPacket(ip, port); }
  void write(const char *content) { transport.response = content; }
  void endPacket() { ++transport.sends; }
};

struct FakeSerial
{
  template<class T> void println(const T &) {}
  template<class T> void print(const T &) {}
};
struct FakeESP { unsigned getChipId() { return 123; } };
struct FakeWiFi { IPAddress localIP() { return IPAddress(); } };
extern FakeSerial Serial;
extern FakeESP ESP;
extern FakeWiFi WiFi;
