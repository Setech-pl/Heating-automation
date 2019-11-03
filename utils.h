#pragma once
#include "scheduler.h"
/*
Utility commands
*/


class ntp_update : public hCommand {
  public:
    bool execute();
    ntp_update(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload) : hCommand(disposable, scheduleTime, scheduleType, payload) {};

};

class connect_external_wifi : public hCommand {
  public:
    bool execute();
    connect_external_wifi(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload) : hCommand(disposable, scheduleTime, scheduleType, payload) {};

};


class enable_internal_wifi : public hCommand {
public:
	bool execute();
	enable_internal_wifi(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload) : hCommand(disposable, scheduleTime, scheduleType, payload) {};
};
