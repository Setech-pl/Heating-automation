#pragma once
#include "platform.h"
class PubSubClient {
public:
  bool online = false, connectSuccess = false, subscribeSuccess = true;
  int attempts = 0, subscriptions = 0, loops = 0;
  std::string user, password;
  explicit PubSubClient(WiFiClient &) {}
  void setServer(const char *, int) {}
  void setCallback(void (*)(char *, byte *, unsigned int)) {}
  bool connected() { return online; }
  bool connect(const char *) { ++attempts; online = connectSuccess; return online; }
  bool connect(const char *id, const char *login, const char *pass) {
    user = login; password = pass; return connect(id);
  }
  bool subscribe(const char *) { ++subscriptions; return subscribeSuccess; }
  void disconnect() { online = false; }
  bool loop() { ++loops; return online; }
};
