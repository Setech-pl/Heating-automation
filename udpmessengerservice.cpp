#pragma once
#include "scheduler.h"
#include "udpmessengerservice.h"
#include <ArduinoJson.h>
#include <TimeLib.h>
#include <WiFiUdp.h>

UDPMessengerService::UDPMessengerService(uint16_t port)
{
  _udp.begin(port);
  _listenPort = port;
}

void UDPMessengerService::listen()
{
  int packetSize = _udp.parsePacket();
  if (packetSize)
  {
    char incomingPacket[_MAX_PACKET_SIZE];
    int len = _udp.read(incomingPacket, _MAX_PACKET_SIZE);
    if (len > 0)
    {

      incomingPacket[len] = 0;
    }
    processMessage(_udp.remoteIP(), _udp.remotePort(), incomingPacket);
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
}

void UDPMessengerService::processMessage(IPAddress senderIp, uint16_t senderPort, char *message)

{
  StaticJsonBuffer<_MAX_PACKET_SIZE> jsonBuffer;
  JsonObject &root = jsonBuffer.parseObject(message);
  Serial.println("process msg");
  if (root.success())
  {
    _commandFlag = true;
    const char *cmd = root["cmd"];
    const char *ID = root["ID"];
    const char *serialID = root["serial"];
    const char *versionC = root["versionC"];
    const char *actualTEMP = root["actualTEMP"]; // "36.6"
    const char *targetTEMP = root["targetTEMP"]; // "36.6"
    _currentCommand.ID = atoi(ID);
    strcpy(_currentCommand.cmd, cmd);
    _currentCommand.actualTEMP = atof(actualTEMP);
    _currentCommand.targetTEMP = atof(targetTEMP);
    _lastSenderIp = senderIp;
    _lastSenderPort = senderPort;
  }
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
  result["cmd"] = "REMOTESET";
  result["SERVERIP"] = hr;
  sprintf(hr, "%d", now());
  result["TIME"] = hr;
  sprintf(hr,  "%f", mqttCommand.targetTEMP);
  result["targetTEMP"] = hr;
  result.printTo(resultBuffer, _MAX_PACKET_SIZE);
  sendPacket(broadcastIP, true, _listenPort, resultBuffer);
}
