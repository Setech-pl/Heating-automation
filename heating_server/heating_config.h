#pragma once

#define _EXTERNAL_WIFI_SID ""
#define _EXTERNAL_WIFI_PASS ""
#define _INTERNAL_WIFI_SID ""
#define _INTERNAL_WIFI_PASS ""
#define _MQTT_SERVER "192.168.1.5"
#define _MQTT_SERVER_PORT 1883
#define _MQTT_LOGIN ""
#define _MQTT_PASSWORD ""
#include "secrets.h"

#define _MQTT_SERVER_LOG_TOPIC "HAlog"
#define _MQTT_SERVER_IN_COMMANDS_TOPIC "HAincommands"
#define _MQTT_SERVER_OUT_COMMANDS_TOPIC "HAoutcommands"
#define _SERVER_VERSION "Heating server (0.2.2)"
const bool _INTERNAL_WIFI_MODE = false;

//heating & domestic pumps management
// Prewent on off cycle validation

#define _MIN_MINUTS_FROM_LAST_START 1
#define _DISABLE_MAX_ONOFF_VALIDATION false

#define _MAX_HEATING_INTERIOR_TEMP 28
#define _MAX_HEATING_TEMP_DELTA 0.7
#define _MAX_HEATING_PUMPS_NO 4
#define _MAX_DAY_OVERHEATING 1
#define _MAX_NIGHT_COOLING -2
#define _MAX_HEATING_PUMP_RUNNING_MINUTES 7720
const int _PRIORITY_ARRAY[_MAX_HEATING_PUMPS_NO] = {2, 2, 1, 0}; //Priority array,
#define _DOMESTIC_WATER_PUMP 5									 //1,2,3,4 heating pumps, 5 water pump
#define _DOMESTIC_WATER_PUMP_OFF 15
#define _DOMESTIC_WATER_PUMP_RUN_MINUTS 15

//messages
#define _MSG_UPDATE_NTP "NTP: Update "
#define _MSG_UPDATE_NTP_SUCCESS "NTP: Success"
#define _INTERNAL_WIFI_CONNECTED "Int.WiFi: ok"

struct pumpStatus
{
	int minuts = 0; //worked minuts;
	int actualMinute = 0;
	bool running = false;
	int start_minute = 0;
	int start_hour = 0;
	int start_day = 0;
	int pumpNumber = 0;
	float actualTemp = 0;
	float setTemp = 0;
};

struct thermoClientStat
{
	int ID;
	long int serialChip = 0;
	char version[5];
	char IP[16];
};

class hConfigurator
{
public:
	void setPumpStatusOn(int pumpNumber, float actualTemp, float setTemp);
	void setPumpStatusOff(int pumpNumber);
	bool getPumpStatus(int pumpNumber);		  //return pump status true - is running false isn't running
	int getPumpRunningMinuts(int pumpNumber); //return number of minuts running for pumpNumber;
	bool heatPumpsRunning();
	bool domesticWaterPumpIsRunning();
	int lastOnOffPump(int pumpNumber, int lastMinuts); //count on-off cycles history for pumpNumber
	void tickMinutes();								   // tick and add minuts to running pumps
	bool holidayPlan = false;
	bool manualCirculationEnabled = false;
	bool registerClient(thermoClientStat client);
	void setMQTTStatus(bool mqttStatus);
	bool getMQTTStatus();
	hConfigurator();
	int getPercentage(int pumpNumber);
	~hConfigurator();

private:
	pumpStatus _pumps[6];
	pumpStatus _pumpsHistory[256];
	bool mqttStatus = false;
	//thermoClientStat _clients[4];
	int _pumpsHistoryC = 0;
	void saveHistory(pumpStatus oldStatus);
};
