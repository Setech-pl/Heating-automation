#include "heating_config.h"
#include "utils.h"
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "scheduler.h"
#include <ESP8266WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include "screen.h"
#include <ArduinoJson.h>
#include "UDPMessengerService.h"



LiquidCrystal_I2C lcd(0x27,20,4);

hScheduler* scheduler  = new hScheduler();
hConfigurator* config = new hConfigurator();
hPumpsController* heatPumpController  = new hPumpsController(scheduler, config);
hScreen* hdisplay= new hScreen(&lcd, config);
UDPMessengerService udpMessenger(3636);
unsigned long timeMillis = 0;



void hook_discover_devices(){
  udpMessenger.discoverDevices();    
}

void setup() {
  int counter=0;
  bool wynik = false; 
  char temp[21];
  tm t;  
  connect_external_wifi connExternalWiFi(true,t,hourly,0);     
  ntp_update ntpUpdateCommand(true,t,hourly,0);     
  lcd.init();
  lcd.backlight(); // Enable or Turn On the backlight 
  hdisplay->printSplashScreen();
  hdisplay->renderScreen(); 
  delay(1000);    
  while (!wynik && counter<5){
    sprintf(temp,"Connecting WiFi(%d)",counter);
    hdisplay->printStatusBar(temp);            
    hdisplay->renderScreen();    
    wynik = connExternalWiFi.execute();    
	counter++;
    delay(200);
  } 
  hdisplay->printStatusBar(connExternalWiFi.result);            
  hdisplay->renderScreen();  
  delay(1000);  
  if (!wynik){
	// I have to turn on internal WiFi
	enable_internal_wifi internalWifiCmd(true,t,hourly,0);
	counter = 0;
	while(!wynik && counter < 5){
	    sprintf(temp,"Internal WiFi(%d)",counter);
	    hdisplay->printStatusBar(temp);            
	    hdisplay->renderScreen();    
	    wynik = internalWifiCmd.execute();  
		counter++;
    delay(200);

	}

  }
  counter=0;
  wynik=false;
  //Updating time from NTP time server
  while (!wynik && counter<5){;
    sprintf(temp,"Updating NTP(%d)  ",counter);
    hdisplay->printStatusBar(temp);        
    hdisplay->renderScreen();
    counter++;
    wynik=ntpUpdateCommand.execute();
    delay(200);
  }
  hdisplay->printStatusBar(ntpUpdateCommand.result);    
  hdisplay->renderScreen();
  delay(1000);  
// if _INTERNAL_WIFI_MODE == true then enable internal wifi

  hook_discover_devices();
  char hr[21];
  sprintf(hr," IP : %d.%d.%d.%d", WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);      
  hdisplay->printStatusBar(hr);  
  hdisplay->renderScreen();
  delay(1000);    
  timeMillis=0;
// add periodical device discovery process
  t.tm_hour = hour();
  t.tm_min = 1;
  t.tm_mday = day();
  t.tm_wday = weekday();
 // scheduler->addTask(new hCallbackCommand(false, t,hourly, &hook_discover_devices));
  t.tm_min=31;
 // scheduler->addTask(new hCallbackCommand(false, t,hourly, &hook_discover_devices));  
}



void loop() {
  if (timeMillis+1000< millis()){
    scheduler->executeTasks();    
    config->tickMinutes();
  }
  if (timeMillis+5000< millis()){
    /*
    hdisplay->nextMenu(10);
    hdisplay->printMenu();
    hdisplay->renderScreen();
    hdisplay->clearScreen();
    timeMillis = millis();    
    */
    hdisplay->printMainScreen();    
    hdisplay->printStatusBar(_BLANK_LINE);
    hdisplay->renderScreen();
    timeMillis=millis();
  }
  udpMessenger.listen();

  //Incoming commands router

  if (udpMessenger.checkNewCommand()){
    tClientCommand temp = udpMessenger.getCurrentCommand();
    if (strcmp(temp.cmd,"ON")==0){
          if (temp.ID>=0 && temp.ID<4){             
            char tm[20];
            sprintf(tm,"C%d heating ON",temp.ID+1);
            hdisplay->printStatusBar(tm);
            hdisplay->renderScreen();
            heatPumpController->turnOnHeatPumpReq(temp.ID,temp.actualTEMP,temp.targetTEMP);
            timeMillis=millis();
          }
    }    
    if (strcmp(temp.cmd,"OFF")==0){ 
          if (temp.ID>=0 && temp.ID<_MAX_HEATING_PUMPS_NO ){       
            char tm[20];
            sprintf(tm,"C%d heating OFF",temp.ID+1);
            hdisplay->printStatusBar(tm);    
            hdisplay->renderScreen();
            heatPumpController->turnOffHeatPumpReq(temp.ID,temp.actualTEMP,temp.targetTEMP);      
            timeMillis=millis();          
          }
    } 
    if (strcmp(temp.cmd,"SHOWSERVER")==0){
          char tm[20];
          sprintf(tm,"Registering C%d",temp.ID+1);  
          hdisplay->printStatusBar(tm);    
          hdisplay->renderScreen();  
          hook_discover_devices();                        
    }
  }

}
