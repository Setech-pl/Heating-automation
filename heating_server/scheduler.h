
#pragma once
#include <time.h>
#include "heating_config.h"

enum escheduleType
{
	daily = 0,
	hourly = 1,
	minutly = 2,
	weekly = 3,
	monthly = 4
};

class hCommand
{
public:
	virtual bool execute() = 0;
	hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload);
	hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, void (*callbackFunction)());

	bool disposable;
	tm scheduleTime;
	escheduleType scheduleType;
	int payload;
	char result[21];

protected:
	void (*_callbackFunction)();
	hCommand *pumpsController;
};

class hCallbackCommand : public hCommand
{
public:
	bool execute();
	hCallbackCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, void (*callbackFunction)()) : hCommand(disposable, scheduleTime, scheduleType, callbackFunction){};
};

class hScheduler
{
public:
	int addTask(hCommand *polecenie); //return integer id of added task
	int addExecuteTask(hCommand *polecenie);
	void removeAllCommands();
	void executeTasks(int commandId = 0);
	void removeCommand(int cNumber);
	void removeCommands(int payload);
	hCommand *getTask(int taskNumber);
	int maxTaskCount();
	int activeTaskCount();
	hScheduler();
	~hScheduler();

private:
	const unsigned int commandCounter = 512;
	hCommand *commands[512];
	int getFreeSlot();
	bool checkSchedule(int cNumber);
	bool findDuplicate(hCommand *polecenie);
};

/*
	heating related commands
*/

class hPumpCommand : public hCommand
{
public:
	bool execute();
	hPumpCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload) : hCommand(disposable, scheduleTime, scheduleType, payload){};
};

class hPumpsController
{
public:
	hPumpsController(hScheduler *scheduler, hConfigurator *config);
	void createDailyPlan(bool holiday);
	void removeDailyPlan(int pumpNumber); //removes plan for pump number /1-5/
	bool turnOnHeatPumpReq(int pumpNumber, float actualTemp, float setTemp);
	bool turnOffHeatPumpReq(int pumpNumber, float actualTemp, float setTemp);
	void turnOnDomesticWaterPumpReq();
	void turnOffDomesticWaterPumpReq();
	void sanityCheck();

private:
	hScheduler *_scheduler;
	hConfigurator *_config;
};
