#define _CPPWINa 1
#include "scheduler.h"
#include <time.h>
#include <new>
#include <math.h>
#include "createDailyPlan.h"
#ifndef _CPPWIN
#include <TimeLib.h>
#endif // !_CPPWIN
#ifdef _CPPWIN
#include "arduino_stub.h"
#endif
#ifndef _CPPWIN
#include <ESP8266WiFi.h>
#endif

/*
payload (0 reserved, 1-4 heating pumps, 5 circulation pump):
1 -  turn on pump (1)
11 - turn off pump (1)
2 - turn on pump (2)
12 - turn off pump (2)
3 - turn on pump (3)
13 - turn off pump (3)
...

pumps aware payloads

123 Update time from NTP
*/

int hScheduler::addTask(hCommand *command)
{
	if (command == nullptr)
		return invalidTask;
	// Protect an existing ownership relationship from accidental resubmission.
	if (command == _executing)
		return duplicate;
	for (int i = 0; i < commandCounter; ++i)
		if (commands[i] == command)
			return duplicate;
	if (!validSchedule(*command)) {
		delete command;
		return invalidTask;
	}
	if (findDuplicate(command))
	{
		delete command;
		return duplicate;
	}
	int slot = getFreeSlot();
	if (slot < 0)
	{
		delete command;
		return full;
	}
	commands[slot] = command;
	if (timeStatus() != timeNotSet) {
		time_t current = now();
		initializeSchedule(*command, current);
	}
	return slot;
}

bool hScheduler::addExecuteTask(hCommand *command)
{
	int commandId = addTask(command);
	return commandId >= 0 && _executing == nullptr &&
		checkSchedule(commandId) && executeTask(commandId);
}

void hScheduler::removeAllCommands()
{
	for (int i = 0; i < commandCounter; ++i)
		removeCommand(i);
}

bool hScheduler::executeTask(int commandId)
{
	if (_executing != nullptr || getTask(commandId) == nullptr)
		return false;
	_executing = commands[commandId];
	_executingRemoved = false;
	bool disposable = _executing->disposable;
	bool result = _executing->execute();
	if (disposable && !_executingRemoved)
		removeCommand(commandId);
	hCommand *finished = _executing;
	bool removed = _executingRemoved;
	_executing = nullptr;
	_executingRemoved = false;
	if (removed)
		delete finished;
	return result;
}

bool hScheduler::executeTasks(int commandId)
{
	if (_executing != nullptr || commandId < 0 || commandId >= commandCounter)
		return false;
	bool attempted = false;
	bool succeeded = true;
	for (int i = commandId; i < commandCounter; ++i)
	{
		if (commands[i] == nullptr || !checkSchedule(i))
			continue;
		attempted = true;
		bool result = executeTask(i);
		succeeded = result && succeeded;
	}
	return attempted && succeeded;
}

void hScheduler::removeCommand(int cNumber)
{
	hCommand *command = getTask(cNumber);
	if (command == nullptr)
		return;
	commands[cNumber] = nullptr;
	if (command == _executing)
		_executingRemoved = true;
	else
		delete command;
}

void hScheduler::removeCommands(int payload)
{
	for (int i = 0; i < commandCounter; ++i)
		if (commands[i] != nullptr && commands[i]->payload == payload)
			removeCommand(i);
}

hCommand *hScheduler::getTask(int taskNumber)
{
	return taskNumber >= 0 && taskNumber < commandCounter ? commands[taskNumber] : nullptr;
}

int hScheduler::maxTaskCount()
{
	return commandCounter;
}

int hScheduler::activeTaskCount()
{
	int result = 0;
	for (int i = 0; i < commandCounter; ++i)
		if (commands[i] != nullptr)
			++result;
	return result;
}

hScheduler::hScheduler() = default;

hScheduler::~hScheduler()
{
	removeAllCommands();
}

int hScheduler::getFreeSlot()
{
	for (int i = 0; i < commandCounter; ++i)
		if (commands[i] == nullptr)
			return i;
	return full;
}

bool hScheduler::validSchedule(const hCommand &command) const
{
	const tm &time = command.scheduleTime;
	return command.scheduleType >= daily && command.scheduleType <= monthly &&
		time.tm_hour >= 0 && time.tm_hour <= 23 && time.tm_min >= 0 && time.tm_min <= 59 &&
		time.tm_sec >= 0 && time.tm_sec <= 59 &&
		(command.scheduleType != weekly || (time.tm_wday >= 1 && time.tm_wday <= 7)) &&
		(command.scheduleType != monthly || (time.tm_mday >= 1 && time.tm_mday <= 31));
}

time_t hScheduler::nextOccurrence(const hCommand &command, time_t after) const
{
	const tm &time = command.scheduleTime;
	time_t midnight = (after / SECS_PER_DAY) * SECS_PER_DAY;
	time_t target = midnight + time.tm_hour * SECS_PER_HOUR + time.tm_min * SECS_PER_MIN + time.tm_sec;
	switch (command.scheduleType) {
	case minutly:
		target = (after / SECS_PER_MIN) * SECS_PER_MIN + time.tm_sec;
		return target < after ? target + SECS_PER_MIN : target;
	case hourly:
		target = (after / SECS_PER_HOUR) * SECS_PER_HOUR + time.tm_min * SECS_PER_MIN + time.tm_sec;
		return target < after ? target + SECS_PER_HOUR : target;
	case daily:
		return target < after ? target + SECS_PER_DAY : target;
	case weekly:
		target += ((time.tm_wday - weekday(after) + 7) % 7) * SECS_PER_DAY;
		return target < after ? target + SECS_PER_WEEK : target;
	case monthly: {
		tmElements_t parts;
		breakTime(after, parts);
		parts.Hour = time.tm_hour;
		parts.Minute = time.tm_min;
		parts.Second = time.tm_sec;
		parts.Day = time.tm_mday;
		// Skip months without the requested day; never normalize 31 February.
		for (int monthCount = 0; monthCount < 24; ++monthCount) {
			time_t candidate = makeTime(parts);
			tmElements_t actual;
			breakTime(candidate, actual);
			if (actual.Day == parts.Day && actual.Month == parts.Month && candidate >= after)
				return candidate;
			if (++parts.Month > 12) { parts.Month = 1; ++parts.Year; }
		}
		return 0;
	}
	}
	return 0;
}

void hScheduler::updateSchedule(hCommand &command, time_t next)
{
	command._nextDue = next;
	command.scheduleTime.tm_wday = weekday(next); // TimeLib: Sunday=1 ... Saturday=7.
	command.scheduleTime.tm_hour = hour(next);
	command.scheduleTime.tm_min = minute(next);
	command.scheduleTime.tm_mon = month(next); // Existing API uses TimeLib months 1-12.
}

void hScheduler::initializeSchedule(hCommand &command, time_t current)
{
	command._nextDue = nextOccurrence(command, current - current % 60);
	// Preserve a specifically requested future first hour/minute. Once started,
	// recurrence is computed from the current calendar rather than stale fields.
	if (command.scheduleType == hourly && command.scheduleTime.tm_hour > hour(current))
		command._nextDue = (current / SECS_PER_DAY) * SECS_PER_DAY +
			command.scheduleTime.tm_hour * SECS_PER_HOUR + command.scheduleTime.tm_min * SECS_PER_MIN + command.scheduleTime.tm_sec;
	if (command.scheduleType == minutly && command.scheduleTime.tm_min > minute(current))
		command._nextDue = (current / SECS_PER_HOUR) * SECS_PER_HOUR +
			command.scheduleTime.tm_min * SECS_PER_MIN + command.scheduleTime.tm_sec;
	command._scheduleInitialized = true;
}

bool hScheduler::checkSchedule(int cNumber)
{
	hCommand &command = *commands[cNumber];
	if (timeStatus() == timeNotSet) {
		// Immediate relay commands remain usable without NTP; calendar tasks wait.
		return !command.requiresClock() && command.scheduleTime.tm_hour == hour() &&
			command.scheduleTime.tm_min == minute();
	}
	time_t current = now();
	if (!command._scheduleInitialized) {
		initializeSchedule(command, current);
	}
	if (command._nextDue == 0 || current < command._nextDue) return false;
	// Missed periods are skipped, not replayed. Execute at most once in the
	// current scheduled minute, then advance directly to a future occurrence.
	time_t occurrence = nextOccurrence(command, current - current % 60);
	bool due = occurrence <= current && current - occurrence < 60;
	updateSchedule(command, nextOccurrence(command, current + 1));
	return due;
}

bool hScheduler::findDuplicate(hCommand *command)
{
	for (int i = 0; i < commandCounter; ++i)
		if (commands[i] != nullptr && commands[i]->isDuplicateOf(*command))
			return true;
	return false;
}

bool hCommand::isDuplicateOf(const hCommand &other) const
{
	return commandType() != nullptr && commandType() == other.commandType() &&
		commandContext() == other.commandContext() &&
		_callbackFunction == other._callbackFunction && payload == other.payload &&
		disposable == other.disposable && scheduleType == other.scheduleType &&
		_identity.hour == other._identity.hour &&
		_identity.minute == other._identity.minute &&
		_identity.second == other._identity.second &&
		_identity.weekday == other._identity.weekday &&
		_identity.day == other._identity.day &&
		_identity.month == other._identity.month &&
		_identity.year == other._identity.year;
}

hCommand::hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload)
	: disposable(disposable), scheduleTime(scheduleTime), scheduleType(scheduleType), payload(payload),
	  _identity{scheduleTime.tm_hour, scheduleTime.tm_min, scheduleTime.tm_sec,
	            scheduleTime.tm_wday, scheduleTime.tm_mday, scheduleTime.tm_mon, scheduleTime.tm_year}
{}

hCommand::hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, void (*callbackFunction)())
	: hCommand(disposable, scheduleTime, scheduleType, 0)
{
	_callbackFunction = callbackFunction;
}

hCommand::hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload, hConfigurator *config)
	: hCommand(disposable, scheduleTime, scheduleType, payload)
{
	_config = config;
}

bool hPumpCommand::execute()
{
	if (_config == nullptr) return false;
	bool running = payload >= 1 && payload <= _MAX_HEATING_PUMPS_NO;
	int pump = running ? payload : payload - 10;
	if (pump < 1 || pump > _MAX_HEATING_PUMPS_NO) return false;
	if (running && (!_config->heatingAllowed(pump) ||
		_generation != _config->requestGeneration(pump) || !_config->canRestartPump(pump))) return false;
	return _config->switchPump(pump, running);
}

hPumpsController::hPumpsController(hScheduler *scheduler, hConfigurator *config)
{
	_scheduler = scheduler;
	_config = config;
}

bool hPumpsController::createDailyPlan(bool holiday)
{
	return !holiday && createPlanForDomesticWaterPump(this);
}

void hPumpsController::removeDailyPlan(int pumpNumber)
{
	_scheduler->removeCommands(pumpNumber);
	_scheduler->removeCommands(pumpNumber + 10);
}

bool hPumpsController::turnOnHeatPumpReq(int pumpNumber, float actualTemp, float setTemp)
{
	if (!isfinite(actualTemp) || !isfinite(setTemp) || pumpNumber < 1 || pumpNumber > _MAX_HEATING_PUMPS_NO)
		return false;
	bool canTurnOn = _config->heatingAllowed(pumpNumber) && _config->canRestartPump(pumpNumber);
	float tempModifier = 0;
	int currentHour = hour();
	if (timeStatus() != timeNotSet && currentHour > 10 && currentHour < 14)
	{
		tempModifier = _MAX_DAY_OVERHEATING;
	}
	if (timeStatus() != timeNotSet && (currentHour > 22 || currentHour < 6))
	{
		tempModifier = _MAX_NIGHT_COOLING;
	}
	//check turn on off validation for example from config.history

	//negative validations
	if (pumpNumber < 1 || pumpNumber > _MAX_HEATING_PUMPS_NO)
		canTurnOn = false;
	if (actualTemp > setTemp + _MAX_HEATING_TEMP_DELTA + tempModifier)
		canTurnOn = false;
	if (actualTemp > _MAX_HEATING_INTERIOR_TEMP + tempModifier)
		canTurnOn = false;
	if (_config->getPumpStatus(pumpNumber))
		canTurnOn = false;
	if (canTurnOn)
	{
		tm tTime = {};
		tTime.tm_hour = hour();
		tTime.tm_min = minute();
		tTime.tm_mday = day();
		tTime.tm_wday = weekday();
		if (!_scheduler->addExecuteTask(new (std::nothrow) hPumpCommand(true, tTime, hourly, pumpNumber, _config)))
			return false;
		//update config
		_config->setPumpStatusOn(pumpNumber, actualTemp, setTemp);
	}
	return canTurnOn;
}

bool hPumpsController::turnOffHeatPumpReq(int pumpNumber, float actualTemp, float setTemp)
{
	if (!isfinite(actualTemp) || !isfinite(setTemp) || pumpNumber < 1 || pumpNumber > _MAX_HEATING_PUMPS_NO)
		return false;
	bool canTurnOff = true;
	//check turn on off validation for example from config.history

	//negative validations
	if (pumpNumber < 1 || pumpNumber > _MAX_HEATING_PUMPS_NO)
	{
#ifndef _CPPWIN
		Serial.println("no because of invalid pump number");
#endif // !_CPPWIN
		canTurnOff = false;
	}
	//check last _MIN_PUMP_ONOFF_CYCLE minut history  for switch on - off
	if (!_config->canStopPump(pumpNumber))
	{
		canTurnOff = false;
#ifndef _CPPWIN
		Serial.println("no because of _MIN_PUMP_ONOFF_CYCLE");
#endif // !_CPPWIN
	}
	//cannot turn off not running pump
	if (!_config->getPumpStatus(pumpNumber))
	{
		canTurnOff = false;
#ifndef _CPPWIN
		Serial.println("no because of pump is not runnig");
#endif // !_CPPWIN
	}

	if (canTurnOff)
	{
#ifndef _CPPWIN
		Serial.println(" I can Turn Off ");
		Serial.print(pumpNumber);
#endif // !_CPPWIN

		//doing off pump action
		tm tTime = {};
		tTime.tm_hour = hour();
		tTime.tm_min = minute();
		tTime.tm_mday = day();
		tTime.tm_wday = weekday();
		if (!_scheduler->addExecuteTask(new (std::nothrow) hPumpCommand(true, tTime, hourly, pumpNumber + 10, _config)))
			return false;
		_config->setPumpStatusOff(pumpNumber);
	}
	else
	{
#ifndef _CPPWIN
		Serial.println("CANT turn off");
#endif // !_CPPWIN
	}
	return canTurnOff;
}

bool hPumpsController::turnOnDomesticWaterPumpReq(tm tTime)
{

	//functions will be call from MQTT incoming requests
	return _scheduler->addTask(new (std::nothrow) hDomesticWaterPumpCommand(false, tTime, daily, _DOMESTIC_WATER_PUMP,_config)) >= 0;
}

bool hPumpsController::turnOffDomesticWaterPumpReq(tm tTime)
{
	if (_scheduler->addTask(new (std::nothrow) hDomesticWaterPumpCommand(false, tTime, daily, _DOMESTIC_WATER_PUMP_OFF, _config)) < 0)
		return false;
	_config->manualCirculationEnabled = false;
	return true;
}

bool hPumpsController::forceStopPump(int pumpNumber)
{
	_config->revokeHeating(pumpNumber);
	if (pumpNumber >= 1 && pumpNumber <= _MAX_HEATING_PUMPS_NO)
		removeDailyPlan(pumpNumber); // Remove queued CO ON/OFF; preserve the independent CWU plan.
	if (!_config->switchPump(pumpNumber, false)) return false;
	_config->setPumpStatusOff(pumpNumber);
	_config->completeSafetyStop(pumpNumber);
	return true;
}

void hPumpsController::checkThermostatTimeouts()
{
	for (int i = 1; i <= _MAX_HEATING_PUMPS_NO; ++i) {
		_config->expireContact(i);
		if (_config->safetyStopRequired(i)) forceStopPump(i);
	}
}

void hPumpsController::sanityCheck()
{
	_config->tickMinutes();
	for (int i = 1; i <= _DOMESTIC_WATER_PUMP; ++i) {
		int limit = i == _DOMESTIC_WATER_PUMP ? _DOMESTIC_WATER_PUMP_RUN_MINUTS : _MAX_HEATING_PUMP_RUNNING_MINUTES;
		if (_config->getPumpStatus(i) && _config->getPumpRunningMinuts(i) >= limit)
			forceStopPump(i); // Does not depend on scheduler capacity or recursive execution.
	}
}

bool hCallbackCommand::execute()
{
	if (_callbackFunction != NULL)
	{
		_callbackFunction();
		return true;
	}
	else
		return false;
}

bool hDomesticWaterPumpCommand::execute()
{
	if (_config == nullptr || (payload != _DOMESTIC_WATER_PUMP && payload != _DOMESTIC_WATER_PUMP_OFF)) return false;
	bool running = payload == _DOMESTIC_WATER_PUMP;
	if (running && (_config->getPumpStatus(_DOMESTIC_WATER_PUMP) || !_config->canRestartPump(_DOMESTIC_WATER_PUMP))) return false;
	if (!running && _config->getPumpStatus(_DOMESTIC_WATER_PUMP) && !_config->canStopPump(_DOMESTIC_WATER_PUMP)) return false;
	if (!_config->switchPump(_DOMESTIC_WATER_PUMP, running)) return false;
	if (running) _config->setPumpStatusOn(_DOMESTIC_WATER_PUMP, 45, 45);
	else _config->setPumpStatusOff(_DOMESTIC_WATER_PUMP);
	return true;
}
