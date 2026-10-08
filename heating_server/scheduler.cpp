#define _CPPWINa 1
#include "scheduler.h"
#include <time.h>
#include <new>
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

//checking command validation

bool hScheduler::checkSchedule(int cNumber)
{
	if (commands[cNumber] == NULL)
	{
		return false;
	}
	switch (commands[cNumber]->scheduleType)
	{
	case daily:
		if (commands[cNumber]->scheduleTime.tm_wday == weekday() && commands[cNumber]->scheduleTime.tm_hour == hour() && (commands[cNumber]->scheduleTime.tm_min == minute()))
		{
			commands[cNumber]->scheduleTime.tm_wday++;
			if (commands[cNumber]->scheduleTime.tm_wday > 6)
			{
				commands[cNumber]->scheduleTime.tm_wday = 0;
			}
			return true;
		}
		else
		{
			return false;
		}
		break;
	case hourly:
		if (commands[cNumber]->scheduleTime.tm_hour == hour() && commands[cNumber]->scheduleTime.tm_min == minute())
		{
			commands[cNumber]->scheduleTime.tm_hour++;
			if (commands[cNumber]->scheduleTime.tm_hour > 23)
			{
				commands[cNumber]->scheduleTime.tm_hour = 0;
			}
			return true;
		}
		else
		{
			return false;
		}
		break;
	case minutly:
		if (commands[cNumber]->scheduleTime.tm_min == minute())
		{
			commands[cNumber]->scheduleTime.tm_min++;
			if (commands[cNumber]->scheduleTime.tm_min > 59)
			{
				commands[cNumber]->scheduleTime.tm_min = 0;
			}
			return true;
		}
		else
		{
			return false;
		}
		break;

	case weekly:
		return false; // Weekly scheduling remains unsupported.
	case monthly:
		if (commands[cNumber]->scheduleTime.tm_mon == month() && commands[cNumber]->scheduleTime.tm_hour == hour() && commands[cNumber]->scheduleTime.tm_min == minute())
		{
			commands[cNumber]->scheduleTime.tm_mon++;
			if (commands[cNumber]->scheduleTime.tm_mon > 12)
			{
				commands[cNumber]->scheduleTime.tm_mon = 1;
			}
			return true;
		}
		else
		{
			return false;
		}
		break;
	}

	return false;
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
		scheduleTime.tm_hour == other.scheduleTime.tm_hour &&
		scheduleTime.tm_min == other.scheduleTime.tm_min &&
		scheduleTime.tm_sec == other.scheduleTime.tm_sec &&
		scheduleTime.tm_wday == other.scheduleTime.tm_wday &&
		scheduleTime.tm_mday == other.scheduleTime.tm_mday &&
		scheduleTime.tm_mon == other.scheduleTime.tm_mon &&
		scheduleTime.tm_year == other.scheduleTime.tm_year;
}

hCommand::hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload)
	: disposable(disposable), scheduleTime(scheduleTime), scheduleType(scheduleType), payload(payload)
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
	// turn on selected pump turnOnPump(payload);
#ifndef _CPPWIN
	Serial.println("executing task, for payload= ");
	Serial.print(this->payload);
#endif //
	return true;
}

hPumpsController::hPumpsController(hScheduler *scheduler, hConfigurator *config)
{
	_scheduler = scheduler;
	_config = config;
}

void hPumpsController::createDailyPlan(bool holiday) //daily plan factory
{

	if (holiday)
	{
		//create holiday daily plan for floor heating or domestic hot water circulation pump
	}

	if (!holiday)
	{
	}
}

void hPumpsController::removeDailyPlan(int pumpNumber)
{
	_scheduler->removeCommands(pumpNumber);
	_scheduler->removeCommands(pumpNumber + 10);
}

bool hPumpsController::turnOnHeatPumpReq(int pumpNumber, float actualTemp, float setTemp)
{
	bool canTurnOn = true;
	float tempModifier = 0;
	if (hour() > 10 && hour() < 14)
	{
		tempModifier = _MAX_DAY_OVERHEATING;
	}
	if (hour() > 22 && hour() < 6)
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
		if (!_scheduler->addExecuteTask(new (std::nothrow) hPumpCommand(true, tTime, hourly, pumpNumber)))
			return false;
		//update config
		_config->setPumpStatusOn(pumpNumber, actualTemp, setTemp);
		sanityCheck();
	}
	return canTurnOn;
}

bool hPumpsController::turnOffHeatPumpReq(int pumpNumber, float /*actualTemp*/, float /*setTemp*/)
{
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
	if (_config->lastOnOffPump(pumpNumber, _MIN_MINUTS_FROM_LAST_START) > 0 || _DISABLE_MAX_ONOFF_VALIDATION)
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
		if (!_scheduler->addExecuteTask(new (std::nothrow) hPumpCommand(true, tTime, hourly, pumpNumber + 10)))
			return false;
		_config->setPumpStatusOff(pumpNumber);
		sanityCheck();
	}
	else
	{
#ifndef _CPPWIN
		Serial.println("CANT turn off");
#endif // !_CPPWIN
	}
	return canTurnOff;
}

void hPumpsController::turnOnDomesticWaterPumpReq(tm tTime)
{

	//functions will be call from MQTT incoming requests
	_scheduler->addTask(new (std::nothrow) hDomesticWaterPumpCommand(false, tTime, hourly, _DOMESTIC_WATER_PUMP,_config));
	sanityCheck();
}

void hPumpsController::turnOffDomesticWaterPumpReq(tm tTime)
{
	if (_scheduler->addTask(new (std::nothrow) hDomesticWaterPumpCommand(false, tTime, minutly, _DOMESTIC_WATER_PUMP_OFF, _config)) < 0)
		return;
	_config->manualCirculationEnabled = false;
	sanityCheck();
}

void hPumpsController::sanityCheck()
{
	for (int i = 1; i <= _MAX_HEATING_PUMPS_NO; i++)
	{
		if (_config->getPumpRunningMinuts(i) >= _MAX_HEATING_PUMP_RUNNING_MINUTES)
		{
			this->turnOffHeatPumpReq(i, 0, 0);
		}
	}
	//search for pumps running longer than 24h
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
	if (payload == _DOMESTIC_WATER_PUMP) {
		_config->setPumpStatusOn(_DOMESTIC_WATER_PUMP,45,45);
	}
	else {
		_config->setPumpStatusOff(_DOMESTIC_WATER_PUMP);
	}
	return true;
}
