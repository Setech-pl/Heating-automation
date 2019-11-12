#include <LiquidCrystal_I2C.h>
#include "heating_config.h"
#include <TimeLib.h>
#include "utils.h"
#include <Wire.h>
#include "scheduler.h"
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "screen.h"
#include <ArduinoJson.h>
#include "UDPMessengerService.h"
#include "createDailyPlan.h"

LiquidCrystal_I2C lcd(0x27, 20, 4);

hScheduler *scheduler = new hScheduler();
hConfigurator *config = new hConfigurator();
hPumpsController *heatPumpController = new hPumpsController(scheduler, config);
hScreen *hdisplay = new hScreen(&lcd, config);
UDPMessengerService udpMessenger(3636);
// Enabling MQTT client support
const char clientid[] = "HeatingSrv";
WiFiClient espClient;
PubSubClient client(espClient);
bool gotMQTTCommand = false;
byte *MQTTCommand;
unsigned long timeMillis = 0;
bool internalWIFIMode = false;

/*
 * 
 * Hook functions
 */
void hook_discover_devices()
{
  udpMessenger.discoverDevices();
}

void hook_sanity_check()
{
  heatPumpController->sanityCheck();
}

void hook_restart()
{
  ESP.restart();
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.println(topic);
  if (strcmp(topic, _MQTT_COMMANDS_TOPIC) == 0)
  {
    gotMQTTCommand = true;
  }
  if (gotMQTTCommand)
  {
    Serial.println("got incomming command");
    for (int i = 0; i < length; i++)
    {
      Serial.print((char)payload[i]);
    }
    Serial.println("end of command");
  }
}

/*
Special setup functions
*/

void setup()
{
  Serial.begin(115200);
  Serial.println("Entering setup mode");
  int counter = 0;
  bool wynik = false;
  char temp[21];
  tm t;
  connect_external_wifi connExternalWiFi(true, t, hourly, 0);
  ntp_update ntpUpdateCommand(true, t, hourly, 0);
  lcd.init();
  lcd.backlight(); // Enable or Turn On the backlight
  hdisplay->printSplashScreen();
  hdisplay->renderScreen();
  delay(200);
  while (!wynik && counter < 5)
  {
    sprintf(temp, "Connecting WiFi(%d)", counter);
    hdisplay->printStatusBar(temp);
    hdisplay->renderScreen();
    wynik = connExternalWiFi.execute();
    counter++;
    delay(500);
  }
  hdisplay->printStatusBar(connExternalWiFi.result);
  hdisplay->renderScreen();
  delay(500);
  if (!wynik)
  {
    // I have to turn on internal WiFi
    enable_internal_wifi internalWifiCmd(true, t, hourly, 0);
    counter = 0;
    while (!internalWIFIMode && counter < 5)
    {
      sprintf(temp, "Internal WiFi(%d)", counter);
      hdisplay->printStatusBar(temp);
      hdisplay->renderScreen();
      internalWIFIMode = internalWifiCmd.execute();
      counter++;
      delay(200);
    }
  }
  counter = 0;
  wynik = false;
  //Updating time from NTP time server
  while (!wynik && counter && !internalWIFIMode< 5)
  {
    ;
    sprintf(temp, "Updating NTP(%d)  ", counter);
    hdisplay->printStatusBar(temp);
    hdisplay->renderScreen();
    counter++;
    wynik = ntpUpdateCommand.execute();
    delay(300);
  }
  hdisplay->printStatusBar(ntpUpdateCommand.result);
  hdisplay->renderScreen();
  delay(1000);
  // if _INTERNAL_WIFI_MODE == true then enable internal wifi

  hook_discover_devices();
  char hr[21];
  if (!internalWIFIMode)
  {
    sprintf(hr, " IP : %d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
  }
  else
  {
    sprintf(hr, " IP : %d.%d.%d.%d", WiFi.softAPIP()[0], WiFi.softAPIP()[1], WiFi.softAPIP()[2], WiFi.softAPIP()[3]);
  }
  hdisplay->printStatusBar(hr);
  hdisplay->renderScreen();
  delay(1000);
  timeMillis = 0;

  // add periodical device discovery process
  t.tm_hour = hour();
  t.tm_min = 32;
  t.tm_mday = day();
  t.tm_wday = weekday();
  scheduler->addTask(new hCallbackCommand(false, t, hourly, &hook_discover_devices));

  // add periodical sanity check
  t.tm_hour = 3;
  t.tm_min = 1;
  t.tm_mday = day();
  t.tm_wday = weekday();
  scheduler->addTask(new hCallbackCommand(false, t, daily, &hook_discover_devices));

  // add periodical NTP Time update
  t.tm_hour = 0;
  t.tm_min = 10;
  t.tm_mday = day();
  t.tm_wday = weekday();
  scheduler->addTask(new ntp_update(false, t, daily, 0));

  // Enabling MQTT client support

  client.setServer(_MQTT_SERVER, _MQTT_SERVER_PORT);
  client.setCallback(mqttCallback);
  hook_mqtt_reconnect();
  t.tm_hour = hour();
  t.tm_min = minute();
  scheduler->addTask(new hCallbackCommand(false, t, minutly, &hook_mqtt_reconnect));
}

void hook_mqtt_reconnect()

{
  if (client.connected())
  {
    config->setMQTTStatus(true);
    Serial.println("Checking mqtt status : connected");
  }
  else
  {
    int i = 0;
    while (i < 5)
    {
      i++;
      if (client.connect(_MQTT_CLIENT_ID))
      {
        Serial.println("connected to MQTT");
        config->setMQTTStatus(true);
        client.subscribe(_MQTT_COMMANDS_TOPIC);
        break;
      }
      else
      {
        Serial.println("Error connecting to mqtt server");
        config->setMQTTStatus(false);
        delay(150);
      }
    }
  }
}

void loop()
{

  if (timeMillis + 1000 < millis())
  {
    scheduler->executeTasks();
    config->tickMinutes();
    hdisplay->printMainScreen();
    hdisplay->printStatusBar(_BLANK_LINE);
    hdisplay->renderScreen();
    timeMillis = millis();
  }
  udpMessenger.listen();

  //Incoming commands router

  if (udpMessenger.checkNewCommand())

  {
    tClientCommand temp = udpMessenger.getCurrentCommand();

    if (strcmp(temp.cmd, "ON") == 0)
    {
      char tm[20];
      sprintf(tm, "Pump %d ON Command", temp.ID);
      hdisplay->printMainScreen();
      hdisplay->printStatusBar(tm);
      hdisplay->renderScreen();
      udpMessenger.sendBackMessage(heatPumpController->turnOnHeatPumpReq(temp.ID, temp.actualTEMP, temp.targetTEMP), config->getPumpStatus(temp.ID));
      if (config->getMQTTStatus())
      {
        char subtopic[16];
        sprintf(subtopic, "%s/%s%d", _MQTT_SENSORS_TOPIC, "thermostat", temp.ID);
        Serial.println(subtopic);
        // client.publish(#_MQTT_SENSORS_TOPIC "/" , hr);
      }
    }

    if (strcmp(temp.cmd, "OFF") == 0)
    {
      char tm[20];
      sprintf(tm, "Pump %d OFF", temp.ID);
      hdisplay->printMainScreen();
      hdisplay->printStatusBar(tm);
      hdisplay->renderScreen();
      udpMessenger.sendBackMessage(heatPumpController->turnOffHeatPumpReq(temp.ID, temp.actualTEMP, temp.targetTEMP), config->getPumpStatus(temp.ID));
    }

    if (strcmp(temp.cmd, "SHOWSERVER") == 0)
    {
      char tm[20];
      sprintf(tm, "Registering C%d", temp.ID);
      hdisplay->printMainScreen();
      hdisplay->printStatusBar(tm);
      hdisplay->renderScreen();
      hook_discover_devices();
    }

    if (strcmp(temp.cmd, "SHOWSTATUS") == 0)
    {
      udpMessenger.sendBackMessage(true, config->getPumpStatus(temp.ID));
    }
  }

  // MQTT client section
  client.loop();
  if (gotMQTTCommand)
  {
    //incoming external command - set temp to Thermo Client ID
    Serial.println('i got mqtt command in main loop');
    gotMQTTCommand = false;
  }
}
