#define _CPPWINa 1

#include "heating_config.h"
#ifdef _CPPWIN
#include "arduino_stub.h"
#endif
#ifndef _CPPWIN
#include <Time.h>
#include <TimeLib.h>
#endif



void hConfigurator::setPumpStatusOn(int pumpNumber, float actualTemp, float setTemp)
{
	if (pumpNumber >= 0 && pumpNumber <= _DOMESTIC_WATER_PUMP) {
		_pumps[pumpNumber].running = true;
		_pumps[pumpNumber].start_minute = minute();
		_pumps[pumpNumber].start_hour = hour();
		_pumps[pumpNumber].pumpNumber = pumpNumber;
		_pumps[pumpNumber].start_day = weekday();
		_pumps[pumpNumber].actualTemp = actualTemp;
		_pumps[pumpNumber].setTemp = setTemp;
		saveHistory(_pumps[pumpNumber]);
	}
}

void hConfigurator::setPumpStatusOff(int pumpNumber)
{
	if (pumpNumber >= 0 && pumpNumber <= _DOMESTIC_WATER_PUMP) {
		_pumps[pumpNumber].running = false;
		saveHistory(_pumps[pumpNumber]);
		_pumps[pumpNumber].actualTemp = 0;
		_pumps[pumpNumber].minuts = 0;
		_pumps[pumpNumber].setTemp = 0;
		_pumps[pumpNumber].start_day = 0;
		_pumps[pumpNumber].start_hour = 0;
		_pumps[pumpNumber].start_minute = 0;
		_pumps[pumpNumber].actualMinute = 0;
	}
}

bool hConfigurator::getPumpStatus(int pumpNumber)
{
	bool result = false;
	for (int i = 0; i < _DOMESTIC_WATER_PUMP; i++) {
		if (_pumps[i].running && _pumps[i].pumpNumber==pumpNumber) {
			result = true;
		}
	}
	return result;
}

int hConfigurator::getPumpRunningMinuts(int pumpNumber)
{
	return _pumps[pumpNumber].minuts;
}

bool hConfigurator::heatPumpsRunning()
{
	bool result = false;
	for (int i = 0; i < _DOMESTIC_WATER_PUMP; i++) {
		if (_pumps[i].running) {
			result = true;
		}
	}
	return result;
};


bool hConfigurator::circulationPumpsRunning()
{
	return _pumps[4].running;
};

int hConfigurator::lastOnOffPump(int pumpNumber, int lastMinuts)
{
	int  result = 0;
	int actualMinute = minute();
	int actualHour = hour();
	int actualDay = weekday();
	if (lastMinuts > 59) lastMinuts = 59;
	if (lastMinuts < 0) lastMinuts = 0;
	if (actualHour < 0 || actualHour>23) actualHour = 0;
	int countedActualMinutes = actualHour * 60 + actualMinute;
	for (int i = 0; i < (sizeof(_pumpsHistory) / sizeof(pumpStatus)); i++) {
		if (_pumpsHistory[i].pumpNumber == pumpNumber) {
			int countedStartMinutes = _pumpsHistory[i].start_hour * 60 + _pumpsHistory[i].start_minute;
			if (_pumpsHistory[i].start_day != actualDay)  countedStartMinutes = countedStartMinutes - 1440;
			if (countedStartMinutes + lastMinuts >=countedActualMinutes ) {
				//I found turn off
				result++; 
			}
		}
	}
	return result;
};


void hConfigurator::tickMinutes()
{
	for (int i = 0; i <= _DOMESTIC_WATER_PUMP; i++) {
		if (&_pumps[i] == nullptr) continue;
		if (_pumps[i].running && _pumps[i].actualMinute != minute()) {
			_pumps[i].minuts++;
			_pumps[i].actualMinute = minute();
		}
	}
}

int hConfigurator::getPercentage(int pumpNumber)
{
	int all = 0;
	float result = 0;
	// get only stats from heat pumps 0-3 numbes
	if (pumpNumber >= 0 && pumpNumber< 4) {

		for (int i = 0; i < 4; i++) {
			all += _pumps[i].minuts;
		}
		if (all > 0) {
			result = (100*(_pumps[pumpNumber].minuts / (float)all));
		}
		else
			result = 0;
	}
	else
		result = 0;
	return (int)result;
}
hConfigurator::~hConfigurator()
{
};


bool hConfigurator::registerClient(thermoClientStat client)
{
	return false;
}



hConfigurator::hConfigurator(){
	for (int i = 0; i < 5; i++) {
		_pumps[i].running = false;
	}
	for (int i = 0; i < (sizeof(_pumpsHistory)/sizeof(pumpStatus)); i++) {
		_pumpsHistory[i].pumpNumber = -1;

	}
  
}

void hConfigurator::saveHistory(pumpStatus oldStatus)
{

	if (_pumpsHistoryC >= sizeof(_pumpsHistory)/sizeof(pumpStatus)) {
		_pumpsHistoryC = 0;
	}
	_pumpsHistory[_pumpsHistoryC] = oldStatus;
	_pumpsHistoryC++;
}
;
