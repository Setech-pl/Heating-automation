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


//global hook functions
void hookTest() {
	//std::cout << "\n hook function";
}


int main()

{


	srand(time(0));
	//Arduino global variables
	hScheduler* dyro = new hScheduler();
	hConfigurator* config = new hConfigurator();
	hPumpsController* heatPumpController = new hPumpsController(dyro, config);
	tm time;
	time.tm_hour = hour();
	time.tm_min = minute();
	time.tm_wday = weekday();
	time.tm_mday = day();
	hCallbackCommand* tt = new hCallbackCommand(false, time, minutly, &hookTest);
	dyro->addTask(tt);
	dyro->addTask(new enable_internal_wifi(true, time, minutly, 0));
	heatPumpController->createDailyPlan(false);

	while (1) {
		
		for (int i = 0; i < 100; i++) {
			heatPumpController->turnOnHeatPumpReq(random(0,3), 0, 5);
			dyro->executeTasks();
			config->tickMinutes();
		}
		for (int i = 0; i < 100; i++) {
			heatPumpController->turnOffHeatPumpReq(random(0, 3));
			dyro->executeTasks();
			config->tickMinutes();
		}


		heatPumpController->turnOnCircPumpReq();
		dyro->executeTasks();
		config->tickMinutes();
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
