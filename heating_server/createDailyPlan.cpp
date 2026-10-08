#include <time.h>
#include "scheduler.h"

bool createPlanForDomesticWaterPump(hPumpsController *heatPumpController)
{
    tm scht = {};
    bool accepted = true;
    //create normal daily plan for  domestic hot water circulation pump
    scht.tm_hour = 5;
    scht.tm_min = 0;
    accepted = heatPumpController->turnOnDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 5;
    scht.tm_min = 30;
    accepted = heatPumpController->turnOffDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 6;
    scht.tm_min = 0;
    accepted = heatPumpController->turnOnDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 6;
    scht.tm_min = 30;
    accepted = heatPumpController->turnOffDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 7;
    scht.tm_min = 0;
    accepted = heatPumpController->turnOnDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 7;
    scht.tm_min = 30;
    accepted = heatPumpController->turnOffDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 12;
    scht.tm_min = 0;
    accepted = heatPumpController->turnOnDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 12;
    scht.tm_min = 30;
    accepted = heatPumpController->turnOffDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 16;
    scht.tm_min = 0;
    accepted = heatPumpController->turnOnDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 16;
    scht.tm_min = 30;
    accepted = heatPumpController->turnOffDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 19;
    scht.tm_min = 0;
    accepted = heatPumpController->turnOnDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 19;
    scht.tm_min = 30;
    accepted = heatPumpController->turnOffDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 21;
    scht.tm_min = 0;
    accepted = heatPumpController->turnOnDomesticWaterPumpReq(scht) && accepted;
    scht.tm_hour = 21;
    scht.tm_min = 30;
    accepted = heatPumpController->turnOffDomesticWaterPumpReq(scht) && accepted;
    return accepted;
}
