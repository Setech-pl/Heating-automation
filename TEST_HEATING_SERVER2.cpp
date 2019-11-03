/*
Windows  C++ test file
(c) 2018/2019 by Marceli
//For arduino please use .ino files
*/
#define _CPPWIN 1

#include "scheduler.h"
#include "heating_config.h"
#include <string>
#include <iostream>
#ifdef _CPPWIN
#include "arduino_stub.h"
#include "utils.h"
#endif

#include "screen.h"

//global variables
hScheduler* dyro = new hScheduler();
hConfigurator* config = new hConfigurator();
hPumpsController* heatPumpController = new hPumpsController(dyro, config);

//global hook functions

void hookTest() {
	std::cout << "\n hook function called by monthly schedule";
}

void hookExecuteSanityCheck() {
	heatPumpController->sanityCheck();
}

void createDailyPlans() {
	heatPumpController->createDailyPlan(false);
	tm time;
	time.tm_hour = hour();
	time.tm_min = minute();
	time.tm_wday = weekday();
	time.tm_mday = day();
	hCallbackCommand* tt = new hCallbackCommand(false, time, minutly, &hookExecuteSanityCheck);
	dyro->addTask(tt);
}

int main()

{


	srand(time(0));
	//Arduino global variables

	tm time;
	time.tm_hour = hour();
	time.tm_min = minute();
	time.tm_wday = weekday();
	time.tm_mday = day();
	time.tm_mon = month();
	hCallbackCommand* tt = new hCallbackCommand(false, time, monthly, &hookTest);
	dyro->addTask(tt);
	dyro->addTask(new enable_internal_wifi(true, time, minutly, 0));
	createDailyPlans();

	while (1) {
	/*
		for (int i = 0; i < 256; i++) {
			bool wynik = heatPumpController->turnOnHeatPumpReq(random(1,4), 11, 21);
			dyro->executeTasks();
		}
		for (int i = 0; i < 256; i++) {
			bool wynik2 = heatPumpController->turnOffHeatPumpReq(random(1,4),21, 21);
			dyro->executeTasks();
		}
		*/
		bool wynik = heatPumpController->turnOnHeatPumpReq(1, 11, 21);
		wynik = config->getPumpStatus(1);
		bool wynik2 = heatPumpController->turnOffHeatPumpReq(1, 21, 21);
		wynik = config->getPumpStatus(1);
		config->tickMinutes();

		heatPumpController->turnOnCircPumpReq();
		dyro->executeTasks();
		heatPumpController->turnOffCircPumpReq();
		dyro->executeTasks();
		config->tickMinutes();
	}
	LiquidCrystal_I2C* lcd = new LiquidCrystal_I2C();
	hScreen* e = new hScreen(lcd, config);
	int input = 0;
	e->printSplashScreen();
	e->renderScreen();
	std::cout << "-1 exit, 10 next main  menu item, 1-2 select submenuItem";
	std::cin >> input;
	input = 0;
	while (input >= 0) {
		if (input == 0) {
			e->printMainScreen();
		}
		else {
			e->nextMenu(input);
			e->printMenu();
		}
		e->renderScreen();
		std::cin >> input;
	}
	delete lcd;
	delete e;
	delete heatPumpController;
	delete config;
	delete dyro;
	delete tt;

}
