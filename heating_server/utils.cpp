
#define _CRT_SECURE_NO_WARNINGS
#include "utils.h"
#include "heating_config.h"
#include "screen.h"
#include <stdio.h>
#include  <string.h>
#ifndef _CPPWIN
  #include <ESP8266WiFi.h>
  #include <NTPClient.h>
  #include <WiFiUdp.h>
  #include <TimeLib.h>
#else
  #include "arduino_stub.h"
#endif // !_CPPWIN

/*
Utility commands
*/

bool formatThermostatTopic(char *topic, size_t capacity, int id)
{
  if (topic == NULL || capacity == 0) return false;
  topic[0] = '\0';
  // Match the existing controller's heating-pump domain; no protocol migration.
  if (id < 1 || id > _MAX_HEATING_PUMPS_NO) return false;
  int length = snprintf(topic, capacity, "%s/thermostat%d", _MQTT_SENSORS_TOPIC, id);
  if (length < 0 || static_cast<size_t>(length) >= capacity) {
    topic[0] = '\0'; // Never expose a truncated topic as usable.
    return false;
  }
  return true;
}

ntp_update::StartupResult ntp_update::synchronizeOnStartup(bool internalWiFiMode,
                                                         hScreen &display)
{
  StartupResult state = skipped;
  strcpy(result, "NTP skipped (AP)");
  if (!internalWiFiMode) {
    state = failed;
    for (int attempt = 0; attempt < 5; ++attempt) {
      char progress[21];
      snprintf(progress, sizeof(progress), "Updating NTP(%d)  ", attempt);
      display.printStatusBar(progress);
      display.renderScreen();
      bool success = execute();
      delay(300);
      if (success) {
        state = synchronized;
        break;
      }
    }
  }
  display.printStatusBar(result);
  display.renderScreen();
  return state;
}

bool ntp_update::execute(){
     WiFiUDP ntpUDP;    
     NTPClient timeClient(ntpUDP, "0.pl.pool.ntp.org", 3600, 60000);
		
	  timeClient.begin();
   
    if (timeClient.update()){
      strcpy(this->result,"NTP update OK     ");
      setTime(timeClient.getEpochTime());
      return true;
    }else {
      strcpy(this->result,"NTP update ERROR  ");
      return false;
    }

  };

bool connect_external_wifi::execute(){
#ifdef _CPPWIN
	WiFiUDP WiFi;
#endif

  int counter=0;  
  WiFi.disconnect();  
  WiFi.mode(WIFI_STA);
  WiFi.begin(_EXTERNAL_WIFI_SID,_EXTERNAL_WIFI_PASS);
  while (WiFi.status() != WL_CONNECTED and counter<10) {
    counter++;
    delay(500);
  }
  if (WiFi.status() == WL_CONNECTED) {    
    strcpy(this->result,"Ext Wifi connected");
    return true;
  }  else {  
    strcpy(this->result,"Ext Wifi error    ");    
    return false;
  }
};

bool enable_internal_wifi::execute()
{
#ifdef _CPPWIN
	WiFiUDP WiFi;
#endif
	int counter = 0;
	bool wynik = false;
	WiFi.disconnect();
	WiFi.mode(WIFI_AP_STA);
	while (!wynik and counter < 2) {
		counter++;
		wynik = WiFi.softAP(_INTERNAL_WIFI_SID, _INTERNAL_WIFI_PASS,6,false,10);
		delay(500);
	}
	if (wynik) {
		strcpy(this->result, "Int Wifi connected");
	}
	else {
		strcpy(this->result, "Int Wifi error    ");
	}
	return wynik;
}
