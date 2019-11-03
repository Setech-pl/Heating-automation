#pragma once
#define _CPPWIN 1
#include <iostream>
#include <stdint.h>
#include <string.h>

int month();
int weekday();
int hour();
int minute();
int day();
void delay(int amount);


enum WIFI_MODES {
	WIFI_AP_STA = 1,
	WIFI_STA = 2
};

enum wifiStatus {
	WL_CONNECTED = 0
};

class IPAdress {
public:
	uint8_t operator[](int index) const {
		 return 0;
	}


private:
	 uint8_t address = 0;
};

class LiquidCrystal_I2C {
public:

	void clear();
	void setCursor(int x, int y);
	void print(const char msg[]);
	void noBacklight();
	void backlight();

};


class WiFiUDP {
public:

	void begin( const char sid[14],  const char pass[26]);
	void disconnect();
	wifiStatus status();
	int RSSI();
	std::string SSID();
	IPAdress localIP();
	bool softAP(const char sid[14], const char pass[26], int channel, bool aut, int subchannel);
	bool mode(WIFI_MODES changeMode);

};

class NTPClient {
public:
	NTPClient(WiFiUDP udp, const char* poolServerName, long timeOffset, unsigned long updateInterval);
	void begin();
	bool update();
	uint32_t getEpochTime();
};



void setTime(uint32_t vtime);

int random(int min, int max = 0);

class MQTTClient {
public:
	void begin(const char* MQTT_Server, const char* WIFIc);
	bool connect();
};