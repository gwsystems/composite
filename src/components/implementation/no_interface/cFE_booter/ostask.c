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
// Necessary to control the number of created tasks

// TODO: Implement flags
int32 OS_TaskCreate(uint32 *task_id, const char *task_name,
                    osal_task_entry function_pointer,
                    uint32 *stack_pointer,
                    uint32 stack_size,
                    uint32 priority, uint32 flags)
{
    int32 result = OS_SUCCESS;

    sl_cs_enter();

    if(task_id == NULL || task_name == NULL || stack_pointer == NULL){
        result = OS_INVALID_POINTER;
        goto exit;
    }

    // Validate the name
    if(!is_valid_name(task_name)) {
        result = OS_ERR_NAME_TOO_LONG;
        goto exit;
    }

    if(is_name_taken(task_name)) {
        result = OS_ERR_NAME_TAKEN;
        goto exit;
    }

    if(priority > 255 || priority < 1) {
        result = OS_ERR_INVALID_PRIORITY;
        goto exit;
    }

    struct sl_thd* thd = sl_thd_alloc(osal_task_entry_wrapper, function_pointer);
    assert(thd);
    union sched_param sp = {.c = {.type = SCHEDP_PRIO, .value = priority}};
    sl_thd_param_set(thd, sp.v);

    struct sl_thd_policy* policy = sl_mod_thd_policy_get(thd);
    strcpy(policy->osal_task_prop.name, task_name);
    policy->osal_task_prop.creator = OS_TaskGetId();
    policy->osal_task_prop.stack_size = stack_size;
    policy->osal_task_prop.priority = priority;
    policy->osal_task_prop.OStask_id = (uint32) thd->thdid;

    *task_id = (uint32) thd->thdid;

exit:
    sl_cs_exit();
    return result;
}

int32 OS_TaskDelete(uint32 task_id)
{
    int32 result = OS_SUCCESS;

    sl_cs_enter();

    struct sl_thd* thd = sl_thd_lkup(task_id);
    if(!thd) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }


    struct sl_thd_policy* thd_policy =  sl_mod_thd_policy_get(thd);

    osal_task_entry delete_handler = thd_policy->delete_handler;
    if(delete_handler) {
        delete_handler();
    }

    sl_thd_free(thd);

exit:
    sl_cs_exit();

    return result;
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
        // FIXME: This is broken, busy loop for now
        // sl_thd_yield(0);
    }
    return OS_SUCCESS;
}

int32 OS_TaskSetPriority(uint32 task_id, uint32 new_priority)
{
    if(new_priority > 255 || new_priority < 1) {
        return OS_ERR_INVALID_PRIORITY;
    }

    struct sl_thd* thd = sl_thd_lkup(task_id);
    if(!thd) {
        return OS_ERR_INVALID_ID;
    }

    union sched_param sp = {.c = {.type = SCHEDP_PRIO, .value = new_priority}};
    sl_thd_param_set(thd, sp.v);

    return OS_SUCCESS;
}

int32 OS_TaskRegister(void)
{
    // Think it is safe for this to do nothing
    return OS_SUCCESS;
}


int32 OS_TaskGetIdByName(uint32 *task_id, const char *task_name)
{
    // FIXME: Implement this. Left as is so unit tests pass
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_TaskGetInfo(uint32 task_id, OS_task_prop_t *task_prop)
{
    if(!task_prop) {
        return OS_INVALID_POINTER;
    }

    struct sl_thd* thd = sl_thd_lkup(task_id);
    if (!thd) {
        return OS_ERR_INVALID_ID;
    }
    struct sl_thd_policy* thd_policy = sl_mod_thd_policy_get(thd);
    assert(thd_policy);

    // TODO: Consider moving this sequence of calls to a helper function
    *task_prop = thd_policy->osal_task_prop;
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

struct mutex {
    int used;

    uint32 creator;
    uint32 held;
    thdid_t holder;
    char name[OS_MAX_API_NAME];
};

struct mutex mutexes[OS_MAX_BIN_SEMAPHORES];


int32 OS_MutSemCreate(uint32 *sem_id, const char *sem_name, uint32 options)
{
    int32 result = OS_SUCCESS;

    sl_cs_enter();

    if (sem_id == NULL || sem_name == NULL) {
        result = OS_INVALID_POINTER;
        goto exit;
    }

    if (!is_valid_name(sem_name)) {
        result = OS_ERR_NAME_TOO_LONG;
        goto exit;
    }

    int id;
    for (id = 0; id < OS_MAX_MUTEXES; id++) {
        if(!mutexes[id].used) {
            break;
        }
    }
    if (mutexes[id].used) {
        result = OS_ERR_NO_FREE_IDS;
        goto exit;
    }

    mutexes[id].used = TRUE;
    mutexes[id].held = FALSE;
    mutexes[id].creator = sl_thd_curr()->thdid;
    strcpy(mutexes[id].name, sem_name);

exit:
    sl_cs_exit();
    return result;
}

int32 OS_MutSemGive(uint32 sem_id)
{
    int32 result = OS_SUCCESS;

    sl_cs_enter();

    if (sem_id >= OS_MAX_MUTEXES || !mutexes[sem_id].used) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    if (!mutexes[sem_id].held || mutexes[sem_id].holder != sl_thd_curr()->thdid) {
        result = OS_SEM_FAILURE;
        goto exit;
    }

    mutexes[sem_id].held = FALSE;

exit:
    sl_cs_exit();

    return result;
}

int32 OS_MutSemTake(uint32 sem_id)
{
    int32 result = OS_SUCCESS;

    sl_cs_enter();

    if (sem_id >= OS_MAX_MUTEXES) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    while (mutexes[sem_id].held && mutexes[sem_id].used) {
        int holder = mutexes[sem_id].holder;

        /*
         * If we are preempted after the exit, and the holder is no longer holding
         * the critical section, then we will yield to them and possibly waste a
         * time-slice.  This will be fixed the next iteration, as we will see an
         * updated value of the holder, but we essentially lose a timeslice in the
         * worst case.  From a real-time perspective, this is bad, but we're erring
         * on simplicity here.
         */
        sl_cs_exit();
        sl_thd_yield(holder);
        sl_cs_enter();
    }

    if (!mutexes[sem_id].used) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    mutexes[sem_id].held   = TRUE;
    mutexes[sem_id].holder = sl_thd_curr()->thdid;

exit:
    sl_cs_exit();

    return result;
}

int32 OS_MutSemDelete(uint32 sem_id)
{
    int32 result = OS_SUCCESS;
    sl_cs_enter();

    if (sem_id >= OS_MAX_MUTEXES || !mutexes[sem_id].used) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    if (mutexes[sem_id].held) {
        result = OS_SEM_FAILURE;
        goto exit;
    }

    mutexes[sem_id].used = FALSE;

    exit:
    sl_cs_exit();

    return result;
}

int32 OS_MutSemGetIdByName(uint32 *sem_id, const char *sem_name)
{
    int32 result = OS_SUCCESS;

    sl_cs_enter();

    if (sem_id == NULL || sem_name == NULL) {
        result = OS_INVALID_POINTER;
        goto exit;
    }

    if (strlen(sem_name) >= OS_MAX_API_NAME) {
        result = OS_ERR_NAME_TOO_LONG;
        goto exit;
    }

    int i;
    for (i = 0; i < OS_MAX_MUTEXES; i++) {
        if (mutexes[i].used && (strcmp (mutexes[i].name, (char*) sem_name) == 0)) {
            *sem_id = i;
            goto exit;
        }
    }

    /* The name was not found in the table,
     *  or it was, and the sem_id isn't valid anymore */
exit:
    sl_cs_exit();
    return result;
}

int32 OS_MutSemGetInfo(uint32 sem_id, OS_mut_sem_prop_t *mut_prop)
{
    int32 result = OS_SUCCESS;
    sl_cs_enter();

    if(!mut_prop)
    {
        result = OS_INVALID_POINTER;
        goto exit;
    }

    if (sem_id >= OS_MAX_MUTEXES || !mutexes[sem_id].used) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    *mut_prop = (OS_mut_sem_prop_t) {
        .creator = mutexes[sem_id].creator,
    };

    strcpy(mut_prop->name, mutexes[sem_id].name);

exit:
    sl_cs_exit();
    return result;
}

/*
** Semaphore API
*/

struct semaphore {
    int used;

    uint32 count;
    int epoch;
    uint32 creator;
    char name[OS_MAX_API_NAME];
};


struct semaphore binary_semaphores[OS_MAX_BIN_SEMAPHORES];

struct semaphore counting_semaphores[OS_MAX_COUNT_SEMAPHORES];

// Generic semaphore methods
int32 OS_SemaphoreCreate(struct semaphore* semaphores, uint32 max_semaphores,
                         uint32 *sem_id, const char *sem_name,
                         uint32 sem_initial_value, uint32 options)
{
    int32 result = OS_SUCCESS;
    sl_cs_enter();

    if (sem_id == NULL || sem_name == NULL) {
        result = OS_INVALID_POINTER;
        goto exit;
    }

    if (!is_valid_name(sem_name)) {
        result = OS_ERR_NAME_TOO_LONG;
        goto exit;
    }

    uint32 id;
    for (id = 0; id < max_semaphores; id++) {
        if(!semaphores[id].used) {
            break;
        }
    }
    if (semaphores[id].used) {
        result = OS_ERR_NO_FREE_IDS;
        goto exit;
    }

    semaphores[id].used = TRUE;
    semaphores[id].creator = sl_thd_curr()->thdid;
    semaphores[id].count = sem_initial_value;
    strcpy(semaphores[id].name, sem_name);

exit:
    sl_cs_exit();
    return result;
}

int32 OS_SemaphoreFlush(struct semaphore* semaphores, uint32 max_semaphores, uint32 sem_id)
{

    int32 result = OS_SUCCESS;

    sl_cs_enter();

    if (sem_id >= max_semaphores || !semaphores[sem_id].used) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    semaphores[sem_id].epoch += 1;

exit:
    sl_cs_exit();
    return result;
}


int32 OS_SemaphoreGive(struct semaphore* semaphores, uint32 max_semaphores, uint32 sem_id)
{
    int32 result = OS_SUCCESS;

    sl_cs_enter();

    if (sem_id >= max_semaphores || !semaphores[sem_id].used) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    // FIXME: Add some checks that the semaphore was actually taken by this thread
    semaphores[sem_id].count += 1;

exit:
    sl_cs_exit();
    return result;

}

int32 OS_SemaphoreTake(struct semaphore* semaphores, uint32 max_semaphores, uint32 sem_id)
{
    int32 result = OS_SUCCESS;

    sl_cs_enter();

    if (sem_id >= max_semaphores) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    int starting_epoch = semaphores[sem_id].epoch;

    while (semaphores[sem_id].used && semaphores[sem_id].count == 0) {
        if(semaphores[sem_id].epoch != starting_epoch) {
            goto exit;
        }
        sl_cs_exit();
        // FIXME: Do an actually sensible yield here!
        sl_thd_yield(0);
        sl_cs_enter();
    }

    if (!semaphores[sem_id].used) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    semaphores[sem_id].count -= 1;

exit:
    sl_cs_exit();

    return result;
}

int32 OS_SemaphoreTimedWait(struct semaphore* semaphores, uint32 max_semaphores,
                            uint32 sem_id, uint32 msecs)
{
    int32 result = OS_SUCCESS;
    cycles_t start_cycles = sl_now();
    microsec_t max_wait = msecs * 1000;

    sl_cs_enter();

    if (sem_id >= max_semaphores) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    while (semaphores[sem_id].used
            && semaphores[sem_id].count == 0
            && sl_cyc2usec(sl_now() - start_cycles) < max_wait) {
        sl_cs_exit();
        // FIXME: Do an actually sensible yield here!
        sl_thd_yield(0);
        sl_cs_enter();
    }

    if (!semaphores[sem_id].used) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    if (semaphores[sem_id].count == 0) {
        result = OS_SEM_TIMEOUT;
        goto exit;
    }

    semaphores[sem_id].count -= 1;

exit:
    sl_cs_exit();

    return result;
}

int32 OS_SemaphoreDelete(struct semaphore* semaphores, uint32 max_semaphores,
                         uint32 sem_id)
{
    int32 result = OS_SUCCESS;
    sl_cs_enter();

    if (sem_id >= max_semaphores || !semaphores[sem_id].used) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    // FIXME: Add smarter checking than this
    if (semaphores[sem_id].count == 0) {
        result = OS_SEM_FAILURE;
        goto exit;
    }

    semaphores[sem_id].used = FALSE;

    exit:
    sl_cs_exit();

    return result;
}

int32 OS_SemaphoreGetIdByName(struct semaphore* semaphores, uint32 max_semaphores,
                              uint32 *sem_id, const char *sem_name)
{
    int32 result = OS_SUCCESS;

    sl_cs_enter();

    if (sem_id == NULL || sem_name == NULL) {
        result = OS_INVALID_POINTER;
        goto exit;
    }

    if (!is_valid_name(sem_name)) {
        result = OS_ERR_NAME_TOO_LONG;
        goto exit;
    }

    uint32 i;
    for (i = 0; i < max_semaphores; i++) {
        if (semaphores[i].used && (strcmp(semaphores[i].name, (char*) sem_name) == 0)) {
            *sem_id = i;
            goto exit;
        }
    }

    /* The name was not found in the table,
     *  or it was, and the sem_id isn't valid anymore */
exit:
    sl_cs_exit();
    return result;
}



// Binary semaphore methods
int32 OS_BinSemCreate(uint32 *sem_id, const char *sem_name,
                      uint32 sem_initial_value, uint32 options)
{
    return OS_SemaphoreCreate(binary_semaphores, OS_MAX_BIN_SEMAPHORES,
                              sem_id, sem_name, sem_initial_value, options);
}

int32 OS_BinSemFlush(uint32 sem_id)
{
    return OS_SemaphoreFlush(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id);
}

int32 OS_BinSemGive(uint32 sem_id)
{
    return OS_SemaphoreGive(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id);
}

int32 OS_BinSemTake(uint32 sem_id)
{
    return OS_SemaphoreTake(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id);
}

int32 OS_BinSemTimedWait(uint32 sem_id, uint32 msecs)
{
    return OS_SemaphoreTimedWait(binary_semaphores, OS_MAX_BIN_SEMAPHORES,
                                 sem_id, msecs);
}

int32 OS_BinSemDelete(uint32 sem_id)
{
    return OS_SemaphoreDelete(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id);
}

int32 OS_BinSemGetIdByName(uint32 *sem_id, const char *sem_name)
{
    return OS_SemaphoreGetIdByName(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id, sem_name);
}

int32 OS_BinSemGetInfo(uint32 sem_id, OS_bin_sem_prop_t *bin_prop)
{
    int32 result = OS_SUCCESS;
    sl_cs_enter();

    if(!bin_prop)
    {
        result = OS_INVALID_POINTER;
        goto exit;
    }

    if (sem_id >= OS_MAX_BIN_SEMAPHORES || !binary_semaphores[sem_id].used) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    *bin_prop = (OS_bin_sem_prop_t) {
        .creator = binary_semaphores[sem_id].creator,
        .value = binary_semaphores[sem_id].count
    };

    strcpy(bin_prop->name, binary_semaphores[sem_id].name);

exit:
    sl_cs_exit();
    return result;
}


int32 OS_CountSemCreate(uint32 *sem_id, const char *sem_name,
                        uint32 sem_initial_value, uint32 options)
{
    return OS_SemaphoreCreate(counting_semaphores, OS_MAX_COUNT_SEMAPHORES,
                              sem_id, sem_name, sem_initial_value, options);
}

int32 OS_CountSemGive(uint32 sem_id)
{
    return OS_SemaphoreGive(counting_semaphores, OS_MAX_COUNT_SEMAPHORES, sem_id);
}

int32 OS_CountSemTake(uint32 sem_id)
{
    return OS_SemaphoreTake(counting_semaphores, OS_MAX_COUNT_SEMAPHORES, sem_id);
}

int32 OS_CountSemTimedWait(uint32 sem_id, uint32 msecs)
{
    return OS_SemaphoreTimedWait(counting_semaphores, OS_MAX_COUNT_SEMAPHORES,
                                 sem_id, msecs);
}

int32 OS_CountSemDelete(uint32 sem_id)
{
    return OS_SemaphoreDelete(counting_semaphores, OS_MAX_COUNT_SEMAPHORES, sem_id);
}

int32 OS_CountSemGetIdByName(uint32 *sem_id, const char *sem_name)
{
    return OS_SemaphoreGetIdByName(counting_semaphores, OS_MAX_COUNT_SEMAPHORES,
                                  sem_id, sem_name);
}

int32 OS_CountSemGetInfo(uint32 sem_id, OS_count_sem_prop_t *count_prop)
{
    int32 result = OS_SUCCESS;
    sl_cs_enter();

    if(!count_prop)
    {
        result = OS_INVALID_POINTER;
        goto exit;
    }

    if (sem_id >= OS_MAX_COUNT_SEMAPHORES || !counting_semaphores[sem_id].used) {
        result = OS_ERR_INVALID_ID;
        goto exit;
    }

    *count_prop = (OS_count_sem_prop_t) {
        .creator = counting_semaphores[sem_id].creator,
        .value = counting_semaphores[sem_id].count
    };

    strcpy(count_prop->name, counting_semaphores[sem_id].name);

exit:
    sl_cs_exit();
    return result;
}
