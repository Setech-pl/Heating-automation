#define _CPPWIN 1

#include "scheduler.h"
#include <iostream>
#include <time.h>
#include "utils.h"
#ifndef _CPPWIN
#include <TimeLib.h>
#endif // !_CPPWIN

#ifdef _CPPWIN
#include "arduino_stub.h"
#endif

/*
payload (1-4 heating pumps, 5 circulation pump):
0 -  turn on pump (0)
10 - turn off pump (0)
1 - turn on pump (1)
11 - turn off pump (1)
2 - turn on pump (2)
12 - turn off pump (2)
...

pumps aware payloads

123 Update time from NTP
*/



int hScheduler::addTask(hCommand* command)
{
	int i = getFreeSlot();
	if (i < commandCounter) {
		if (! findDuplicate( command))
		commands[i] = command;

	}
	else {
		delete command;
		i = 0;
	}
	return i;
}

int hScheduler::addExecuteTask(hCommand * polecenie)
{
	int commandId = 0;
	commandId=addTask(polecenie);
	executeTasks(commandId);
	return 1;
}


void hScheduler::removeAllCommands()
{
	for (int i = 0; i < commandCounter; i++) {
		removeCommand(i);
	}
}

void hScheduler::executeTasks(int commandId)
{
	int i = commandId;
	while ( i < commandCounter) {
		if (checkSchedule(i) ){
			commands[i]->execute();
			//remove disposable commands
			if (commands[i]->disposable){
				removeCommand(i);				
			}
		}		
		i++;
	}
}

void hScheduler::removeCommand(int cNumber)
{
	if (cNumber >= 0 && cNumber < commandCounter) {
		if (commands[cNumber] != NULL) {
			delete commands[cNumber];
			commands[cNumber] = NULL;

		}
	}

}


void hScheduler::removeCommands(int payload)
{
	for (int i = 0; i < commandCounter; i++) {
		if (commands[i] != NULL) {
			if (commands[i]->payload == payload)  removeCommand(i);
		}

	}
}


hCommand * hScheduler::getTask(int taskNumber)
{
	if (taskNumber <= commandCounter) {
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
	for (int i = 0; i < commandCounter; i++) {
		if (commands[i] != 0) result++;
	}
	return 0;
}

hScheduler::hScheduler()
{
	for (int i = 0; i < commandCounter; i++) {
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
	while (i < commandCounter && commands[i]) {
		i++;
	}
	return i;


}

//checking command validation

bool hScheduler::checkSchedule(int cNumber)
{
	if (commands[cNumber] == NULL){
		return false;
	}
	switch (commands[cNumber]->scheduleType) {
	case daily:
		if (commands[cNumber]->scheduleTime.tm_wday==weekday() && commands[cNumber]->scheduleTime.tm_hour == hour() 
			&& (commands[cNumber]->scheduleTime.tm_min = minute())) {
			commands[cNumber]->scheduleTime.tm_wday++;
				if (commands[cNumber]->scheduleTime.tm_wday > 6) {
					commands[cNumber]->scheduleTime.tm_wday = 0;
				}
			return true;
		}
		else {
			return false;
		}
		break;
	case hourly:
		if (commands[cNumber]->scheduleTime.tm_hour == hour() && commands[cNumber]->scheduleTime.tm_min == minute()) {
			commands[cNumber]->scheduleTime.tm_hour++;
			if (commands[cNumber]->scheduleTime.tm_hour > 23) {
				commands[cNumber]->scheduleTime.tm_hour = 0;
			}
			return true;

		}
		else {
			return false;
		}
		break;
	case minutly:
		if (commands[cNumber]->scheduleTime.tm_min = minute()) {
			commands[cNumber]->scheduleTime.tm_min++;
			if (commands[cNumber]->scheduleTime.tm_min > 59) {
				commands[cNumber]->scheduleTime.tm_min = 0;
			}
			return true;
		}
		else {
			return false;
		}
		break;
	}

	return false;
}

bool hScheduler::findDuplicate(hCommand * command)
{
	int i = 0;
	bool result = false;
	while ( i < commandCounter) {
		if (commands[i] != NULL) {
			if (
				//(commands[i]->disposable == command->disposable) &&
				(commands[i]->payload == command->payload) &&
				(commands[i]->scheduleTime.tm_hour==command->scheduleTime.tm_hour) &&
				(commands[i]->scheduleTime.tm_min==command->scheduleTime.tm_min) &&
				(commands[i]->scheduleTime.tm_sec==command->scheduleTime.tm_sec) &&
				(commands[i]->scheduleTime.tm_wday==command->scheduleTime.tm_wday)
				) {
				result = true;
				break;
			}
		}
		i++;
	}
	return result;
}

void hScheduler::saintyCheck()
{
}




hCommand::hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload)
{
	this->disposable = disposable;
	this->scheduleTime = scheduleTime;
	this->scheduleType = scheduleType;
	this->payload = payload;

}

hCommand::hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, void(*callbackFunction)())
{
	this->disposable = disposable;
	this->scheduleTime = scheduleTime;
	this->scheduleType = scheduleType;
	this->_callbackFunction = callbackFunction;
}



bool hPumpCommand::execute()
{
	// turn on selected pump turnOnPump(payload);

	return true;

}


hPumpsController::hPumpsController(hScheduler *scheduler, hConfigurator *config)
{
	_scheduler = scheduler;
	_config = config;
}


void hPumpsController::createDailyPlan(bool holiday)  //daily plan factory
{
	tm scht;

		if (holiday) {
			//create holiday daily plan for floor heating or domestic hot water circulation pump

			

		}

		if (!holiday) {
			//create normal daily plan for  domestic hot water circulation pump   
			scht.tm_hour = 5;
			scht.tm_min =  0;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP));
			scht.tm_hour = 5;
			scht.tm_min =  30;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP_OFF));
			scht.tm_hour = 6;
			scht.tm_min =  0;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP));
			scht.tm_hour = 6;
			scht.tm_min =  30;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP_OFF ));

			scht.tm_hour = 14;
			scht.tm_min =  0;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP));
			scht.tm_hour = 14;
			scht.tm_min =  30;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP_OFF));
			scht.tm_hour = 16;
			scht.tm_min =  0;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP));
			scht.tm_hour = 16;
			scht.tm_min =  30;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP_OFF));
			scht.tm_hour = 18;
			scht.tm_min =  0;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP));
			scht.tm_hour = 18;
			scht.tm_min =  30;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP_OFF));
			scht.tm_hour = 20;
			scht.tm_min = 0;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP));
			scht.tm_hour = 20;
			scht.tm_min = 30;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP_OFF));
			scht.tm_hour = 22;
			scht.tm_min = 0;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP));
			scht.tm_hour = 22;
			scht.tm_min = 30;
			_scheduler->addTask(new hPumpCommand(false, scht, daily, _DOMESTIC_WATER_PUMP_OFF));

		}

		//NTP Update at sunday 03:00
		scht.tm_hour = 3;
		scht.tm_min = 0;
		scht.tm_wday = 0;
		_scheduler->addTask(new ntp_update(false, scht, daily, 123));

}

void hPumpsController::removeDailyPlan(int pumpNumber)
{
	_scheduler->removeCommands(pumpNumber);
	_scheduler->removeCommands(pumpNumber+10);
}

void hPumpsController::turnOnHeatPumpReq(int pumpNumber, float actualTemp, float setTemp)
{
	bool canTurnOn = true;
	int taskId=0;
	float tempModifier = 0;
	if (hour() > 10 && hour() < 14) {
		tempModifier = _MAX_DAY_OVERHEATING;
	}
	if (hour() > 22 && hour() < 6) {
		tempModifier = _MAX_NIGHT_COOLING;
	}
	//check turn on off validation for example from config.history
	
	//negative validations
	if (pumpNumber < 0 || pumpNumber>=_DOMESTIC_WATER_PUMP) canTurnOn = false;
	if (actualTemp > setTemp+_MAX_HEATING_TEMP_DELTA+ tempModifier) canTurnOn = false;
	if (actualTemp > _MAX_HEATING_INTERIOR_TEMP+ tempModifier) canTurnOn = false;
	if (_config->getPumpStatus(pumpNumber)) canTurnOn = false;
	if (canTurnOn) {
		tm tTime;
		tTime.tm_hour = hour();
		tTime.tm_min = minute();
		tTime.tm_mday = day();
		tTime.tm_wday = weekday();
		//taskId=this->_scheduler->addTask(new hPumpCommand(true, tTime, hourly, pumpNumber));
		taskId = this->_scheduler->addExecuteTask(new hPumpCommand(true, tTime, hourly, pumpNumber));
		//update config
		_config->setPumpStatusOn(pumpNumber,actualTemp,setTemp); 
	}
}

void hPumpsController::turnOffHeatPumpReq(int pumpNumber, float actualTemp, float setTemp)
{
	bool canTurnOff = true;
	int taskId = 0;
	if (pumpNumber < 0 || pumpNumber>3) canTurnOff = false;
	//check last _MIN_PUMP_ONOFF_CYCLE minut history  for switch on - off
	if (_config->lastOnOffPump(pumpNumber, _MIN_PUMP_ONOFF_CYCLE)>0) canTurnOff = false;
	if (!_config->getPumpStatus(pumpNumber)) canTurnOff = false;
	if (canTurnOff) {
		//doing off pump action
		tm tTime;
		tTime.tm_hour = hour();
		tTime.tm_min = minute();
		tTime.tm_mday = day();
		tTime.tm_wday = weekday();
		//taskId=this->_scheduler->addTask(new hPumpCommand(true, tTime, hourly, pumpNumber+10));
		taskId = this->_scheduler->addExecuteTask(new hPumpCommand(true, tTime, hourly, pumpNumber + 10));
		_config->setPumpStatusOff(pumpNumber);
	}
}

void hPumpsController::turnOnCircPumpReq()
{
	//enables circulation pump and turn on 5  minutes
	_config->manualCirculationEnabled = true;
	int taskId = 0;
	tm tTime;
	tTime.tm_hour = hour();
	tTime.tm_min = minute();
	tTime.tm_mday = day();
	tTime.tm_wday = weekday();
	taskId=_scheduler->addTask(new hPumpCommand(true, tTime, hourly, _DOMESTIC_WATER_PUMP));
	tTime.tm_min = minute() + 5;
	if ((tTime.tm_min+5) > 59) tTime.tm_min = tTime.tm_min - 60;
	//tTime.tm_min = (minute()+5)>59 ? (minute()+5-59) : (minute()+5);
	taskId = _scheduler->addTask(new hPumpCommand(true, tTime, hourly, _DOMESTIC_WATER_PUMP_OFF));
}

void hPumpsController::turnOffCircPumpReq()
{	
	_config->manualCirculationEnabled = false;
	int taskId = 0;
	tm tTime;
	tTime.tm_hour = hour();
	tTime.tm_min = minute();
	tTime.tm_mday = day();
	tTime.tm_wday = weekday();
	taskId = _scheduler->addTask(new hPumpCommand(true, tTime, minutly, 14));
}

void hPumpsController::saintyCheck()
{

	//search for pumps running longer than 24h
}

bool hCallbackCommand::execute()
{
	if (_callbackFunction != NULL) {
		_callbackFunction();
		return true;
	}else 
	return false;
}
