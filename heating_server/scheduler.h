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
	virtual ~hCommand() = default;
	bool isDuplicateOf(const hCommand &other) const;
	// Unknown derived commands are not deduplicated. Each concrete command
	// opts in with its own typeKey; context identifies any external target.
	virtual bool requiresClock() const { return true; }
	virtual const void *commandType() const { return nullptr; }
	virtual const void *commandContext() const { return _config; }
	hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload);
	hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, void (*callbackFunction)());
	hCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload, hConfigurator *_config);
	bool disposable;
	tm scheduleTime;
	escheduleType scheduleType;
	int payload;
	char result[21] = {};

private:
	friend class hScheduler;
	struct Identity { int hour, minute, second, weekday, day, month, year; };
	const Identity _identity; // Original registration key; advancing time never changes it.
	time_t _nextDue = 0;
	bool _scheduleInitialized = false;
protected:
	template<class T> static const void *typeKey() {
		static char key;
		return &key;
	}
	void (*_callbackFunction)() = nullptr;
	hCommand *pumpsController = nullptr;
	hConfigurator *_config = nullptr;
};

class hCallbackCommand : public hCommand
{
public:
	const void *commandType() const override { return typeKey<hCallbackCommand>(); }
	bool execute() override;
	hCallbackCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, void (*callbackFunction)()) : hCommand(disposable, scheduleTime, scheduleType, callbackFunction){};
};


class hDomesticWaterPumpCommand : public hCommand
{
public:
	const void *commandType() const override { return typeKey<hDomesticWaterPumpCommand>(); }
	bool execute() override;
	hDomesticWaterPumpCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload, hConfigurator *_config) : hCommand(disposable, scheduleTime, scheduleType, payload, _config) {};

};



class hScheduler
{
public:
	enum AddResult { invalidTask = -1, full = -2, duplicate = -3 };
	// Takes ownership of a fresh heap command on every outcome. Returns a
	// slot [0, maxTaskCount()) or a negative AddResult. Re-submitting an
	// already owned pointer rejects it without deleting the existing task.
	int addTask(hCommand *polecenie);
	// Consumes as above; runs only this task if due, returning execute()'s result.
	// Accepted tasks that cannot run yet remain owned. Reentrant execution
	// returns false; callbacks may still add or remove tasks.
	bool addExecuteTask(hCommand *polecenie);
	// Removal/destruction deletes owned tasks through the virtual destructor.
	// Removing the currently executing task defers deletion until it returns.
	void removeAllCommands();
	// Scans due tasks from commandId; true iff at least one was attempted
	// and all attempted executions succeeded. Disposable tasks are consumed
	// after one attempt, including a failed attempt. Recurring tasks remain
	// owned until explicit removal or scheduler destruction.
	bool executeTasks(int commandId = 0);
	void removeCommand(int cNumber);
	void removeCommands(int payload);
	// Borrowed pointer; invalid after removal, disposable execution or destruction.
	hCommand *getTask(int taskNumber);
	int maxTaskCount();
	int activeTaskCount();
	hScheduler();
	~hScheduler();

private:
	hScheduler(const hScheduler &) = delete;
	hScheduler &operator=(const hScheduler &) = delete;
	static const int commandCounter = 512;
	hCommand *commands[commandCounter] = {};
	// Callbacks may remove their own task: unlink immediately, delete only
	// after execute() returns. There is no separate pointer execution queue.
	hCommand *_executing = nullptr;
	bool _executingRemoved = false;
	bool executeTask(int commandId);
	int getFreeSlot();
	bool checkSchedule(int cNumber);
	bool validSchedule(const hCommand &command) const;
	time_t nextOccurrence(const hCommand &command, time_t after) const;
	void updateSchedule(hCommand &command, time_t next);
	void initializeSchedule(hCommand &command, time_t current);
	bool findDuplicate(hCommand *polecenie);
};

/*
	heating related commands
*/

class hPumpCommand : public hCommand
{
public:
	const void *commandType() const override { return typeKey<hPumpCommand>(); }
	bool execute() override;
	bool requiresClock() const override { return !disposable; }
	hPumpCommand(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload, hConfigurator *config = nullptr)
		: hCommand(disposable, scheduleTime, scheduleType, payload, config) {};
};

class hPumpsController
{
public:
	hPumpsController(hScheduler *scheduler, hConfigurator *config);
	bool createDailyPlan(bool holiday);
	void removeDailyPlan(int pumpNumber); //removes plan for pump number /1-5/
	bool turnOnHeatPumpReq(int pumpNumber, float actualTemp, float setTemp);
	bool turnOffHeatPumpReq(int pumpNumber, float actualTemp, float setTemp);
	bool turnOnDomesticWaterPumpReq(tm tTime);
	bool turnOffDomesticWaterPumpReq(tm tTime);
	void sanityCheck();
	bool forceStopPump(int pumpNumber);

private:
	hScheduler *_scheduler;
	hConfigurator *_config;
};
