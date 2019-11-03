#define _CPPWINa 1
#include "scheduler.h"
#include <time.h>
#include "utils.h"
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
	int i = getFreeSlot();
	if (i < commandCounter)
	{
		if (!findDuplicate(command))
			commands[i] = command;
	}
	else
	{
		delete command;
		i = 0;
	}
	return i;
}

int hScheduler::addExecuteTask(hCommand *polecenie)
{
	int commandId = 0;
	commandId = addTask(polecenie);
	executeTasks(commandId);
	return 1;
}

void hScheduler::removeAllCommands()
{
	for (int i = 0; i < commandCounter; i++)
	{
		removeCommand(i);
	}
}

void hScheduler::executeTasks(int commandId)
{
	int i = commandId;
	while (i < commandCounter)
	{
		if (checkSchedule(i))
		{
			commands[i]->execute();
			//remove disposable commands
			if (commands[i]->disposable)
			{
				removeCommand(i);
			}
		}
		i++;
	}
}

void hScheduler::removeCommand(int cNumber)
{
	if (cNumber >= 0 && cNumber < commandCounter)
	{
		if (commands[cNumber] != NULL)
		{
			delete commands[cNumber];
			commands[cNumber] = NULL;
		}
	}
}

void hScheduler::removeCommands(int payload)
{
	for (int i = 0; i < commandCounter; i++)
	{
		if (commands[i] != NULL)
		{
			if (commands[i]->payload == payload)
				removeCommand(i);
		}
	}
}

hCommand *hScheduler::getTask(int taskNumber)
{
	if (taskNumber <= commandCounter)
	{
		return commands[taskNumber];
	}
	return nullptr;
}

int hScheduler::maxTaskCount()
{
	return commandCounter;
}

int hScheduler::activeTaskCount()
{
	int result = 0;
	for (int i = 0; i < commandCounter; i++)
	{
		if (commands[i] != 0)
			result++;
	}
	return 0;
}

hScheduler::hScheduler()
{
	for (int i = 0; i < commandCounter; i++)
	{
		commands[i] = NULL;
	}
}

hScheduler::~hScheduler()
{
	removeAllCommands();
}

int hScheduler::getFreeSlot()
{
	int i = 0;
	while (i < commandCounter && commands[i])
	{
		i++;
	}
	return i;
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
	int i = 0;
	bool result = false;
	while (i < commandCounter)
	{
		if (commands[i] != NULL)
		{
			if (
				//(commands[i]->disposable == command->disposable) &&
				(commands[i]->payload == command->payload) &&
				(commands[i]->scheduleTime.tm_hour == command->scheduleTime.tm_hour) &&
				(commands[i]->scheduleTime.tm_min == command->scheduleTime.tm_min) &&
				(commands[i]->scheduleTime.tm_sec == command->scheduleTime.tm_sec) &&
				(commands[i]->scheduleTime.tm_wday == command->scheduleTime.tm_wday))
			{
				result = true;
				break;
			}
		}
		i++;
	}
	return result;
}

hCommand::hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload)
{
	this->disposable = disposable;
	this->scheduleTime = scheduleTime;
	this->scheduleType = scheduleType;
	this->payload = payload;
}

hCommand::hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, void (*callbackFunction)())
{
	this->disposable = disposable;
	this->scheduleTime = scheduleTime;
	this->scheduleType = scheduleType;
	this->_callbackFunction = callbackFunction;
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
	tm scht;

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
	int taskId = 0;
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
		tm tTime;
		tTime.tm_hour = hour();
		tTime.tm_min = minute();
		tTime.tm_mday = day();
		tTime.tm_wday = weekday();
		//taskId=this->_scheduler->addTask(new hPumpCommand(true, tTime, hourly, pumpNumber));
		taskId = this->_scheduler->addExecuteTask(new hPumpCommand(true, tTime, hourly, pumpNumber));
		//update config
		_config->setPumpStatusOn(pumpNumber, actualTemp, setTemp);
		sanityCheck();
	}
	return canTurnOn;
}

bool hPumpsController::turnOffHeatPumpReq(int pumpNumber, float actualTemp, float setTemp)
{
	bool canTurnOff = true;
	int taskId = 0;
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
		tm tTime;
		tTime.tm_hour = hour();
		tTime.tm_min = minute();
		tTime.tm_mday = day();
		tTime.tm_wday = weekday();
		//taskId=this->_scheduler->addTask(new hPumpCommand(true, tTime, hourly, pumpNumber+10));
		taskId = this->_scheduler->addExecuteTask(new hPumpCommand(true, tTime, hourly, pumpNumber + 10));
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

void hPumpsController::turnOnDomesticWaterPumpReq()
{
	//enables circulation pump and turn on _DOMESTIC_WATER_PUMP_RUN_MINUTS  minutes
	//functions will be call from MQTT incoming requests
	_config->manualCirculationEnabled = true;
	int taskId = 0;
	tm tTime;
	tTime.tm_hour = hour();
	tTime.tm_min = minute();
	tTime.tm_mday = day();
	tTime.tm_wday = weekday();
	taskId = _scheduler->addTask(new hPumpCommand(true, tTime, hourly, _DOMESTIC_WATER_PUMP));
	tTime.tm_min = minute() + _DOMESTIC_WATER_PUMP_RUN_MINUTS;
	if ((tTime.tm_min + _DOMESTIC_WATER_PUMP_RUN_MINUTS) > 59)
	{
		tTime.tm_min = tTime.tm_min + _DOMESTIC_WATER_PUMP_RUN_MINUTS - 59;
		tTime.tm_hour++;
		if (tTime.tm_hour > 23)
			tTime.tm_hour = 0;
	}

	taskId = _scheduler->addTask(new hPumpCommand(true, tTime, hourly, _DOMESTIC_WATER_PUMP_OFF));
	_config->setPumpStatusOn( _DOMESTIC_WATER_PUMP,45,45);
	sanityCheck();
}

void hPumpsController::turnOffDomesticWaterPumpReq()
{
	_config->manualCirculationEnabled = false;
	int taskId = 0;
	tm tTime;
	tTime.tm_hour = hour();
	tTime.tm_min = minute();
	tTime.tm_mday = day();
	tTime.tm_wday = weekday();
	taskId = _scheduler->addTask(new hPumpCommand(true, tTime, minutly, _DOMESTIC_WATER_PUMP_OFF));
	_config->setPumpStatusOff(_DOMESTIC_WATER_PUMP);
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
