#include <time.h>

#include "cFE_util.h"

#include "sl.h"
#include "sl_consts.h"

#include "gen/osapi.h"
#include "gen/cfe_psp.h"
#include "gen/common_types.h"


#include <cos_component.h>
#include <cos_defkernel_api.h>


/*
** Initialization of API
*/
int32 OS_API_Init(void)
{
    struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
    struct cos_compinfo    *ci    = cos_compinfo_get(defci);

    cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
    cos_defcompinfo_init();

    return OS_SUCCESS;
}

/*
** OS_DeleteAllObjects() provides a means to clean up all resources allocated by this
** instance of OSAL.  It would typically be used during an orderly shutdown but may also
** be helpful for testing purposes.
*/
void OS_DeleteAllObjects(void)
{
    // It's safe for this method to do nothing for now
}



/*
** OS Time/Tick related API
*/

int32 OS_Milli2Ticks(uint32 milli_seconds)
{
    return (int32) (CFE_PSP_GetTimerTicksPerSecond() * milli_seconds) / 1000;
}

int32 OS_Tick2Micros(void)
{
    return SL_PERIOD_US;
}

OS_time_t local_time;
cycles_t old_cycle_count;

OS_time_t OS_AdvanceTime(OS_time_t initial_time, microsec_t usec) {
    microsec_t old_seconds = (microsec_t) initial_time.seconds;
    microsec_t old_additional_usec = (microsec_t) initial_time.microsecs;

    microsec_t old_usec = old_seconds * (1000 * 1000) + old_additional_usec;
    microsec_t new_usec = old_usec + usec;

    microsec_t new_seconds = new_usec / (1000 * 1000);
    microsec_t new_additional_usec = new_usec % (1000 * 1000);

    return (OS_time_t) {
        .seconds = new_seconds,
        .microsecs = new_additional_usec
    };
}

int32 OS_GetLocalTime(OS_time_t *time_struct)
{
    if(old_cycle_count == 0) {
        local_time = (OS_time_t) {
            .seconds = 1181683060,
            .microsecs = 0
        };
        old_cycle_count = sl_now();
    } else {
        cycles_t new_cycle_count = sl_now();
        cycles_t elapsed_cycles = new_cycle_count - old_cycle_count;

        microsec_t elapsed_usec = sl_cyc2usec(elapsed_cycles);

        local_time = OS_AdvanceTime(local_time, elapsed_usec);
        old_cycle_count = new_cycle_count;
    }

    *time_struct = local_time;

    return OS_SUCCESS;

}/* end OS_GetLocalTime */

int32 OS_SetLocalTime(OS_time_t *time_struct)
{
    local_time = *time_struct;
    old_cycle_count = sl_now();

    return OS_SUCCESS;
} /*end OS_SetLocalTime */

/*
** Exception API
*/

int32 OS_ExcAttachHandler(uint32 ExceptionNumber,
                          void (*ExceptionHandler)(uint32, const void *,uint32),
                          int32 parameter)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}


int32 OS_ExcEnable             (int32 ExceptionNumber)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ExcDisable            (int32 ExceptionNumber)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Floating Point Unit API
*/

int32 OS_FPUExcAttachHandler(uint32 ExceptionNumber, void * ExceptionHandler,
                             int32 parameter)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_FPUExcEnable(int32 ExceptionNumber)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_FPUExcDisable(int32 ExceptionNumber)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_FPUExcSetMask(uint32 mask)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_FPUExcGetMask(uint32 *mask)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Interrupt API
*/
int32 OS_IntAttachHandler(uint32 InterruptNumber, osal_task_entry InterruptHandler, int32 parameter)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntUnlock(int32 IntLevel)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntLock(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntEnable(int32 Level)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntDisable(int32 Level)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntSetMask(uint32 mask)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntGetMask(uint32 *mask)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntAck(int32 InterruptNumber)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Shared memory API
*/
int32 OS_ShMemInit(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ShMemCreate(uint32 *Id, uint32 NBytes, const char* SegName)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ShMemSemTake(uint32 Id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ShMemSemGive(uint32 Id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ShMemAttach(cpuaddr * Address, uint32 Id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ShMemGetIdByName(uint32 *ShMemId, const char *SegName )
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Heap API
*/
int32 OS_HeapGetInfo(OS_heap_prop_t *heap_prop)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** API for useful debugging function
*/

// Implementation stolen from the posix os api
int32 OS_GetErrorName(int32 error_num, os_err_name_t* err_name)
{
    /*
     * Implementation note for developers:
     *
     * The size of the string literals below (including the terminating null)
     * must fit into os_err_name_t.  Always check the string length when
     * adding or modifying strings in this function.  If changing os_err_name_t
     * then confirm these strings will fit.
     */

    os_err_name_t local_name;
    uint32        return_code = OS_SUCCESS;

    if ( err_name == NULL )
    {
       return(OS_INVALID_POINTER);
    }

    switch (error_num)
    {
        case OS_SUCCESS:
            strcpy(local_name,"OS_SUCCESS"); break;
        case OS_ERROR:
            strcpy(local_name,"OS_ERROR"); break;
        case OS_INVALID_POINTER:
            strcpy(local_name,"OS_INVALID_POINTER"); break;
        case OS_ERROR_ADDRESS_MISALIGNED:
            strcpy(local_name,"OS_ADDRESS_MISALIGNED"); break;
        case OS_ERROR_TIMEOUT:
            strcpy(local_name,"OS_ERROR_TIMEOUT"); break;
        case OS_INVALID_INT_NUM:
            strcpy(local_name,"OS_INVALID_INT_NUM"); break;
        case OS_SEM_FAILURE:
            strcpy(local_name,"OS_SEM_FAILURE"); break;
        case OS_SEM_TIMEOUT:
            strcpy(local_name,"OS_SEM_TIMEOUT"); break;
        case OS_QUEUE_EMPTY:
            strcpy(local_name,"OS_QUEUE_EMPTY"); break;
        case OS_QUEUE_FULL:
            strcpy(local_name,"OS_QUEUE_FULL"); break;
        case OS_QUEUE_TIMEOUT:
            strcpy(local_name,"OS_QUEUE_TIMEOUT"); break;
        case OS_QUEUE_INVALID_SIZE:
            strcpy(local_name,"OS_QUEUE_INVALID_SIZE"); break;
        case OS_QUEUE_ID_ERROR:
            strcpy(local_name,"OS_QUEUE_ID_ERROR"); break;
        case OS_ERR_NAME_TOO_LONG:
            strcpy(local_name,"OS_ERR_NAME_TOO_LONG"); break;
        case OS_ERR_NO_FREE_IDS:
            strcpy(local_name,"OS_ERR_NO_FREE_IDS"); break;
        case OS_ERR_NAME_TAKEN:
            strcpy(local_name,"OS_ERR_NAME_TAKEN"); break;
        case OS_ERR_INVALID_ID:
            strcpy(local_name,"OS_ERR_INVALID_ID"); break;
        case OS_ERR_NAME_NOT_FOUND:
            strcpy(local_name,"OS_ERR_NAME_NOT_FOUND"); break;
        case OS_ERR_SEM_NOT_FULL:
            strcpy(local_name,"OS_ERR_SEM_NOT_FULL"); break;
        case OS_ERR_INVALID_PRIORITY:
            strcpy(local_name,"OS_ERR_INVALID_PRIORITY"); break;

        default: strcpy(local_name,"ERROR_UNKNOWN");
                 return_code = OS_ERROR;
    }

    strcpy((char*) err_name, local_name);

    return return_code;
}


/*
** Abstraction for printf statements
*/
int is_printf_enabled = TRUE;

void OS_printf(const char *string, ...)
{
    if(is_printf_enabled) {
        char s[OS_BUFFER_SIZE];
        va_list arg_ptr;
        int ret, len = OS_BUFFER_SIZE;

        va_start(arg_ptr, string);
        ret = vsnprintf(s, len, string, arg_ptr);
        va_end(arg_ptr);
        llprint(s, ret);
    }
}


void OS_printf_disable(void)
{
    is_printf_enabled = FALSE;
}

void OS_printf_enable(void)
{
    is_printf_enabled = TRUE;
}

/*
** Call to exit the running application
** Normally embedded applications run forever, but for debugging purposes
** (unit testing for example) this is needed in order to end the test
*/
void OS_ApplicationExit(int32 Status)
{
    PANIC("Application exit invoked!");
}
