#include "cFE_util.h"

#include "gen/osapi.h"
#include "gen/common_types.h"

/*
** Initialization of API
*/
int32 OS_API_Init(void)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** OS-specific background thread implementation - waits forever for events to occur.
**
** This should be called from the BSP main routine / initial thread after all other
** board / application initialization has taken place and all other tasks are running.
*/
void OS_IdleLoop(void)
{
    panic("Unimplemented method!"); // TODO: Implement me!
}

/*
** OS_DeleteAllObjects() provides a means to clean up all resources allocated by this
** instance of OSAL.  It would typically be used during an orderly shutdown but may also
** be helpful for testing purposes.
*/
void OS_DeleteAllObjects(void)
{
    panic("Unimplemented method!"); // TODO: Implement me!
}

/*
** OS_ApplicationShutdown() provides a means for a user-created thread to request the orderly
** shutdown of the whole system, such as part of a user-commanded reset command.
** This is preferred over e.g. ApplicationExit() which exits immediately and does not
** provide for any means to clean up first.
*/
void OS_ApplicationShutdown(uint8 flag)
{
    panic("Unimplemented method!"); // TODO: Implement me!
}

/*
** OS Time/Tick related API
*/

int32 OS_Milli2Ticks(uint32 milli_seconds)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_Tick2Micros(void)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32  OS_GetLocalTime(OS_time_t *time_struct)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32  OS_SetLocalTime(OS_time_t *time_struct)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Exception API
*/

int32 OS_ExcAttachHandler(uint32 ExceptionNumber,
                          void (*ExceptionHandler)(uint32, const void *,uint32),
                          int32 parameter)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}


int32 OS_ExcEnable             (int32 ExceptionNumber)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ExcDisable            (int32 ExceptionNumber)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Floating Point Unit API
*/

int32 OS_FPUExcAttachHandler(uint32 ExceptionNumber, void * ExceptionHandler,
                             int32 parameter)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_FPUExcEnable(int32 ExceptionNumber)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_FPUExcDisable(int32 ExceptionNumber)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_FPUExcSetMask(uint32 mask)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_FPUExcGetMask(uint32 *mask)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Interrupt API
*/
int32 OS_IntAttachHandler(uint32 InterruptNumber, osal_task_entry InterruptHandler, int32 parameter)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntUnlock(int32 IntLevel)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntLock(void)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntEnable(int32 Level)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntDisable(int32 Level)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntSetMask(uint32 mask)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntGetMask(uint32 *mask)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_IntAck(int32 InterruptNumber)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Shared memory API
*/
int32 OS_ShMemInit(void)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ShMemCreate(uint32 *Id, uint32 NBytes, const char* SegName)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ShMemSemTake(uint32 Id)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ShMemSemGive(uint32 Id)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ShMemAttach(cpuaddr * Address, uint32 Id)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ShMemGetIdByName(uint32 *ShMemId, const char *SegName )
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Heap API
*/
int32 OS_HeapGetInfo(OS_heap_prop_t *heap_prop)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** API for useful debugging function
*/
int32 OS_GetErrorName(int32 error_num, os_err_name_t* err_name)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}


/*
** Abstraction for printf statements
*/
void OS_printf( const char *string, ...) {
    panic("Unimplemented method!"); // TODO: Implement me!
}

void OS_printf_disable(void)
{
    panic("Unimplemented method!"); // TODO: Implement me!
}

void OS_printf_enable(void)
{
    panic("Unimplemented method!"); // TODO: Implement me!
}

/*
** Call to exit the running application
** Normally embedded applications run forever, but for debugging purposes
** (unit testing for example) this is needed in order to end the test
*/
void OS_ApplicationExit(int32 Status)
{
    panic("Unimplemented method!"); // TODO: Implement me!
}
