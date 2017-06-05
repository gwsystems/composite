#include "cFE_util.h"
#include "ostask.h"

#include "gen/osapi.h"
#include "gen/common_types.h"

#include "sl.h"

#include <cos_kernel_api.h>


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

// TODO: Figure out how to check if names are taken
int is_name_taken(const char* name) {
    // for(int i = 0; i < num_tasks; i++) {
    //     thdid_t task_id = task_ids[i];
    //     struct sl_thd_policy* task = sl_mod_thd_policy_get(sl_thd_lkup(task_id));
    //
    //     if(strcmp(task->name, name) == 0) {
    //         return TRUE;
    //     }
    // }
    return FALSE;
}

/*
** Task API
*/

// TODO: Implement flags
int32 OS_TaskCreate(uint32 *task_id, const char *task_name,
                    osal_task_entry function_pointer,
                    uint32 *stack_pointer,
                    uint32 stack_size,
                    uint32 priority, uint32 flags)
{
    sl_cs_enter();

    if(task_name == NULL || stack_pointer == NULL){
        return OS_INVALID_POINTER;
    }

    // Validate the name
    if(!is_valid_name(task_name)) {
        return OS_ERR_NAME_TOO_LONG;
    }

    if(is_name_taken(task_name)) {
        return OS_ERR_NAME_TAKEN;
    }

    if(priority > 255 || priority < 1) {
        return OS_ERR_INVALID_PRIORITY;
    }

    struct sl_thd* thd = sl_thd_alloc(osal_task_entry_wrapper, function_pointer);
    union sched_param sp = {.c = {.type = SCHEDP_PRIO, .value = priority}};
    sl_thd_param_set(thd, sp.v);

    struct sl_thd_policy* policy = sl_mod_thd_policy_get(thd);
    strcpy(policy->osal_task_prop.name, task_name);
    policy->osal_task_prop.creator = OS_TaskGetId();
    policy->osal_task_prop.stack_size = stack_size;
    policy->osal_task_prop.priority = priority;
    policy->osal_task_prop.OStask_id = (uint32) thd->thdid;

    *task_id = (uint32) thd->thdid;

    sl_cs_exit();

    return OS_SUCCESS;
}

int32 OS_TaskDelete(uint32 task_id)
{
    sl_cs_enter();

    struct sl_thd* thd = sl_thd_lkup(task_id);
    if(!thd) {
        return OS_ERR_INVALID_ID;
    }

    struct sl_thd_policy* thd_policy =  sl_mod_thd_policy_get(thd);

    osal_task_entry delete_handler = thd_policy->delete_handler;
    if(delete_handler) {
        delete_handler();
    }

    sl_thd_free(thd);

    sl_cs_exit();

    return OS_SUCCESS;
}

uint32 OS_TaskGetId(void)
{
    struct sl_thd* thd = sl_thd_curr();
    if(!thd) {
        PANIC("Could not get the current thread!");
    }
    return thd->thdid;
}

void OS_TaskExit(void)
{
    OS_TaskDelete(OS_TaskGetId());
    // TODO: Figure out if this is the right thing to do in case of failure
    PANIC("Broken invariant, should be unreacheable!");
}

int32 OS_TaskInstallDeleteHandler(osal_task_entry function_pointer)
{
    sl_mod_thd_policy_get(sl_thd_lkup(OS_TaskGetId()))->delete_handler = function_pointer;
    return OS_SUCCESS;
}

int32 OS_TaskDelay(uint32 millisecond)
{
    // TODO: Use Phanis extension
    cycles_t start_time = sl_now();

    while(sl_cyc2usec(sl_now() - start_time) / 1000 < millisecond)  {
        sl_thd_yield(0);
    }
    return OS_SUCCESS;
}

int32 OS_TaskSetPriority(uint32 task_id, uint32 new_priority)
{
    union sched_param sp = {.c = {.type = SCHEDP_PRIO, .value = new_priority}};
    sl_thd_param_set(sl_thd_lkup(task_id), sp.v);

    return OS_SUCCESS;
}

int32 OS_TaskRegister(void)
{
    // Think it is safe for this to do nothing
    return OS_SUCCESS;
}


int32 OS_TaskGetIdByName(uint32 *task_id, const char *task_name)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TaskGetInfo(uint32 task_id, OS_task_prop_t *task_prop)
{
    // TODO: Consider moving this sequence of calls to a helper function
    *task_prop = sl_mod_thd_policy_get(sl_thd_lkup(OS_TaskGetId()))->osal_task_prop;
    return OS_SUCCESS;
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
