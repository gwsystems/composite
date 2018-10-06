#include <cos_kernel_api.h>
#include <schedinit.h>
#include <sl.h>
#include <sl_lock.h>

#include <cFE_emu.h>

#include "cFE_util.h"
#include "ostask.h"

#include "gen/osapi.h"
#include "gen/common_types.h"
#include "gen/cfe_time.h"

#include <cos_time.h>
#include <event_trace.h>

extern int number_apps;

void
timer_fn_1hz(void *d)
{
	cycles_t start_time = 0, next_deadline = 0, interval = time_usec2cyc(HZ_PAUSE_US);
	unsigned int timer_counter = 0;
	OS_time_t time;

	rdtscll(start_time);
	next_deadline = start_time;

	while (1) {
		cycles_t now, elapsed;

		next_deadline += interval;
		elapsed = sl_thd_block_timeout(0, next_deadline);

		OS_GetLocalTime(&time);
		while (elapsed > interval) {
			CFE_TIME_Local1HzISR();

			elapsed -= interval;
		}

		CFE_TIME_Local1HzISR();
	}
}

/*
 * Internal Task helper functions
 */

/*  We need to keep track of this to check if register or delete handler calls are invalid */
thdid_t main_delegate_thread_id;

struct cfe_task_info {
	osal_task_entry delete_handler;
	OS_task_prop_t  osal_task_prop;
};

struct cfe_task_info cfe_tasks[SL_MAX_NUM_THDS] = {{0}};
struct sl_thd *sensoremu_thd = NULL;
extern void CFE_PSP_SensorISR(arcvcap_t, void *);

void
OS_SchedulerStart(cos_thd_fn_t main_delegate)
{
	struct sl_thd *       main_delegate_thread;
	sched_param_t         sp;
	struct cfe_task_info *task_info;

	sl_init(SCHED_PERIOD_US);
	sl_usage_enable();

	main_delegate_thread = sl_thd_alloc(main_delegate, NULL);
	sp                   = sched_param_pack(SCHEDP_PRIO, MAIN_DELEGATE_THREAD_PRIORITY);
	sl_thd_param_set(main_delegate_thread, sp);
	main_delegate_thread_id = sl_thd_thdid(main_delegate_thread);

	task_info = &cfe_tasks[main_delegate_thread_id];
	strcpy(task_info->osal_task_prop.name, "MAIN_THREAD");
	task_info->osal_task_prop.priority  = MAIN_DELEGATE_THREAD_PRIORITY;
	task_info->osal_task_prop.OStask_id = (uint32)sl_thd_thdid(main_delegate_thread);

	sensoremu_thd = sl_thd_aep_alloc(CFE_PSP_SensorISR, NULL, 1, CFE_HPET_SH_KEY, 0, 0);
	assert(sensoremu_thd);
	schedinit_child();
	PRINTC("Done with initialization. Starting scheduling loop\n");
#ifdef CFE_RK_MULTI_CORE
	sl_sched_loop_nonblock();
#else
	sl_sched_loop();
#endif
}

void
osal_task_entry_wrapper(void *task_entry)
{
	((osal_task_entry)task_entry)();
}

int
is_valid_name(const char *name)
{
	int i;
	for (i = 0; i < OS_MAX_API_NAME; i++) {
		if (name[i] == '\0') { return TRUE; }
	}
	return FALSE;
}

/* TODO: Figure out how to check if thread names are taken */
int
is_thread_name_taken(const char *name)
{
	return FALSE;
}

/*
 * Task API
 */

/* NOTE: We don't do flags, but I can't find an implementation that does */
int32
OS_TaskCreate(uint32 *task_id, const char *task_name, osal_task_entry function_pointer, uint32 *stack_pointer,
              uint32 stack_size, uint32 priority, uint32 flags)
{
	struct sl_thd *       thd;
	sched_param_t         sp;
	struct cfe_task_info *task_info;
	struct cos_defcompinfo child_dci;

	if (task_id == NULL || task_name == NULL || function_pointer == NULL) { return OS_INVALID_POINTER; }

	if (!is_valid_name(task_name)) { return OS_ERR_NAME_TOO_LONG; }

	if (is_thread_name_taken(task_name)) { return OS_ERR_NAME_TAKEN; }

	if (priority > 255 || priority < 1) { return OS_ERR_INVALID_PRIORITY; }

	/* If the create call is rooted in another component, STASH_MAGIC_VALUE will be passed as the function_pointer  */
	if (function_pointer == STASH_MAGIC_VALUE) {
		/* Since we know this is rooted in another component, we take the values from the stash */
		thdclosure_index_t idx = emu_stash_retrieve_thdclosure();
		spdid_t spdid = emu_stash_retrieve_spdid();

		printc("task create in server (task_name = %s, fp = %p, idx = %d, spdid = %d)\n", task_name, function_pointer, idx, spdid);

		cos_defcompinfo_childid_init(&child_dci, spdid);

		thd = sl_thd_aep_alloc_ext(&child_dci, NULL, idx, 0, 0, 0, 0, 0, NULL);
		assert(thd);
		emu_stash_set_thdid(sl_thd_thdid(thd));
	} else {
		thd = sl_thd_alloc(osal_task_entry_wrapper, function_pointer);
		assert(thd);
	}

	sp = sched_param_pack(SCHEDP_PRIO, priority);
	sl_thd_param_set(thd, sp);

	task_info = &cfe_tasks[sl_thd_thdid(thd)];
	strcpy(task_info->osal_task_prop.name, task_name);
	task_info->osal_task_prop.creator    = OS_TaskGetId();
	task_info->osal_task_prop.stack_size = stack_size;
	task_info->osal_task_prop.priority   = priority;
	task_info->osal_task_prop.OStask_id  = (uint32)sl_thd_thdid(thd);
	task_info->delete_handler            = NULL;

	*task_id = (uint32)sl_thd_thdid(thd);

	return OS_SUCCESS;
}

int32
OS_TaskDelete(uint32 task_id)
{
	struct cfe_task_info *task_info;
	struct sl_thd *       thd;
	osal_task_entry       delete_handler;

	thd = sl_thd_lkup(task_id);
	if (!thd) { return OS_ERR_INVALID_ID; }
	/* FIXME: Need to handle the deletion of a thread pretending to be another thread  */
	if (thd->state == SL_THD_FREE) { return OS_SUCCESS; }

	task_info = &cfe_tasks[task_id];

	delete_handler = task_info->delete_handler;
	if (delete_handler) { delete_handler(); }

	sl_thd_free(thd);

	return OS_SUCCESS;
}

uint32
OS_TaskGetId(void)
{
	thdid_t real_id = sl_thdid();
	/* Sometimes we need to disguise a thread as another thread... */
	thdid_t possible_override = id_overrides[real_id];
	if (possible_override) return possible_override;
	return real_id;
}

void
OS_TaskExit(void)
{
	sl_thd_free(sl_thd_curr());
	PANIC("Should be unreachable!");
}

int32
OS_TaskInstallDeleteHandler(osal_task_entry function_pointer)
{
	struct cfe_task_info *task_info;

	if (OS_TaskGetId() == main_delegate_thread_id) { return OS_ERR_INVALID_ID; }

	task_info                 = &cfe_tasks[sl_thd_thdid(sl_thd_curr())];
	task_info->delete_handler = function_pointer;

	return OS_SUCCESS;
}

int32
OS_TaskDelay(uint32 millisecond)
{
	cycles_t wakeup = sl_now() + sl_usec2cyc(millisecond * 1000);

	/* do not disturb.. zzzz... */
	while (sl_now() < wakeup) sl_thd_block_timeout(0, wakeup);

	return OS_SUCCESS;
}

int32
OS_TaskSetPriority(uint32 task_id, uint32 new_priority)
{
	struct sl_thd *thd;
	sched_param_t  sp;

	if (new_priority > 255 || new_priority < 1) { return OS_ERR_INVALID_PRIORITY; }

	thd = sl_thd_lkup(task_id);
	if (!thd) { return OS_ERR_INVALID_ID; }

	sp = sched_param_pack(SCHEDP_PRIO, new_priority);
	sl_thd_param_set(thd, sp);

	return OS_SUCCESS;
}

int32
OS_TaskRegister(void)
{
	if (OS_TaskGetId() == main_delegate_thread_id) { return OS_ERR_INVALID_ID; }

	return OS_SUCCESS;
}


int32
OS_TaskGetIdByName(uint32 *task_id, const char *task_name)
{
	thdid_t i;

	if (!task_id || !task_name) return OS_INVALID_POINTER;

	for (i = 1; i < SL_MAX_NUM_THDS; i++) {
		struct sl_thd *thd = sl_thd_lkup(i);
		if (!thd || thd->state == SL_THD_FREE) continue;
		if (strcmp(cfe_tasks[i].osal_task_prop.name, task_name) == 0) {
			*task_id = i;
			return OS_SUCCESS;
		}
	}

	return OS_ERR_NAME_NOT_FOUND;
}

int32
OS_TaskGetInfo(uint32 task_id, OS_task_prop_t *task_prop)
{
	struct sl_thd *thd;

	if (!task_prop) { return OS_INVALID_POINTER; }

	thd = sl_thd_lkup(task_id);

	/* TODO: Fix this ugly workaround */
	if (!thd || thd->state == SL_THD_FREE) { return OS_ERR_INVALID_ID; }

	struct cfe_task_info *task_info = &cfe_tasks[task_id];
	*task_prop                      = task_info->osal_task_prop;

	return OS_SUCCESS;
}

/*
 * Main thread waiting API
 */

/*
 * OS-specific background thread implementation - waits forever for events to occur.
 *
 * This should be called from the BSP main routine / initial thread after all other
 * board / application initialization has taken place and all other tasks are running.
 */
void
OS_IdleLoop(void)
{
	while (1) sl_thd_block(0);
}

/*
 * OS_ApplicationShutdown() provides a means for a user-created thread to request the orderly
 * shutdown of the whole system, such as part of a user-commanded reset command.
 * This is preferred over e.g. ApplicationExit() which exits immediately and does not
 * provide for any means to clean up first.
 */
void
OS_ApplicationShutdown(uint8 flag)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
}

/*
 * Mutex API
 */

struct mutex {
	int               used;
	struct sl_lock    lock;
	OS_mut_sem_prop_t prop;
};

struct sl_lock mutex_data_lock = SL_LOCK_STATIC_INIT();
struct mutex   mutexes[OS_MAX_MUTEXES];

int32
OS_MutSemCreate(uint32 *sem_id, const char *sem_name, uint32 options)
{
	uint32 id;
	int32  result = OS_SUCCESS;

	sl_lock_take(&mutex_data_lock);

	if (sem_id == NULL || sem_name == NULL) {
		result = OS_INVALID_POINTER;
		goto exit;
	}

	if (!is_valid_name(sem_name)) {
		result = OS_ERR_NAME_TOO_LONG;
		goto exit;
	}

	for (id = 0; id < OS_MAX_MUTEXES; id++) {
		if (mutexes[id].used && strcmp(sem_name, mutexes[id].prop.name) == 0) {
			result = OS_ERR_NAME_TAKEN;
			goto exit;
		}
	}

	for (id = 0; id < OS_MAX_MUTEXES; id++) {
		if (!mutexes[id].used) { break; }
	}
	if (id >= OS_MAX_MUTEXES || mutexes[id].used) {
		result = OS_ERR_NO_FREE_IDS;
		goto exit;
	}

	*sem_id = id;

	mutexes[id].used = TRUE;
	sl_lock_init(&mutexes[id].lock);
	mutexes[id].prop.creator = sl_thdid();
	strcpy(mutexes[id].prop.name, sem_name);

exit:
	sl_lock_release(&mutex_data_lock);

	return result;
}

int32
OS_MutSemGive(uint32 sem_id)
{
	int32 result = OS_SUCCESS;

	EVTTR_MUTEX_GIVE_START((short)sem_id);
	if (sem_id >= OS_MAX_MUTEXES || !mutexes[sem_id].used) {
		result = OS_ERR_INVALID_ID;

		goto ret;
	}

	sl_lock_release(&mutexes[sem_id].lock);

ret:
	EVTTR_MUTEX_GIVE_END((short)sem_id);

	return result;
}

int32
OS_MutSemTake(uint32 sem_id)
{
	int32 result = OS_SUCCESS;

	EVTTR_MUTEX_TAKE_START((short)sem_id);
	if (sem_id >= OS_MAX_MUTEXES || !mutexes[sem_id].used) {
		result = OS_ERR_INVALID_ID;

		goto ret;
	}

	sl_lock_take(&mutexes[sem_id].lock);

ret:
	EVTTR_MUTEX_TAKE_END((short)sem_id);

	return result;
}

int32
OS_MutSemDelete(uint32 sem_id)
{
	int32 result = OS_SUCCESS;

	sl_lock_take(&mutex_data_lock);

	if (sem_id >= OS_MAX_MUTEXES || !mutexes[sem_id].used) {
		result = OS_ERR_INVALID_ID;
		goto exit;
	}

	if (sl_lock_holder(&mutexes[sem_id].lock) != 0) {
		result = OS_SEM_FAILURE;
		goto exit;
	}

	mutexes[sem_id].used = FALSE;

exit:
	sl_lock_release(&mutex_data_lock);
	return result;
}

int32
OS_MutSemGetIdByName(uint32 *sem_id, const char *sem_name)
{
	int   i;
	int32 result = OS_SUCCESS;

	sl_lock_take(&mutex_data_lock);

	if (sem_id == NULL || sem_name == NULL) {
		result = OS_INVALID_POINTER;
		goto exit;
	}

	if (strlen(sem_name) >= OS_MAX_API_NAME) {
		result = OS_ERR_NAME_TOO_LONG;
		goto exit;
	}

	for (i = 0; i < OS_MAX_MUTEXES; i++) {
		if (mutexes[i].used && (strcmp(mutexes[i].prop.name, (char *)sem_name) == 0)) {
			*sem_id = i;
			goto exit;
		}
	}

	/* The name was not found in the table,
	 *  or it was, and the sem_id isn't valid anymore */
	result = OS_ERR_NAME_NOT_FOUND;
exit:
	sl_lock_release(&mutex_data_lock);
	return result;
}

int32
OS_MutSemGetInfo(uint32 sem_id, OS_mut_sem_prop_t *mut_prop)
{
	int32 result = OS_SUCCESS;

	sl_lock_take(&mutex_data_lock);

	if (!mut_prop) {
		result = OS_INVALID_POINTER;
		goto exit;
	}

	if (sem_id >= OS_MAX_MUTEXES || !mutexes[sem_id].used) {
		result = OS_ERR_INVALID_ID;
		goto exit;
	}

	*mut_prop = mutexes[sem_id].prop;

exit:
	sl_lock_release(&mutex_data_lock);
	return result;
}

#define OS_SEM_MAX_VAL 512
/*
 * Semaphore API
 */

struct semaphore {
	int used;

	volatile uint32  count;

	/* Ideally, should be a priority sorted list of waiters, not just the first! */
	volatile thdid_t first_waiter;

	uint32 creator;
	char   name[OS_MAX_API_NAME];

	struct sl_lock lock;
};

struct sl_lock semaphore_data_lock = SL_LOCK_STATIC_INIT();

struct semaphore binary_semaphores[OS_MAX_BIN_SEMAPHORES];
struct semaphore counting_semaphores[OS_MAX_COUNT_SEMAPHORES];

int32
OS_SemaphoreCreate(struct semaphore *semaphores, uint32 max_semaphores, uint32 *sem_id, const char *sem_name,
                   uint32 sem_initial_value, uint32 options)
{
	uint32 id;
	int32  result = OS_SUCCESS;

	sl_lock_take(&semaphore_data_lock);

	if (sem_id == NULL || sem_name == NULL) {
		result = OS_INVALID_POINTER;
		goto exit;
	}

	if (!is_valid_name(sem_name)) {
		result = OS_ERR_NAME_TOO_LONG;
		goto exit;
	}

	for (id = 0; id < max_semaphores; id++) {
		if (semaphores[id].used && strcmp(sem_name, semaphores[id].name) == 0) {
			result = OS_ERR_NAME_TAKEN;
			goto exit;
		}
	}

	for (id = 0; id < max_semaphores; id++) {
		if (!semaphores[id].used) { break; }
	}

	if (id >= max_semaphores || semaphores[id].used) {
		result = OS_ERR_NO_FREE_IDS;
		goto exit;
	}

	*sem_id                = id;
	semaphores[id].used    = TRUE;
	semaphores[id].creator = sl_thdid();
	semaphores[id].count   = sem_initial_value;
	semaphores[id].first_waiter = 0;
	strcpy(semaphores[id].name, sem_name);
	sl_lock_init(&semaphores[id].lock);

exit:
	sl_lock_release(&semaphore_data_lock);
	return result;
}

int32
OS_SemaphoreFlush(struct semaphore *semaphores, uint32 max_semaphores, uint32 sem_id)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}


int32
OS_SemaphoreGive(struct semaphore *semaphores, uint32 max_semaphores, uint32 sem_id)
{
	int32 result = OS_SUCCESS;
	thdid_t wakeup_tid = 0;

	if (sem_id >= max_semaphores || !semaphores[sem_id].used) {
		result = OS_ERR_INVALID_ID;
		goto exit;
	}

	sl_lock_take(&semaphores[sem_id].lock);

	(semaphores[sem_id].count)++;
	wakeup_tid = semaphores[sem_id].first_waiter;
	assert(semaphores[sem_id].count <= OS_SEM_MAX_VAL);
	sl_lock_release(&semaphores[sem_id].lock);
	if (likely(wakeup_tid)) sl_thd_wakeup(wakeup_tid);

exit:
	return result;
}

#define OS_SEM_WAIT_US 5000

int32
OS_SemaphoreTake(struct semaphore *semaphores, uint32 max_semaphores, uint32 sem_id)
{
	int32 result = OS_SUCCESS;

	if (sem_id >= max_semaphores || !semaphores[sem_id].used) {
		result = OS_ERR_INVALID_ID;
		goto exit;
	}

	sl_lock_take(&semaphores[sem_id].lock);
	while (semaphores[sem_id].count == 0) {

		if (semaphores[sem_id].first_waiter == 0) semaphores[sem_id].first_waiter = sl_thdid();
		sl_lock_release(&semaphores[sem_id].lock);
		sl_thd_block(0);
		sl_lock_take(&semaphores[sem_id].lock);
	}

	assert(semaphores[sem_id].used);
	(semaphores[sem_id].count)--;

	assert(semaphores[sem_id].count <= OS_SEM_MAX_VAL);
	sl_lock_release(&semaphores[sem_id].lock);

exit:

	return result;
}

int32
OS_SemaphoreTimedWait(struct semaphore *semaphores, uint32 max_semaphores, uint32 sem_id, uint32 msecs)
{
	int32      result     = OS_SUCCESS;
	microsec_t start_time = sl_now();
	microsec_t max_wait   = sl_usec2cyc(msecs * 1000);

	if (sem_id >= max_semaphores || !semaphores[sem_id].used) {
		result = OS_ERR_INVALID_ID;
		goto exit;
	}

	sl_lock_take(&semaphores[sem_id].lock);
	while (semaphores[sem_id].count == 0 && (sl_now() - start_time) < max_wait) {
		cycles_t waitcycs = 0;

		if (semaphores[sem_id].first_waiter == 0) semaphores[sem_id].first_waiter = sl_thdid();
		sl_lock_release(&semaphores[sem_id].lock);

		waitcycs = sl_now() + sl_usec2cyc(OS_SEM_WAIT_US);
		if (unlikely(waitcycs > max_wait)) waitcycs = max_wait;
		sl_thd_block_timeout(0, waitcycs);
		sl_lock_take(&semaphores[sem_id].lock);
	}

	assert(semaphores[sem_id].used);
	if (semaphores[sem_id].count == 0) {
		result = OS_SEM_TIMEOUT;
		goto done;
	}

	(semaphores[sem_id].count)--;

done:
	assert(semaphores[sem_id].count <= OS_SEM_MAX_VAL);
	sl_lock_release(&semaphores[sem_id].lock);

exit:
	return result;
}

int32
OS_SemaphoreDelete(struct semaphore *semaphores, uint32 max_semaphores, uint32 sem_id)
{
	int32 result = OS_SUCCESS;
	sl_lock_take(&semaphore_data_lock);

	if (sem_id >= max_semaphores || !semaphores[sem_id].used) {
		result = OS_ERR_INVALID_ID;
		goto exit;
	}

	semaphores[sem_id].used = FALSE;

exit:
	sl_lock_release(&semaphore_data_lock);

	return result;
}

int32
OS_SemaphoreGetIdByName(struct semaphore *semaphores, uint32 max_semaphores, uint32 *sem_id, const char *sem_name)
{
	uint32 i;
	int32  result = OS_SUCCESS;

	sl_lock_take(&semaphore_data_lock);

	if (sem_id == NULL || sem_name == NULL) {
		result = OS_INVALID_POINTER;
		goto exit;
	}

	if (!is_valid_name(sem_name)) {
		result = OS_ERR_NAME_TOO_LONG;
		goto exit;
	}

	for (i = 0; i < max_semaphores; i++) {
		if (semaphores[i].used && (strcmp(semaphores[i].name, (char *)sem_name) == 0)) {
			*sem_id = i;
			goto exit;
		}
	}

	/* The name was not found in the table,
	 *  or it was, and the sem_id isn't valid anymore
	 */
	result = OS_ERR_NAME_NOT_FOUND;
exit:
	sl_lock_release(&semaphore_data_lock);
	return result;
}


/* Binary semaphore methods */
int32
OS_BinSemCreate(uint32 *sem_id, const char *sem_name, uint32 sem_initial_value, uint32 options)
{
	return OS_SemaphoreCreate(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id, sem_name, sem_initial_value,
	                          options);
}

int32
OS_BinSemFlush(uint32 sem_id)
{
	return OS_SemaphoreFlush(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id);
}

int32
OS_BinSemGive(uint32 sem_id)
{
	int32 result = OS_SUCCESS;

	EVTTR_BINSEM_GIVE_START((short)sem_id);
	result = OS_SemaphoreGive(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id);
	EVTTR_BINSEM_GIVE_END((short)sem_id);

	return result;
}

int32
OS_BinSemTake(uint32 sem_id)
{
	int32 result = OS_SUCCESS;

	EVTTR_BINSEM_TAKE_START((short)sem_id);
	result = OS_SemaphoreTake(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id);
	EVTTR_BINSEM_TAKE_END((short)sem_id);

	return result;
}

int32
OS_BinSemTimedWait(uint32 sem_id, uint32 msecs)
{
	int32 result = OS_SUCCESS;

	EVTTR_BINSEM_TIMEDWAIT_START((short)sem_id);
	result = OS_SemaphoreTimedWait(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id, msecs);
	EVTTR_BINSEM_TIMEDWAIT_END((short)sem_id);

	return result;
}

int32
OS_BinSemDelete(uint32 sem_id)
{
	return OS_SemaphoreDelete(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id);
}

int32
OS_BinSemGetIdByName(uint32 *sem_id, const char *sem_name)
{
	return OS_SemaphoreGetIdByName(binary_semaphores, OS_MAX_BIN_SEMAPHORES, sem_id, sem_name);
}

int32
OS_BinSemGetInfo(uint32 sem_id, OS_bin_sem_prop_t *bin_prop)
{
	int32 result = OS_SUCCESS;
	sl_lock_take(&semaphore_data_lock);

	if (!bin_prop) {
		result = OS_INVALID_POINTER;
		goto exit;
	}

	if (sem_id >= OS_MAX_BIN_SEMAPHORES || !binary_semaphores[sem_id].used) {
		result = OS_ERR_INVALID_ID;
		goto exit;
	}

	*bin_prop = (OS_bin_sem_prop_t){.creator = binary_semaphores[sem_id].creator,
	                                .value   = binary_semaphores[sem_id].count};

	strcpy(bin_prop->name, binary_semaphores[sem_id].name);

exit:
	sl_lock_release(&semaphore_data_lock);
	return result;
}


int32
OS_CountSemCreate(uint32 *sem_id, const char *sem_name, uint32 sem_initial_value, uint32 options)
{
	return OS_SemaphoreCreate(counting_semaphores, OS_MAX_COUNT_SEMAPHORES, sem_id, sem_name, sem_initial_value,
	                          options);
}

int32
OS_CountSemGive(uint32 sem_id)
{
	int32 result = OS_SUCCESS;

	EVTTR_COUNTSEM_GIVE_START((short)sem_id);
	result = OS_SemaphoreGive(counting_semaphores, OS_MAX_COUNT_SEMAPHORES, sem_id);
	EVTTR_COUNTSEM_GIVE_END((short)sem_id);

	return result;
}

int32
OS_CountSemTake(uint32 sem_id)
{
	int32 result = OS_SUCCESS;

	EVTTR_COUNTSEM_TAKE_START((short)sem_id);
	result = OS_SemaphoreTake(counting_semaphores, OS_MAX_COUNT_SEMAPHORES, sem_id);
	EVTTR_COUNTSEM_TAKE_END((short)sem_id);

	return result;
}

int32
OS_CountSemTimedWait(uint32 sem_id, uint32 msecs)
{
	int32 result = OS_SUCCESS;

	EVTTR_COUNTSEM_TIMEDWAIT_START((short)sem_id);
	result = OS_SemaphoreTimedWait(counting_semaphores, OS_MAX_COUNT_SEMAPHORES, sem_id, msecs);
	EVTTR_COUNTSEM_TIMEDWAIT_END((short)sem_id);

	return result;
}

int32
OS_CountSemDelete(uint32 sem_id)
{
	return OS_SemaphoreDelete(counting_semaphores, OS_MAX_COUNT_SEMAPHORES, sem_id);
}

int32
OS_CountSemGetIdByName(uint32 *sem_id, const char *sem_name)
{
	return OS_SemaphoreGetIdByName(counting_semaphores, OS_MAX_COUNT_SEMAPHORES, sem_id, sem_name);
}

int32
OS_CountSemGetInfo(uint32 sem_id, OS_count_sem_prop_t *count_prop)
{
	int32 result = OS_SUCCESS;
	sl_lock_take(&semaphore_data_lock);

	if (!count_prop) {
		result = OS_INVALID_POINTER;
		goto exit;
	}

	if (sem_id >= OS_MAX_COUNT_SEMAPHORES || !counting_semaphores[sem_id].used) {
		result = OS_ERR_INVALID_ID;
		goto exit;
	}

	*count_prop = (OS_count_sem_prop_t){.creator = counting_semaphores[sem_id].creator,
	                                    .value   = counting_semaphores[sem_id].count};

	strcpy(count_prop->name, counting_semaphores[sem_id].name);

exit:
	sl_lock_release(&semaphore_data_lock);

	return result;
}
