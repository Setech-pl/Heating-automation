
#include <LiquidCrystal_I2C.h>
#include "heating_config.h"
#include <TimeLib.h>
#include "utils.h"
#include <Wire.h>
#include "scheduler.h"
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "screen.h"
#include <ArduinoJson.h>
#include "UDPMessengerService.h"

LiquidCrystal_I2C lcd(0x27, 20, 4);

hScheduler *scheduler = new hScheduler();
hConfigurator *config = new hConfigurator();
hPumpsController *heatPumpController = new hPumpsController(scheduler, config);
hScreen *hdisplay = new hScreen(&lcd, config);
UDPMessengerService udpMessenger(3636);
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

void setup()
{
  Serial.begin(115200);
  Serial.println("Entering setup mode");
  delay(300);
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
  while (!wynik && counter < 5)
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
}

void loop()
{

  if (timeMillis + 1000 < millis())
  {
    scheduler->executeTasks();
    config->tickMinutes();
  }
  if (timeMillis + 2000 < millis())
  {

    hdisplay->printMainScreen();
    hdisplay->printStatusBar(_BLANK_LINE);
    hdisplay->renderScreen();
    timeMillis = millis();
  }
  udpMessenger.listen();

  //Incoming commands router

  if (udpMessenger.checkNewCommand())
  {
    Serial.println("command incoming");
    tClientCommand temp = udpMessenger.getCurrentCommand();
    if (strcmp(temp.cmd, "ON") == 0)
    {
      //  if (temp.ID >= 0 && temp.ID < _MAX_HEATING_PUMPS_NO)
      //  {
      char tm[20];
      sprintf(tm, "Pump %d ON", temp.ID + 1);
      hdisplay->printStatusBar(tm);
      hdisplay->renderScreen();
      // send back actual status of running pump
      udpMessenger.sendBackMessage(heatPumpController->turnOnHeatPumpReq(temp.ID, temp.actualTEMP, temp.targetTEMP), config->getPumpStatus(temp.ID));
      timeMillis = millis();
      //   }
    }
    if (strcmp(temp.cmd, "OFF") == 0)
    {
      if (temp.ID >= 0 && temp.ID < _MAX_HEATING_PUMPS_NO)
      {
        char tm[20];
        sprintf(tm, "Pump %d OFF", temp.ID + 1);
        hdisplay->printStatusBar(tm);
        hdisplay->renderScreen();
        udpMessenger.sendBackMessage(heatPumpController->turnOffHeatPumpReq(temp.ID, temp.actualTEMP, temp.targetTEMP), config->getPumpStatus(temp.ID));
        timeMillis = millis();
      }
    }
    if (strcmp(temp.cmd, "SHOWSERVER") == 0)
    {
      char tm[20];
      sprintf(tm, "Registering C%d", temp.ID + 1);
      hdisplay->printStatusBar(tm);
      hdisplay->renderScreen();
      hook_discover_devices();
    }
  }

  // MQTT client section
  if (false)
  {
    //incoming external command - set temp to Thermo Client ID
    float tablicaTemp[_MAX_HEATING_PUMPS_NO];
    tablicaTemp[0] = 33.2;
 //  udpMessenger.setTempFromMQTT(tablicaTemp);
  }
}
