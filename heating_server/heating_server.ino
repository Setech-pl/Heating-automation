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
#include "udpmessengerservice.h"
#include "runtime.h"
#include "relay_output.h"
#if __has_include("relay_config.h")
#include "relay_config.h"
#else
#include "relay_config.example.h"
#endif
#include <new>
#include "createDailyPlan.h"

LiquidCrystal_I2C lcd(0x27, 20, 4);

hArduinoGpio gpio;
hRelayOutputs relayOutputs(gpio, HEATING_RELAYS);
hScheduler schedulerInstance;
hScheduler *scheduler = &schedulerInstance;
hConfigurator configInstance(&relayOutputs);
hConfigurator *config = &configInstance;
hPumpsController controllerInstance(scheduler, config);
hPumpsController *heatPumpController = &controllerInstance;
hScreen displayInstance(&lcd, config);
hScreen *hdisplay = &displayInstance;
UDPMessengerService udpMessenger(3636);
// Enabling MQTT client support
WiFiClient espClient;
PubSubClient client(espClient);
hMqttReconnect mqttReconnect;
uint32_t timeMillis = 0;
uint32_t lastNtpRetry = 0;
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

const char *outputStateName(int pumpId)
{
  hRelayOutputs::State state = config->outputState(pumpId);
  return state == hRelayOutputs::unknown ? "UNCONFIGURED" : state == hRelayOutputs::on ? "ON" : "OFF";
}

void hook_restart()
{
  ESP.restart();
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  // No inbound MQTT command schema exists in this project. Log bounded data;
  // never treat reception/subscription as execution or retain a borrowed payload.
  if (topic == nullptr || payload == nullptr || length > 512 || strcmp(topic, _MQTT_COMMANDS_TOPIC) != 0) return;
  Serial.println("MQTT command unsupported (no protocol configured)");
}

void hook_ntp_update()
{
  if (internalWIFIMode || WiFi.status() != WL_CONNECTED) return;
  tm schedule = {};
  ntp_update command(true, schedule, daily, 0);
  command.execute();
}

/*
Special setup functions
*/

void setup()
{
  Serial.begin(115200);
  if (!relayOutputs.begin()) Serial.println("Some relay channels are unconfigured");
  for (int i = 1; i <= _MAX_HEATING_PUMPS_NO; ++i) {
    thermoClientStat thermostat;
    thermostat.ID = i; thermostat.serialChip = HEATING_THERMOSTAT_SERIALS[i - 1];
    if (!config->registerClient(thermostat)) Serial.println("Thermostat assignment disabled or invalid");
  }
  Serial.println("Entering setup mode");
  int counter = 0;
  bool wynik = false;
  char temp[21];
  tm t = {};
  connect_external_wifi connExternalWiFi(true, t, hourly, 0);
  ntp_update ntpUpdateCommand(true, t, hourly, 0);
  lcd.init();
  hdisplay->backlight();
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
  //Updating time from NTP time server
  ntpUpdateCommand.synchronizeOnStartup(internalWIFIMode, *hdisplay);
  delay(1000);
  // if _INTERNAL_WIFI_MODE == true then enable internal wifi

  udpMessenger.begin(internalWIFIMode);
  hook_discover_devices();
  hdisplay->printNetworkStatus(internalWIFIMode);
  hdisplay->renderScreen();
  delay(1000);
  timeMillis = static_cast<uint32_t>(millis());
  lastNtpRetry = timeMillis;

  // add periodical device discovery process
  t.tm_hour = hour();
  t.tm_min = 32;
  t.tm_mday = day();
  t.tm_wday = weekday();
  scheduler->addTask(new (std::nothrow) hCallbackCommand(false, t, hourly, &hook_discover_devices));

  // add periodical sanity check
  t.tm_hour = hour();
  t.tm_min = minute();
  t.tm_mday = day();
  t.tm_wday = weekday();
  scheduler->addTask(new (std::nothrow) hCallbackCommand(false, t, minutly, &hook_sanity_check));

  // add periodical NTP Time update
  t.tm_hour = 0;
  t.tm_min = 10;
  t.tm_mday = day();
  t.tm_wday = weekday();
  scheduler->addTask(new (std::nothrow) hCallbackCommand(false, t, daily, &hook_ntp_update));

  // Enabling MQTT client support

  espClient.setTimeout(200); // DNS and TCP connection timeout, milliseconds.
  client.setServer(_MQTT_SERVER, _MQTT_SERVER_PORT);
  client.setCallback(mqttCallback);
  hook_mqtt_reconnect();
  if (HEATING_ENABLE_DOMESTIC_PLAN) createPlanForDomesticWaterPump(heatPumpController);
}

void hook_mqtt_reconnect()
{
  config->setMQTTStatus(mqttReconnect.poll(client, static_cast<uint32_t>(millis()),
    !internalWIFIMode && WiFi.status() == WL_CONNECTED,
    _MQTT_CLIENT_ID, _MQTT_LOGIN, _MQTT_PASSWORD, _MQTT_COMMANDS_TOPIC));
}

void loop()
{
  // Expire and cancel requests before scheduler execution or contact renewal.
  // This check runs on every iteration, even without packets or an NTP clock.
  heatPumpController->checkThermostatTimeouts();

  if (heatingTickDue(static_cast<uint32_t>(millis()), timeMillis, 1000))
  {
    hook_sanity_check(); // Monotonic safety also works without a valid NTP clock.
    scheduler->executeTasks();
    hdisplay->printMainScreen();
    hdisplay->printStatusBar(_BLANK_LINE);
    hdisplay->renderScreen();

  }
  if (timeStatus() == timeNotSet && !internalWIFIMode &&
      heatingTickDue(static_cast<uint32_t>(millis()), lastNtpRetry, 60000)) hook_ntp_update();
  udpMessenger.listen();

  //Incoming commands router

  if (udpMessenger.checkNewCommand())

  {
    tClientCommand temp = udpMessenger.getCurrentCommand();

    bool control = strcmp(temp.cmd, "ON") == 0 || strcmp(temp.cmd, "OFF") == 0;
    bool heartbeat = strcmp(temp.cmd, "HEARTBEAT") == 0;
    bool assigned = false;
    if (control || heartbeat) {
      assigned = config->recordContact(temp.ID, temp.serial, strcmp(temp.cmd, "ON") == 0);
      if (!assigned) {
        udpMessenger.sendBackMessage(false, config->getPumpStatus(temp.ID), outputStateName(temp.ID));
      }
      if (assigned && heartbeat) {
        udpMessenger.sendBackMessage(true, config->getPumpStatus(temp.ID), outputStateName(temp.ID));
      }
    }

    if (assigned && strcmp(temp.cmd, "ON") == 0)
    {
      char tm[20];
      snprintf(tm, sizeof(tm), "Pump %d ON", temp.ID);
      hdisplay->printMainScreen();
      hdisplay->printStatusBar(tm);
      hdisplay->renderScreen();
      bool accepted = heatPumpController->turnOnHeatPumpReq(temp.ID, temp.actualTEMP, temp.targetTEMP);
      udpMessenger.sendBackMessage(accepted, config->getPumpStatus(temp.ID), outputStateName(temp.ID));
      if (config->getMQTTStatus())
      {
        char subtopic[THERMOSTAT_TOPIC_CAPACITY];
        if (formatThermostatTopic(subtopic, sizeof(subtopic), temp.ID)) {
          Serial.println(subtopic);
        }
        // client.publish(#_MQTT_SENSORS_TOPIC "/" , hr);
      }
    }

    if (assigned && strcmp(temp.cmd, "OFF") == 0)
    {
      char tm[20];
      snprintf(tm, sizeof(tm), "Pump %d OFF", temp.ID);
      hdisplay->printMainScreen();
      hdisplay->printStatusBar(tm);
      hdisplay->renderScreen();
      bool accepted = heatPumpController->turnOffHeatPumpReq(temp.ID, temp.actualTEMP, temp.targetTEMP);
      udpMessenger.sendBackMessage(accepted, config->getPumpStatus(temp.ID), outputStateName(temp.ID));
    }

    if (strcmp(temp.cmd, "SHOWSERVER") == 0)
    {
      char tm[20];
      snprintf(tm, sizeof(tm), "Discovery C%d", temp.ID);
      hdisplay->printMainScreen();
      hdisplay->printStatusBar(tm);
      hdisplay->renderScreen();
      hook_discover_devices();
    }

    if (strcmp(temp.cmd, "SHOWSTATUS") == 0)
    {
      udpMessenger.sendBackMessage(temp.ID >= 1 && temp.ID <= _DOMESTIC_WATER_PUMP, config->getPumpStatus(temp.ID), outputStateName(temp.ID));
    }
  }

  // A single bounded reconnect attempt is serviced independently of NTP.
  client.loop();
  hook_mqtt_reconnect();
}
