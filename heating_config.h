#pragma once

#define _EXTERNAL_WIFI_SID ""
#define _EXTERNAL_WIFI_PASS ""
#define _INTERNAL_WIFI_SID ""
#define _INTERNAL_WIFI_PASS ""

#include "secrets.h"
const bool _INTERNAL_WIFI_MODE = false;


//messages
#define _MSG_UPDATE_NTP "NTP: Update "
#define _MSG_UPDATE_NTP_SUCCESS "NTP: Success"
#define _INTERNAL_WIFI_CONNECTED "Int.WiFi: ok"

//heating & domestic pumps management

#define _MIN_PUMP_ONOFF_CYCLE 0
#define _MAX_HEATING_INTERIOR_TEMP 26
#define _MAX_HEATING_TEMP_DELTA 0.7 
#define _MAX_HEATING_PUMPS_NO 4
#define _MAX_DAY_OVERHEATING 1
#define _MAX_NIGHT_COOLING -2
#define _MAX_HEATING_PUMP_RUNNING_MINUTES 13
const int _PRIORITY_ARRAY[_MAX_HEATING_PUMPS_NO] = { 2,2,1,0 }; //Priority array, 
const int _DOMESTIC_WATER_PUMP = 4; //0,1,2,3 heating pumps, 4 water pump
const int _DOMESTIC_WATER_PUMP_OFF = _DOMESTIC_WATER_PUMP + 10;


struct pumpStatus {
	int minuts = 0; //worked minuts;
	int actualMinute = 0;
	bool running = false;
	int start_minute = 0;
	int start_hour = 0;
	int start_day = 0;
	int pumpNumber;
	float actualTemp = 0;
	float setTemp = 0;
};


struct thermoClientStat {
	int ID;
	long int serialChip;
	char version[5];
	char IP[16];
};



class hConfigurator{
  public:

	void setPumpStatusOn(int pumpNumber, float actualTemp, float setTemp);
	void setPumpStatusOff(int pumpNumber);
	bool getPumpStatus(int pumpNumber); //return pump status true - is running false isn't running
	int getPumpRunningMinuts(int pumpNumber);//return number of minuts running for pumpNumber;
	bool heatPumpsRunning();
	bool circulationPumpsRunning();
	int lastOnOffPump(int pumpNumber, int lastMinuts); //count on-off cycles history for pumpNumber
	void tickMinutes(); // tick and add minuts to running pumps
	bool holidayPlan = false;
	bool manualCirculationEnabled = false;
	bool registerClient(thermoClientStat client);
	hConfigurator();
	int getPercentage(int pumpNumber);
	~hConfigurator();
    
   private:
	pumpStatus _pumps[5];
	pumpStatus _pumpsHistory[128];
	//thermoClientStat _clients[4];
	int _pumpsHistoryC = 0;
	void saveHistory(pumpStatus oldStatus);

};
