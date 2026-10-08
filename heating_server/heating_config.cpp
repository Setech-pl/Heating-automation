#define _CPPWINa 1
#include "heating_config.h"
#include <Arduino.h>
#include <limits.h>
#ifdef _CPPWIN
#include "arduino_stub.h"
#endif
#ifndef _CPPWIN
#include <TimeLib.h>
#endif

void hConfigurator::setPumpStatusOn(int pumpNumber, float actualTemp, float setTemp)
{
	if (getPumpStatus(pumpNumber)) return;
	if (pumpNumber > 0 && pumpNumber <= _DOMESTIC_WATER_PUMP)
	{
		_pumps[pumpNumber].running = true;
		_started[pumpNumber] = uptime();
		_pumps[pumpNumber].minuts = 0;
		_pumps[pumpNumber].actualMinute = minute();
		_pumps[pumpNumber].start_minute = minute();
		_pumps[pumpNumber].start_hour = hour();
		_pumps[pumpNumber].pumpNumber = pumpNumber;
		_pumps[pumpNumber].start_day = weekday();
		_pumps[pumpNumber].actualTemp = actualTemp;
		_pumps[pumpNumber].setTemp = setTemp;
		if (pumpNumber < _DOMESTIC_WATER_PUMP)
			saveHistory(pumpNumber, true, _started[pumpNumber]);
	}
}

void hConfigurator::setPumpStatusOff(int pumpNumber)
{
	if (!getPumpStatus(pumpNumber)) return;
	if (pumpNumber > 0 && pumpNumber <= _DOMESTIC_WATER_PUMP)
	{
		_pumps[pumpNumber].running = false;
		_stopped[pumpNumber] = uptime();
		_hasStopped[pumpNumber] = true;
		if (pumpNumber < _DOMESTIC_WATER_PUMP)
			saveHistory(pumpNumber, false, _stopped[pumpNumber]);
		_pumps[pumpNumber].actualTemp = 0;
		_pumps[pumpNumber].minuts = 0;
		_pumps[pumpNumber].setTemp = 0;
		_pumps[pumpNumber].start_day = 0;
		_pumps[pumpNumber].start_hour = 0;
		_pumps[pumpNumber].start_minute = 0;
		_pumps[pumpNumber].actualMinute = 0;
	}
}

bool hConfigurator::getMQTTStatus()
{
	return this->mqttStatus;
}

void hConfigurator::setMQTTStatus(bool mqttStatus)
{
	this->mqttStatus = mqttStatus;
}

bool hConfigurator::getPumpStatus(int pumpNumber)
{
	bool result = false;
	if (pumpNumber > 0 && pumpNumber <= _DOMESTIC_WATER_PUMP)
	{
		result = _pumps[pumpNumber].running;
	}
	return result;
}

int hConfigurator::getPumpRunningMinuts(int pumpNumber)
{
	return pumpNumber > 0 && pumpNumber <= _DOMESTIC_WATER_PUMP ? _pumps[pumpNumber].minuts : 0;
}

bool hConfigurator::heatPumpsRunning()
{
	bool result = false;
	for (int i = 1; i <= _MAX_HEATING_PUMPS_NO; i++)
	{
		if (_pumps[i].running)
		{
			result = true;
		}
	}
	return result;
};

bool hConfigurator::domesticWaterPumpIsRunning()
{
	return _pumps[_DOMESTIC_WATER_PUMP].running;
};

uint64_t hConfigurator::uptime()
{
	uint32_t current = static_cast<uint32_t>(millis());
	_uptime += static_cast<uint32_t>(current - _lastMillis);
	_lastMillis = current;
	return _uptime;
}

bool hConfigurator::switchPump(int pumpNumber, bool running)
{
	return _outputs != nullptr && _outputs->set(pumpNumber, running);
}

hRelayOutputs::State hConfigurator::outputState(int pumpNumber) const
{
	return _outputs != nullptr ? _outputs->state(pumpNumber) : hRelayOutputs::unknown;
}

bool hConfigurator::canRestartPump(int pumpNumber)
{
	if (pumpNumber < 1 || pumpNumber > _DOMESTIC_WATER_PUMP) return false;
	return _DISABLE_MAX_ONOFF_VALIDATION || !_hasStopped[pumpNumber] ||
		uptime() - _stopped[pumpNumber] >= uint64_t(_MIN_MINUTS_FROM_LAST_START) * 60000;
}

bool hConfigurator::canStopPump(int pumpNumber)
{
	if (pumpNumber < 1 || pumpNumber > _DOMESTIC_WATER_PUMP) return false;
	return _DISABLE_MAX_ONOFF_VALIDATION ||
		uptime() - _started[pumpNumber] >= uint64_t(_MIN_MINUTS_FROM_LAST_START) * 60000;
}

int hConfigurator::lastOnOffPump(int pumpNumber, int lastMinuts)
{
	if (pumpNumber < 1 || pumpNumber > _DOMESTIC_WATER_PUMP || lastMinuts <= 0) return 0;
	if (lastMinuts > 59) lastMinuts = 59;
	uint64_t current = uptime();
	int result = 0;
	// Count ON transitions in the half-open monotonic window, never old weekdays.
	for (const Event &event : _pumpsHistory)
		if (event.pumpNumber == pumpNumber && event.on &&
			current - event.milliseconds < uint64_t(lastMinuts) * 60000) ++result;
	return result;
}

void hConfigurator::tickMinutes()
{
	uint64_t current = uptime();
	for (int i = 1; i <= _DOMESTIC_WATER_PUMP; ++i)
		if (_pumps[i].running) {
			uint64_t minutes = (current - _started[i]) / 60000;
			_pumps[i].minuts = minutes > INT_MAX ? INT_MAX : static_cast<int>(minutes);
		}
}

int hConfigurator::getPercentage(int pumpNumber)
{
	if (pumpNumber < 1 || pumpNumber > _MAX_HEATING_PUMPS_NO) return 0;
	int64_t all = 0;
	for (int i = 1; i <= _MAX_HEATING_PUMPS_NO; ++i) all += _pumps[i].minuts;
	return all > 0 ? static_cast<int>(int64_t(100) * _pumps[pumpNumber].minuts / all) : 0;
}

hConfigurator::~hConfigurator() = default;

bool hConfigurator::registerClient(thermoClientStat /*client*/)
{
	return false; // No client registration protocol has been supplied.
}

hConfigurator::hConfigurator(hRelayOutputs *outputs)
	: _outputs(outputs), _lastMillis(static_cast<uint32_t>(millis()))
{}

void hConfigurator::saveHistory(int pumpNumber, bool on, uint64_t timestamp)
{
	_pumpsHistory[_pumpsHistoryC] = {timestamp, pumpNumber, on};
	_pumpsHistoryC = (_pumpsHistoryC + 1) % 256;
}
