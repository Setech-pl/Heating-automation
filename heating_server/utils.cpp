
#define _CRT_SECURE_NO_WARNINGS
#include "utils.h"
#include "heating_config.h"
#include  <string.h>
#ifndef _CPPWIN
  #include <ESP8266WiFi.h>
  #include <NTPClient.h>
  #include <WiFiUdp.h>
  #include <Time.h>
  #include <TimeLib.h>
#else
  #include "arduino_stub.h"
#endif // !_CPPWIN

/*
Utility commands
*/


bool ntp_update::execute(){
     WiFiUDP ntpUDP;    
     NTPClient timeClient(ntpUDP, "0.pl.pool.ntp.org", 3600, 60000);
		
	  timeClient.begin();
   
    if (timeClient.update()){
      strcpy(this->result,"NTP update OK     ");
      setTime(timeClient.getEpochTime());
      return true;
    }else {
      strcpy(this->result,"NPT update ERROR  ");
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
