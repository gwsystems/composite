#include "cFE_util.h"
#include "ostask.h"

#include "gen/osapi.h"
#include "gen/common_types.h"

#include "sl.h"

#include <cos_kernel_api.h>

// TODO: sl_policy thread
struct os_task {
    int non_empty;
    thdid_t thdid;
    char name[OS_MAX_API_NAME];
};
struct os_task tasks[OS_MAX_TASKS];

/*
** Internal Task helper functions
*/
void OS_SchedulerStart(cos_thd_fn_t main_delegate) {
    sl_init();

    struct sl_thd* main_delegate_thread = sl_thd_alloc(main_delegate, NULL);
    union sched_param sp = {.c = {.type = SCHEDP_PRIO, .value = 1}};
    sl_thd_param_set(main_delegate_thread, sp.v);


    sl_sched_loop();
}

void osal_task_entry_wrapper(void* task_entry) {
    ((osal_task_entry) task_entry)();
}

int is_valid_name(const char* name) {
    int i;
    for(i = 0; i < OS_MAX_API_NAME; i++) {
        if(name[i] == '\0') {
            return TRUE;
        }
    }
    return FALSE;
}

/*
** Task API
*/
// FIXME: Put all these within a critical section...

int32 OS_TaskCreate(uint32 *task_id, const char *task_name,
                    osal_task_entry function_pointer,
                    uint32 *stack_pointer,
                    uint32 stack_size,
                    uint32 priority, uint32 flags)
{
    if(task_name == NULL || stack_pointer == NULL){
        return OS_INVALID_POINTER;
    }

    // Validate the name
    if(!is_valid_name(task_name)) {
        return OS_ERR_NAME_TOO_LONG;
    }

    if(priority > 255 || priority < 1) {
        return OS_ERR_INVALID_PRIORITY;
    }

    int i;
    int selected_task_id = -1;
    for(i = 0; i < OS_MAX_TASKS; i++) {
        if(tasks[i].non_empty) {
            if(strcmp(tasks[i].name, task_name) == 0) {
                return OS_ERR_NAME_TAKEN;
            }
        }else if(selected_task_id == -1){
            selected_task_id = i;
        }
    }
    if(selected_task_id == -1) {
        return OS_ERR_NO_FREE_IDS;
    }


    struct sl_thd* thd = sl_thd_alloc(osal_task_entry_wrapper, function_pointer);
    union sched_param sp = {.c = {.type = SCHEDP_PRIO, .value = priority}};
    sl_thd_param_set(thd, sp.v);


    tasks[selected_task_id] = (struct os_task) {
        .non_empty = TRUE,
        .thdid = thd->thdid
    };
    strcpy(tasks[selected_task_id].name, task_name);

    *task_id = selected_task_id;

    return OS_SUCCESS;
}

int32 OS_TaskDelete(uint32 task_id)
{
    if(task_id > OS_MAX_TASKS || !tasks[task_id].non_empty) {
        return OS_ERR_INVALID_ID;
    }

    int delete_id = tasks[task_id].thdid;
    tasks[task_id].non_empty = FALSE;

    struct sl_thd* thd = sl_thd_lkup(delete_id);
    if(!thd) {
        return OS_ERROR;
    }
    sl_thd_free(thd);

    return OS_SUCCESS;
}

uint32 OS_TaskGetId(void)
{
    struct sl_thd* thd = sl_thd_curr();
    thdid_t t = thd->thdid;
    int i;
    for(i = 0; i < OS_MAX_TASKS; i++) {
        if(tasks[i].thdid == t) {
            return i;
        }
    }

    PANIC("Broken invariant, should be unreacheable!");
    return 0;
}

void OS_TaskExit(void)
{
    OS_TaskDelete(OS_TaskGetId());
    // TODO: Figure out if this is the right thing to do in case of failure
    PANIC("Broken invariant, should be unreacheable!");
}

int32 OS_TaskInstallDeleteHandler(osal_task_entry function_pointer){
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TaskDelay(uint32 millisecond)
{
    // TODO: Use Phanis extension
    // Meanwhile: Busy loop yielding
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TaskSetPriority(uint32 task_id, uint32 new_priority)
{
    if(task_id > OS_MAX_TASKS || !tasks[task_id].non_empty) {
        return OS_ERR_INVALID_ID;
    }

    if(new_priority < 1 || new_priority > 255) {
        return OS_ERR_INVALID_PRIORITY;
    }

    union sched_param sp = {.c = {.type = SCHEDP_PRIO, .value = new_priority}};
    sl_thd_param_set(sl_thd_lkup(tasks[task_id].thdid), sp.v);

    return OS_SUCCESS;
}

int32 OS_TaskRegister(void)
{
    // Think it is safe for this to do nothing
    return OS_SUCCESS;
}


int32 OS_TaskGetIdByName(uint32 *task_id, const char *task_name)
{
    if(!task_name) {
        return OS_INVALID_POINTER;
    }

    if(!is_valid_name(task_name)) {
        return OS_ERR_NAME_TOO_LONG;
    }

    int i;
    for(i = 0; i < OS_MAX_TASKS; i++) {
        if(tasks[i].non_empty && strcmp(tasks[i].name, task_name) == 0) {
            *task_id = i;
            return OS_SUCCESS;
        }
    }

    return OS_ERR_NAME_NOT_FOUND;
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
