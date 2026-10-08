#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

using byte = unsigned char;
class String : public std::string {
public:
  using std::string::string;
  String(unsigned long number) : std::string(std::to_string(number)) {}
  String(const std::string &text) : std::string(text) {}
};

struct FakePlatform {
  unsigned long milliseconds = 10000;
  time_t calendarEpoch = 1791462840; // 2026-10-08 12:34 UTC; deterministic fallback.
  bool calendarValid = true;
  int ntpRequests = 0;
  int ntpReads = 0;
  int succeedOnRequest = 1; // 0: timeout on every request
  unsigned long epoch = 1700000000;
  int setTimeCalls = 0;
  time_t clock = 0;
  bool clockValid = false;
  std::vector<unsigned long> delays;
};
extern FakePlatform platform;
inline unsigned long millis() { return platform.milliseconds; }
inline void delay(unsigned long milliseconds) {
  platform.delays.push_back(milliseconds);
  platform.milliseconds += milliseconds;
}
inline unsigned long word(byte high, byte low) { return (high << 8) | low; }

class IPAddress {
  byte bytes[4];
public:
  IPAddress(byte a = 192, byte b = 0, byte c = 2, byte d = 1) : bytes{a,b,c,d} {}
  byte operator[](int index) const { return bytes[index]; }
  byte &operator[](int index) { return bytes[index]; }
  bool operator==(const IPAddress &other) const { return std::memcmp(bytes, other.bytes, 4) == 0; }
};

class UDP {
public:
  virtual ~UDP() {}
  virtual int begin(int) = 0;
  virtual int beginPacket(const char *, int) = 0;
  virtual size_t write(const byte *, size_t) = 0;
  virtual int endPacket() = 0;
  virtual int parsePacket() = 0;
  virtual int read(byte *, size_t) = 0;
  virtual void stop() = 0;
};
struct FakeDatagrams {
  std::string packet, response;
  int reportedSize = 0, sends = 0;
  IPAddress sender, destination;
  uint16_t port = 1234, destinationPort = 0;
};
inline FakeDatagrams &datagrams() { static FakeDatagrams value; return value; }

class WiFiUDP : public UDP {
  bool application = false;
public:
  int begin(int port) override { application = port == 3636; return 1; }
  int beginPacket(const char *, int) override { return 1; }
  int beginPacket(IPAddress address, uint16_t port) {
    datagrams().destination = address; datagrams().destinationPort = port; return 1;
  }
  size_t write(const byte *, size_t count) override { return count; }
  size_t write(const char *text) { datagrams().response = text; return std::strlen(text); }
  int endPacket() override {
    if (application) ++datagrams().sends; else ++platform.ntpRequests;
    return 1;
  }
  int parsePacket() override {
    if (application) return datagrams().reportedSize;
    return platform.succeedOnRequest > 0 &&
           platform.ntpRequests >= platform.succeedOnRequest ? 48 : 0;
  }
  int read(char *buffer, size_t capacity) {
    size_t count = std::min(capacity, datagrams().packet.size());
    std::memcpy(buffer, datagrams().packet.data(), count);
    datagrams().reportedSize = 0;
    return static_cast<int>(count);
  }
  int read(byte *buffer, size_t capacity) override {
    ++platform.ntpReads;
    if (capacity < 48) return 0;
    std::memset(buffer, 0, 48);
    uint32_t seconds = static_cast<uint32_t>(platform.epoch + 2208988800UL);
    for (int i = 0; i < 4; ++i) buffer[40 + i] = seconds >> (24 - i * 8);
    return 48;
  }
  IPAddress remoteIP() { return datagrams().sender; }
  uint16_t remotePort() { return datagrams().port; }
  void stop() override {}
};

enum { WIFI_STA, WIFI_AP_STA, WL_CONNECTED };
struct FakeWiFi {
  IPAddress station, accessPoint;
  int localReads = 0, apReads = 0;
  bool connected = true;
  std::string ssid = "test-network";
  IPAddress localIP() { ++localReads; return station; }
  IPAddress softAPIP() { ++apReads; return accessPoint; }
  int RSSI() { return -75; }
  String SSID() { return ssid; }
  void disconnect() {}
  void mode(int) {}
  void begin(const char *, const char *) {}
  int status() { return connected ? WL_CONNECTED : -1; }
  bool softAP(const char *, const char *, int, bool, int) { return true; }
};
struct FakeSerial {
  std::vector<std::string> contactEvents;
  std::string prefix;
  void begin(int) {}
  void print(const char *text) { prefix = text; }
  void println(int id) {
    if (prefix.find("Thermostat contact") == 0) contactEvents.push_back(prefix + std::to_string(id));
    prefix.clear();
  }
  template<class T> void print(const T &) {}
  template<class T> void println(const T &) {}
};
extern FakeWiFi WiFi;
extern FakeSerial Serial;

struct FakeESP { unsigned getChipId() { return 123; } void restart() {} };
extern FakeESP ESP;
class WiFiClient {
public:
  unsigned long timeout = 0;
  void setTimeout(unsigned long value) { timeout = value; }
};

struct GpioCall { int pin, value; bool mode; };
inline std::vector<GpioCall> &gpioCalls() { static std::vector<GpioCall> calls; return calls; }
enum { OUTPUT = 1, SDA = 4, SCL = 5 };
inline void digitalWrite(int pin, int value) { gpioCalls().push_back({pin, value, false}); }
inline void pinMode(int pin, int mode) { gpioCalls().push_back({pin, mode, true}); }

class LiquidCrystal_I2C {
  int row = 0;
public:
  LiquidCrystal_I2C() = default;
  LiquidCrystal_I2C(int, int, int) {}
  void init() {}
  int clears = 0, writes = 0;
  std::string rows[4];
  std::vector<std::string> statusWrites;
  void clear() { ++clears; for (auto &line : rows) line.clear(); }
  void setCursor(int column, int line) {
    if (column != 0 || line < 0 || line >= 4) std::abort();
    row = line;
  }
  void print(const char *text) {
    if (std::strlen(text) > 20) { std::fprintf(stderr, "LCD row exceeds 20 bytes: %s\n", text); std::abort(); }
    ++writes;
    rows[row] = text;
    if (row == 3) statusWrites.push_back(text);
  }
  void backlight() {}
  void noBacklight() {}
};
