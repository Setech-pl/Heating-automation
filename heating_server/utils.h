#pragma once
#include "scheduler.h"
#include <stddef.h>

class hScreen;

// Prefix, separator/name, longest decimal int (including sign), and NUL.
const size_t THERMOSTAT_TOPIC_CAPACITY =
    sizeof(_MQTT_SENSORS_TOPIC) + sizeof("/thermostat") - 1 + sizeof(int) * 3;
bool formatThermostatTopic(char *topic, size_t capacity, int id);
/*
Utility commands
*/


class ntp_update : public hCommand {
  public:
	const void *commandType() const override { return typeKey<ntp_update>(); }
    enum StartupResult { skipped, synchronized, failed };
    StartupResult synchronizeOnStartup(bool internalWiFiMode, hScreen &display);
    bool execute() override;
    ntp_update(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload) : hCommand(disposable, scheduleTime, scheduleType, payload) {};

};

class connect_external_wifi : public hCommand {
  public:
	const void *commandType() const override { return typeKey<connect_external_wifi>(); }
    bool execute() override;
    connect_external_wifi(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload) : hCommand(disposable, scheduleTime, scheduleType, payload) {};

};


class enable_internal_wifi : public hCommand {
public:
	const void *commandType() const override { return typeKey<enable_internal_wifi>(); }
	bool execute() override;
	enable_internal_wifi(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload) : hCommand(disposable, scheduleTime, scheduleType, payload) {};
};
