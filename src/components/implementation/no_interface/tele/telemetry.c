#include <osapi.h>

#include <common_types.h>
#include <cfe_es.h>
#include <cfe_error.h>

#include "telemetry.h"

enum tele_operation {
    DS_SETUP_PACKET_FILTER = 0,
    DS_CLEAR_PACKET_FILTER,
    DS_ENABLE_PACKET_STORAGE,
    DS_DISABLE_PACKET_STORAGE,

    OUTPUT_STORED_PACKETS,
    DELETE_STORED_PACKETS,

    FM_CREATE_TEST_FILE,
    FM_MOVE_TEST_FILE,
    FM_COPY_TEST_FILE,
    FM_DELETE_TEST_FILE,
    FM_DELETE_MOVED_FILE,

    VERIFY_TEST_FILE_EXISTS,
    VERIFY_MOVED_FILE_EXISTS,

    HS_ENABLE_ALIVENESS,
    HS_ENABLE_APPMON,
    HS_DISABLE_ALIVENESS,
    HS_DISABLE_APPMON,

    /* MM probably won't make the cut -- possibly we can set up buffers in its address space? */

    SC_CREATE_DELAYED_FM_COMMAND,
    VERIFY_SC_COMMAND_FIRED
};

#define OPERATION_COUNT 23
enum tele_operation operations[OPERATION_COUNT] = {HS_ENABLE_ALIVENESS, HS_ENABLE_APPMON,

                                                   DS_SETUP_PACKET_FILTER,
                                                   DS_ENABLE_PACKET_STORAGE,

                                                   FM_CREATE_TEST_FILE, VERIFY_TEST_FILE_EXISTS,

                                                   OUTPUT_STORED_PACKETS,

                                                   FM_MOVE_TEST_FILE, VERIFY_MOVED_FILE_EXISTS, FM_DELETE_MOVED_FILE,

                                                   FM_CREATE_TEST_FILE, VERIFY_TEST_FILE_EXISTS,
                                                   FM_COPY_TEST_FILE, VERIFY_TEST_FILE_EXISTS, VERIFY_MOVED_FILE_EXISTS,
                                                   FM_DELETE_TEST_FILE, FM_DELETE_MOVED_FILE,

                                                   DS_CLEAR_PACKET_FILTER,
                                                   DS_DISABLE_PACKET_STORAGE,

                                                   HS_DISABLE_APPMON, HS_DISABLE_ALIVENESS,
                                                   OUTPUT_STORED_PACKETS, DELETE_STORED_PACKETS};

void
PerformOperation()
{
    static unsigned int n = 0;
    enum tele_operation op = operations[n];
    n++;

    /* TODO: Implement a case for each operation in enum tele_operations */
    switch (op) {
        default:
            break;
    }
}

void
TELE_AppMain(void)
{
    int32 Result;
    uint32 RunStatus = CFE_ES_APP_RUN;

    /* Register application... */
    Result = CFE_ES_RegisterApp();

    if (Result != CFE_SUCCESS) {
        OS_printf("TELE APP could not be started\n");
        RunStatus = CFE_ES_APP_ERROR;
    }

    OS_printf("TELE APP started and ready\n");

    while (CFE_ES_RunLoop(&RunStatus)) {
        PerformOperation();
        OS_TaskDelay(2000);
    }
}
