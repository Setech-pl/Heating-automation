#define _CPPWINa 1
#define _CRT_SECURE_NO_WARNINGS
#include "screen.h"
#include "heating_config.h"
#include <stdlib.h>
#include <stdio.h>
#include  <string.h>
#include "scheduler.h"

#ifdef _CPPWIN
#include "arduino_stub.h"
#endif



#include <Time.h>
#ifndef _CPPWIN
#include <TimeLib.h>
#endif

void hScreen::printStatusBar(const char Msg[]){
  strcpy(this->lines[3],_BLANK_LINE);                        
  strcpy(this->lines[3],Msg); 
}




void hScreen::printMainScreen(){
  char hr[21];
  sprintf(hr," H(%s)   %02d:%02d   C(%s)", _config->heatPumpsRunning() ? "*" : "-",  hour(), minute(), _config->circulationPumpsRunning() ? "*" : "-");
  strcpy(this->lines[0],_BLANK_LINE);    
  strcpy(this->lines[1],_BLANK_LINE);    
  strcpy(this->lines[2],_BLANK_LINE);  
  strcpy(this->lines[3],_BLANK_LINE);
  strcpy(this->lines[0],hr);
  sprintf(hr, "%sP1: %03d%%  %sP2: %03d%%",_config->getPumpStatus(1) ?"+" : " ", _config->getPercentage(1), _config->getPumpStatus(2) ? "+" : " ",_config->getPercentage(2));
  strcpy(this->lines[1], hr);
  sprintf(hr, "%sP3: %03d%%  %sP4: %03d%%", _config->getPumpStatus(3) ?"+" : " ", _config->getPercentage(3), _config->getPumpStatus(4) ? "+" : " ", _config->getPercentage(4));
  strcpy(this->lines[2], hr);

};

char* hScreen::getLine(int LineNumber){
  return this->lines[LineNumber];
}

int hScreen::selectedMenuItem()
{
	return _menuItem;
}

int hScreen::selectedSubMenuItem()
{
	return _submenuItem;
}

void hScreen::noBackLight()
{
	if (_backLight) {
		_lcd->noBacklight();
		_backLight = false;
	}
}



hScreen::hScreen(LiquidCrystal_I2C* lcd, hConfigurator* config){
  _config = config;
  _lcd=lcd;
  for (int i=0; i<4; i++){
    strcpy(this->lines[i],"");
  }
  _clearScreen = false;
};

void hScreen::printSplashScreen(){
  strcpy(this->lines[0],_SERVER_VERSION);
  strcpy(this->lines[1],"(c) 2018/19 Marceli ");
  strcpy(this->lines[2],_BLANK_LINE);
  strcpy(this->lines[3],_BLANK_LINE);  
}
void hScreen::printMenu()
{
#ifdef _CPPWIN
	WiFiUDP WiFi;
#endif
	int signalQuality = WiFi.RSSI();
	if (signalQuality <= -100) {
		signalQuality = 0;
	}
	else if (signalQuality >= -50) {
		signalQuality = 100;
	}
	else {
		signalQuality = 2 * (signalQuality + 100);
	}

	switch (_menuItem) {
		char hr[21];
	case 1 :
		strcpy(this->lines[0], " MENU> CONFIG 1  ");
		sprintf(hr, "%sRECONNECT WIFI", _submenuItem == 1 ? ">" : " ");
		strcpy(this->lines[1], hr);
		sprintf(hr, "%sUPDATE NTP", _submenuItem == 2 ? ">" : " ");
		strcpy(this->lines[2], hr);
    sprintf(hr, "%sMODE :%s", _submenuItem == 3 ? ">" : " ", _INTERNAL_WIFI_MODE ? "INTERNAL" : "EXTERNAL");
    strcpy(this->lines[3], hr);
		break;
	case 2:
		strcpy(this->lines[0], " MENU> CONFIG 2  ");
		sprintf(hr, " %s", WiFi.SSID().c_str());
		strcpy(this->lines[1], hr);
		sprintf(hr, " RSSI :%03d%%", signalQuality);
		strcpy(this->lines[2], hr);
    sprintf(hr," IP : %d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);    
    strcpy(this->lines[3], hr);    
		break;
	case 3:
		strcpy(this->lines[0], " MENU> SCHEDULER  ");
		sprintf(hr, "%sHOLIDAY PLAN: %s", _submenuItem == 1 ? ">" : " ", _config->holidayPlan  ? "ON ":"OFF");
		strcpy(this->lines[1], hr);
		sprintf(hr, "%sNORMAL PLAN : %s", _submenuItem == 2 ? ">" : " ", _config->holidayPlan ? "OFF" : "ON ");
		strcpy(this->lines[2], hr);
		sprintf(hr, "%sRESET SCHEDULER", _submenuItem == 3 ? ">" : " ");
		strcpy(this->lines[3], hr);

		//removes all daily planes for circulation pump

	case 5:
		strcpy(this->lines[0], " MENU> SYSTEM");
		sprintf(hr, "%sCIRCULATION:%s", _submenuItem == 1 ? ">" : " ", _config->manualCirculationEnabled ? "ON" : "OFF");
		strcpy(this->lines[1], hr);
		sprintf(hr, "%sREBOOT...", _submenuItem == 2 ? ">" : " ");
		strcpy(this->lines[2], hr);
	break;
 default:
    printMainScreen();
  break;
	}

#ifdef _CPPWIN

#endif

}
void hScreen::nextMenu(int submenuItem)
{
	if (submenuItem >= 0 && submenuItem < 4) 	_submenuItem = submenuItem;  else  _submenuItem = 0;
	if (_submenuItem == 0) {
		_menuItem++;
	//	clearScreen(); 
		if (_menuItem > 4) _menuItem = 0;

	}
}
;

void hScreen::renderScreen(){
  if (_clearScreen){
   _lcd->clear();
  }
    for (int i=0; i<4; i++){
		if (checkLinesChanges(i) || _clearScreen) {
			_lcd->setCursor(0, i);
			_lcd->print(this->getLine(i));
#ifndef _CPPWIN
			Serial.println(this->getLine(i));
#endif // !_CPPWIN
			strcpy(this->_lines[i], this->getLine(i));
		}
    }
   
  _clearScreen = false;  
};

void hScreen::clearScreen(){
  _clearScreen = true;
  for (int i = 0; i < 4; i++) {
	  strcpy(lines[i], _BLANK_LINE);
  }
}
void hScreen::backlight()
{
	if (!_backLight) {
		_backLight = true;
		_lcd->backlight();
	}
}
;

bool hScreen::checkLinesChanges(int lineNumber){
  //for (int i=0; i<4; i++){
    if (sizeof(lines[lineNumber])!=sizeof(_lines[lineNumber])){
      return true;
    //  break;
    }
    if (strcmp(lines[lineNumber],_lines[lineNumber])!=0){
      return true;
    }
  //}
  return false;
};


hDisableBacklight::hDisableBacklight(bool disposable, tm scheduleTime, escheduleType scheduleType, int payload,  hScreen* display) 
	:hCommand(disposable, scheduleTime, scheduleType, payload) 
{
	_lcd = display;
}

bool hDisableBacklight::execute()
{
	_lcd->noBackLight();
	return true;
}
