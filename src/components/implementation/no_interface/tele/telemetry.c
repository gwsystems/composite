#include <osapi.h>

#include <common_types.h>
#include <cfe_es.h>
#include <cfe_error.h>

#include "telemetry.h"

void
TELE_AppMain(void)
{
    int32 Result;
    uint32 RunStatus = CFE_ES_APP_RUN;

    /*
    ** Register application...
    */
    Result = CFE_ES_RegisterApp();

    if (Result != CFE_SUCCESS) {
        OS_printf("tele APP could not be started\n");
        RunStatus = CFE_ES_APP_ERROR;
    }

    OS_printf("tele APP started and ready\n");

    while (CFE_ES_RunLoop(&RunStatus)) {
        OS_TaskDelay(1000);
    }
}
