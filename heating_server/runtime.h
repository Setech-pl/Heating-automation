#pragma once
#include <stdint.h>

inline bool heatingTickDue(uint32_t current, uint32_t &previous, uint32_t interval) {
  if (static_cast<uint32_t>(current - previous) < interval) return false;
  previous = current; // One tick after a stall, no replay storm.
  return true;
}

// Single synchronous attempt, at most once per minute. The pinned MQTT
// library's 1 s socket timeout and WiFiClient's TCP timeout bound the attempt.
class hMqttReconnect {
  uint32_t _previous = 0;
  bool _attempted = false;
public:
  template<class Client>
  bool poll(Client &client, uint32_t current, bool networkAvailable,
            const char *id, const char *user, const char *password, const char *topic) {
    if (!networkAvailable) return false;
    if (client.connected()) return true;
    if (_attempted && static_cast<uint32_t>(current - _previous) < 60000) return false;
    _previous = current;
    _attempted = true;
    bool connected = user[0] ? client.connect(id, user, password) : client.connect(id);
    if (!connected) return false;
    if (!client.subscribe(topic)) { client.disconnect(); return false; }
    return true;
  }
};
