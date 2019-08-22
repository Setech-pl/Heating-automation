#define _CPPWIN 1
#pragma once
#include "arduino_stub.h"
#include "pch.h"
#include <iostream>
#include <cstdlib>
#include <string>

cSerial Serial;

int random(int min, int max ) {
	return rand() % max + min;

}

int weekday() {
	return rand()%6;
	//return 6;
}


int hour() {
	//return rand()%24;
	return  23;
}

int minute() {
	return rand()%60;
	//return 58;
}

int day()
{
	return 0;
}


void delay(int amount)
{
}

void setTime(uint32_t vtime)
{
	
		std::cout << "Setting Time:\n";
}

void LiquidCrystal_I2C::clear()
{
}

void LiquidCrystal_I2C::setCursor(int x, int y)
{
}

void LiquidCrystal_I2C::print(const char  msg[])
{
	std::cout << "\n";
	std::cout << msg;
}

void LiquidCrystal_I2C::noBacklight()
{
	std::cout << "Disabling lcd backlight";
}

void LiquidCrystal_I2C::backlight()
{
	std::cout << "enabling backlight";
}



NTPClient::NTPClient(WiFiUDP udp, const char* poolServerName, long timeOffset, unsigned long updateInterval)
{
}

void NTPClient::begin()
{
}

bool NTPClient::update()
{
	return true;
}

uint32_t NTPClient::getEpochTime()
{
	return 1547110618;
}


void WiFiUDP::begin( const char sid[14],  const char pass[26])
{
}

void WiFiUDP::disconnect()
{
}

wifiStatus WiFiUDP::status()
{
	return WL_CONNECTED;
}

int WiFiUDP::RSSI()
{
	return 50;
}

std::string WiFiUDP::SSID()
{
	return "TELETEL";
}

IPAdress WiFiUDP::localIP()
{
	return IPAdress();
}

bool WiFiUDP::softAP(const char sid[14], const char pass[26])
{
	std::cout << "enabling software AP sid";
	std::cout << sid;
	std::cout << pass;
	return true;
}

bool WiFiUDP::mode(WIFI_MODES changeMode)
{
	std::cout << "Wifi Mode";
	std::cout << changeMode;
	return true;
}




void cSerial::println(const char * msg)
{
	std::cout << "\n";
	std::cout << msg;
}
