#pragma once

#ifndef _CPPWIN 
  #include <Time.h>
  #include <TimeLib.h>
  #include <ESP8266WiFi.h>  
 #include <LiquidCrystal_I2C.h>
#else
  #include "arduino_stub.h"
#endif // _CPPWIN
#include "heating_config.h"

#include "scheduler.h"

const char _BLANK_LINE[21] = "                    ";

class hScreen {
	public:
    hScreen(LiquidCrystal_I2C* lcd, hConfigurator* config);
	void printStatusBar(const char Msg[]);
    void printMainScreen(); 
    void printSplashScreen(); 
	void printMenu();
	void nextMenu(int submenu = 0); //if submenu = 0 changes to next menu item, or changes to submenu item;
    void renderScreen();
    void clearScreen();     
	void backlight();
    char* getLine(int LineNumber);
	int selectedMenuItem();
	int selectedSubMenuItem();
	void noBackLight();


  
  private:
	int _menuItem = 0;
	int _submenuItem = 0;
    bool _clearScreen;
	char lines[4][21];
    char _lines[4][21];    
    bool checkLinesChanges(int lineNumber);
	bool _backLight = false;
    LiquidCrystal_I2C* _lcd;
	hConfigurator* _config;

};


class hDisableBacklight : public hCommand {
public:
	bool execute();
	hDisableBacklight(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload, hScreen*  display) ;
private:
	hScreen* _lcd;

};
