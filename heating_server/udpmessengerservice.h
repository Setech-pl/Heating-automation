#pragma once
#ifndef UDPMESSENGERSERVICE_H
#endif /* UDPMESSENGERSERVICE_H */
#define UDPMESSENGERSERVICE_H

#include <WiFiUdp.h>
#include <ESP8266WiFi.h>
#include <ArduinoJson.h>
#include "scheduler.h"

/* A message from Thermostat client
 {
  "cmd": "ON",
  "ID": "1",
"serial":"123456",
"versionC":"1.1",
"actualTEMP":"36.6",
"targetTEMP":"38.6",
"actualHumidity":"90"
}


 {"cmd":"ON","ID":"11","serial":"1123456","versionC":"11.1","actualTEMP":36.6,"targetTEMP":38.6}
 */

struct tClientCommand
{
  int ID;
  char cmd[20];
  float actualTEMP;
  float targetTEMP;
  float actualHum;
  char serialID[32];
  char versionC[12];
};

class UDPMessengerService
{
protected:
  static const int _MAX_PACKET_SIZE = 512;
  uint16_t _listenPort;
  WiFiUDP _udp;
  void processMessage(IPAddress senderIp, uint16_t senderPort, char *message);
  void sendPacket(IPAddress ip, bool broadcast, uint16_t port, const char *content);
  void getDeviceInfo(JsonObject &result);

private:
  bool _commandFlag = false;
  IPAddress _lastSenderIp;
  uint16_t _lastSenderPort;
  tClientCommand _currentCommand;

public:
  UDPMessengerService(uint16_t port);
  void listen();
  void discoverDevices(); //send broadCast Packet
  tClientCommand getCurrentCommand();
  bool checkNewCommand();
  void sendBackMessage(bool status, bool runningStatus);
  void setTempFromMQTT(tClientCommand mqttCommand);
};
