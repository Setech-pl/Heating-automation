#include <PubSubClient.h>
#include "runtime.h"
#include <cstdio>
#include <cstdlib>
#include <string>
unsigned long mqttClock = 0;
static int checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { \
  std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition); std::exit(1); \
} } while (0)

int main() {
  static_assert(MQTT_SOCKET_TIMEOUT == 1, "Match the firmware's pinned MQTT timeout");
  for (bool partial : {false,true}) {
    mqttClock = 0;
    Client transport;
    if (partial) transport.reply = {0x20,2}; // TCP accepts, broker never completes CONNACK.
    PubSubClient client(transport);
    client.setServer("test-broker",1883); client.setCallback(nullptr);
    hMqttReconnect retry;
    CHECK(!retry.poll(client,0,true,"id","dummy-user","dummy-password","topic"));
    CHECK(transport.attempts == 1);
    CHECK(mqttClock >= 1000 && mqttClock <= 1010);
    CHECK(transport.stops == 1 && !transport.online);
    CHECK(!retry.poll(client,59999,true,"id","dummy-user","dummy-password","topic"));
    CHECK(transport.attempts == 1);
    CHECK(!retry.poll(client,60000,true,"id","dummy-user","dummy-password","topic"));
    CHECK(transport.attempts == 2);
  }
  mqttClock = 0;
  Client transport; transport.reply = {0x20,2,0,0};
  PubSubClient client(transport);
  client.setServer("test-broker",1883); client.setCallback(nullptr);
  hMqttReconnect retry;
  CHECK(retry.poll(client,0,true,"id","dummy-user","dummy-password","topic"));
  CHECK(client.connected()); CHECK(transport.attempts == 1);
  std::string packet(transport.sent.begin(),transport.sent.end());
  CHECK(packet.find("dummy-user") != std::string::npos);
  CHECK(packet.find("dummy-password") != std::string::npos);
  CHECK(packet.find("topic") != std::string::npos);
  CHECK(retry.poll(client,1,true,"id","dummy-user","dummy-password","topic"));
  CHECK(transport.attempts == 1);
  std::printf("PASS: %d assertions; pinned PubSubClient CONNACK timeout, backoff, auth and subscribe\n",checks);
}
