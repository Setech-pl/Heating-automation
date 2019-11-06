#include <time.h>
#include "scheduler.h"

void createPlanForDomesticWaterPump(hPumpsController *heatPumpController)
{
    tm scht;
    //create normal daily plan for  domestic hot water circulation pump
    scht.tm_hour = 5;
    scht.tm_min = 0;
    heatPumpController->turnOnDomesticWaterPumpReq(scht);
    scht.tm_hour = 5;
    scht.tm_min = 30;
    heatPumpController->turnOffDomesticWaterPumpReq(scht);
    scht.tm_hour = 6;
    scht.tm_min = 0;
    heatPumpController->turnOnDomesticWaterPumpReq(scht);
    scht.tm_hour = 6;
    scht.tm_min = 30;
    heatPumpController->turnOffDomesticWaterPumpReq(scht);
    scht.tm_hour = 7;
    scht.tm_min = 0;
    heatPumpController->turnOnDomesticWaterPumpReq(scht);
    scht.tm_hour = 7;
    scht.tm_min = 30;
    heatPumpController->turnOffDomesticWaterPumpReq(scht);
    scht.tm_hour = 12;
    scht.tm_min = 0;
    heatPumpController->turnOnDomesticWaterPumpReq(scht);
    scht.tm_hour = 12;
    scht.tm_min = 30;
    heatPumpController->turnOffDomesticWaterPumpReq(scht);
    scht.tm_hour = 16;
    scht.tm_min = 0;
    heatPumpController->turnOnDomesticWaterPumpReq(scht);
    scht.tm_hour = 16;
    scht.tm_min = 30;
    heatPumpController->turnOffDomesticWaterPumpReq(scht);
    scht.tm_hour = 19;
    scht.tm_min = 0;
    heatPumpController->turnOnDomesticWaterPumpReq(scht);
    scht.tm_hour = 19;
    scht.tm_min = 30;
    heatPumpController->turnOffDomesticWaterPumpReq(scht);
    scht.tm_hour = 21;
    scht.tm_min = 0;
    heatPumpController->turnOnDomesticWaterPumpReq(scht);
    scht.tm_hour = 21;
    scht.tm_min = 30;
    heatPumpController->turnOffDomesticWaterPumpReq(scht);
}
