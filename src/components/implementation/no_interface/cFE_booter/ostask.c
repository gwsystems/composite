#include "cFE_util.h"

#include "gen/osapi.h"
#include "gen/common_types.h"


/*
** Task API
*/

int32 OS_TaskCreate(uint32 *task_id, const char *task_name,
                    osal_task_entry function_pointer,
                    uint32 *stack_pointer,
                    uint32 stack_size,
                    uint32 priority, uint32 flags)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TaskDelete(uint32 task_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

void OS_TaskExit(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}

int32 OS_TaskInstallDeleteHandler(osal_task_entry function_pointer){
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TaskDelay(uint32 millisecond)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TaskSetPriority(uint32 task_id, uint32 new_priority)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TaskRegister(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

uint32 OS_TaskGetId(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TaskGetIdByName(uint32 *task_id, const char *task_name)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TaskGetInfo(uint32 task_id, OS_task_prop_t *task_prop)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Main thread waiting API
*/

/*
** OS-specific background thread implementation - waits forever for events to occur.
**
** This should be called from the BSP main routine / initial thread after all other
** board / application initialization has taken place and all other tasks are running.
*/
void OS_IdleLoop(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}

/*
** OS_ApplicationShutdown() provides a means for a user-created thread to request the orderly
** shutdown of the whole system, such as part of a user-commanded reset command.
** This is preferred over e.g. ApplicationExit() which exits immediately and does not
** provide for any means to clean up first.
*/
void OS_ApplicationShutdown(uint8 flag)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
}

/*
** Mutex API
*/

int32 OS_MutSemCreate(uint32 *sem_id, const char *sem_name, uint32 options)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_MutSemGive(uint32 sem_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_MutSemTake(uint32 sem_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_MutSemDelete(uint32 sem_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_MutSemGetIdByName(uint32 *sem_id, const char *sem_name)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_MutSemGetInfo(uint32 sem_id, OS_mut_sem_prop_t *mut_prop)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

/*
** Semaphore API
*/

int32 OS_BinSemCreate(uint32 *sem_id, const char *sem_name,
                      uint32 sem_initial_value, uint32 options)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_BinSemFlush(uint32 sem_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_BinSemGive(uint32 sem_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_BinSemTake(uint32 sem_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_BinSemTimedWait(uint32 sem_id, uint32 msecs)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_BinSemDelete(uint32 sem_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_BinSemGetIdByName(uint32 *sem_id, const char *sem_name)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_BinSemGetInfo(uint32 sem_id, OS_bin_sem_prop_t *bin_prop)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_CountSemCreate(uint32 *sem_id, const char *sem_name,
                        uint32 sem_initial_value, uint32 options)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_CountSemGive(uint32 sem_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_CountSemTake(uint32 sem_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_CountSemTimedWait(uint32 sem_id, uint32 msecs)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_CountSemDelete(uint32 sem_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_CountSemGetIdByName(uint32 *sem_id, const char *sem_name)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_CountSemGetInfo(uint32 sem_id, OS_count_sem_prop_t *count_prop)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
