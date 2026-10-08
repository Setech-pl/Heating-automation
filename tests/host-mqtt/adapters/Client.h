#pragma once
#include "Arduino.h"
#include "IPAddress.h"
#include <vector>
class Client : public Print {
public:
  bool tcpSuccess = true, online = false;
  int attempts = 0, stops = 0;
  size_t cursor = 0;
  std::vector<uint8_t> reply, sent;
  int connect(IPAddress, uint16_t) { ++attempts; online = tcpSuccess; return online; }
  int connect(const char *, uint16_t) { return connect(IPAddress(),0); }
  int available() { return static_cast<int>(reply.size() - cursor); }
  int read() { return cursor < reply.size() ? reply[cursor++] : -1; }
  bool connected() { return online; }
  void stop() { ++stops; online = false; }
  void flush() {}
  size_t write(uint8_t value) override { sent.push_back(value); return 1; }
  size_t write(const uint8_t *values, size_t count) override {
    sent.insert(sent.end(),values,values+count); return count;
  }
};
