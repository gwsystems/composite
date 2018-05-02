#include <cos_component.h>
#include <cos_defkernel_api.h>

#include <sl.h>
#include <sl_consts.h>

#include "gen/osapi.h"
#include "gen/cfe_psp.h"
#include "gen/cfe_time.h"
#include "gen/common_types.h"

#include "cFE_util.h"

/*
 * Initialization of API
 */
int have_initialized = 0;

int32
OS_API_Init(void)
{
	struct cos_defcompinfo *defci;
	struct cos_compinfo *   ci;

	if (have_initialized) return OS_SUCCESS;

	cos_defcompinfo_init();
	defci = cos_defcompinfo_curr_get();
	ci    = cos_compinfo_get(defci);
	cos_meminfo_init(&(ci->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);

	OS_FS_Init();
	OS_ModuleTableInit();

	have_initialized = 1;

	return OS_SUCCESS;
}

/*
 * OS_DeleteAllObjects() provides a means to clean up all resources allocated by this
 * instance of OSAL.  It would typically be used during an orderly shutdown but may also
 * be helpful for testing purposes.
 */
void
OS_DeleteAllObjects(void)
{
	uint32 i;

	/* FIXME: Add deleting tasks when we have a way of iterating through them
	 * for (i = 0; i < OS_MAX_TASKS; ++i)
	 * {
	 *     OS_TaskDelete(i);
	 * }
	 * for (i = 0; i < OS_MAX_TIMERS; ++i)
	 * {
	 *     OS_TimerDelete(i);
	 * } */
	for (i = 0; i < OS_MAX_QUEUES; ++i) { OS_QueueDelete(i); }
	for (i = 0; i < OS_MAX_MUTEXES; ++i) { OS_MutSemDelete(i); }
	for (i = 0; i < OS_MAX_COUNT_SEMAPHORES; ++i) { OS_CountSemDelete(i); }
	for (i = 0; i < OS_MAX_BIN_SEMAPHORES; ++i) { OS_BinSemDelete(i); }
	for (i = 0; i < OS_MAX_MODULES; ++i) { OS_ModuleUnload(i); }
	for (i = 0; i < OS_MAX_NUM_OPEN_FILES; ++i) { OS_close(i); }
}


/*
 * OS Time/Tick related API
 */

int32
OS_Milli2Ticks(uint32 milliseconds)
{
	uint32 ticks_per_millisecond = CFE_PSP_GetTimerTicksPerSecond() / 1000;
	return (int32)(ticks_per_millisecond * milliseconds);
}

int32
OS_Tick2Micros(void)
{
	return SL_MIN_PERIOD_US;
}

OS_time_t  local_time;
microsec_t last_time_check;

OS_time_t
OS_AdvanceTime(OS_time_t initial_time, microsec_t usec)
{
	microsec_t old_seconds         = (microsec_t)initial_time.seconds;
	microsec_t old_additional_usec = (microsec_t)initial_time.microsecs;

	microsec_t old_usec = old_seconds * (1000 * 1000) + old_additional_usec;
	microsec_t new_usec = old_usec + usec;

	microsec_t new_seconds         = new_usec / (1000 * 1000);
	microsec_t new_additional_usec = new_usec % (1000 * 1000);

	return (OS_time_t){.seconds = new_seconds, .microsecs = new_additional_usec};
}

int32
OS_GetLocalTime(OS_time_t *time_struct)
{
	if (!time_struct) { return OS_INVALID_POINTER; }

	if (last_time_check == 0) {
		local_time      = (OS_time_t){.seconds = 1181683060, .microsecs = 0};
		last_time_check = sl_now_usec();
	} else {
		microsec_t current_time = sl_now_usec();
		microsec_t elapsed_usec = current_time - last_time_check;

		local_time      = OS_AdvanceTime(local_time, elapsed_usec);
		last_time_check = current_time;
	}

	*time_struct = local_time;

	return OS_SUCCESS;
} /* end OS_GetLocalTime */

int32
OS_SetLocalTime(OS_time_t *time_struct)
{
	if (!time_struct) { return OS_INVALID_POINTER; }

	local_time      = *time_struct;
	last_time_check = sl_now_usec();

	return OS_SUCCESS;
} /*end OS_SetLocalTime */

/*
 * Exception API
 */

int32
OS_ExcAttachHandler(uint32 ExceptionNumber, void (*ExceptionHandler)(uint32, const void *, uint32), int32 parameter)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}


int32
OS_ExcEnable(int32 ExceptionNumber)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_ExcDisable(int32 ExceptionNumber)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

/*
 * Floating Point Unit API
 */

int32
OS_FPUExcAttachHandler(uint32 ExceptionNumber, void *ExceptionHandler, int32 parameter)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_FPUExcEnable(int32 ExceptionNumber)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_FPUExcDisable(int32 ExceptionNumber)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_FPUExcSetMask(uint32 mask)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_FPUExcGetMask(uint32 *mask)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

/*
 * Interrupt API
 * The disabling APIs always work, since interrupts are always disabled
 */
int32
OS_IntAttachHandler(uint32 InterruptNumber, osal_task_entry InterruptHandler, int32 parameter)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_IntUnlock(int32 IntLevel)
{
	return OS_SUCCESS;
}

int32
OS_IntLock(void)
{
	return OS_SUCCESS;
}

int32
OS_IntEnable(int32 Level)
{
	return OS_SUCCESS;
}

int32
OS_IntDisable(int32 Level)
{
	return OS_SUCCESS;
}

int32
OS_IntSetMask(uint32 mask)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_IntGetMask(uint32 *mask)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_IntAck(int32 InterruptNumber)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

/*
 * Shared memory API
 */
int32
OS_ShMemInit(void)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_ShMemCreate(uint32 *Id, uint32 NBytes, const char *SegName)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_ShMemSemTake(uint32 Id)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_ShMemSemGive(uint32 Id)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_ShMemAttach(cpuaddr *Address, uint32 Id)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_ShMemGetIdByName(uint32 *ShMemId, const char *SegName)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

/*
 * Heap API
 */
int32
OS_HeapGetInfo(OS_heap_prop_t *heap_prop)
{
	/* TODO: Implement me! */
	return OS_ERR_NOT_IMPLEMENTED;
}

/*
 * API for useful debugging function
 * (Implementation stolen from the posix osapi)
 */

int32
OS_GetErrorName(int32 error_num, os_err_name_t *err_name)
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

	if (err_name == NULL) { return (OS_INVALID_POINTER); }

	switch (error_num) {
	case OS_SUCCESS:
		strcpy(local_name, "OS_SUCCESS");
		break;
	case OS_ERROR:
		strcpy(local_name, "OS_ERROR");
		break;
	case OS_INVALID_POINTER:
		strcpy(local_name, "OS_INVALID_POINTER");
		break;
	case OS_ERROR_ADDRESS_MISALIGNED:
		strcpy(local_name, "OS_ADDRESS_MISALIGNED");
		break;
	case OS_ERROR_TIMEOUT:
		strcpy(local_name, "OS_ERROR_TIMEOUT");
		break;
	case OS_INVALID_INT_NUM:
		strcpy(local_name, "OS_INVALID_INT_NUM");
		break;
	case OS_SEM_FAILURE:
		strcpy(local_name, "OS_SEM_FAILURE");
		break;
	case OS_SEM_TIMEOUT:
		strcpy(local_name, "OS_SEM_TIMEOUT");
		break;
	case OS_QUEUE_EMPTY:
		strcpy(local_name, "OS_QUEUE_EMPTY");
		break;
	case OS_QUEUE_FULL:
		strcpy(local_name, "OS_QUEUE_FULL");
		break;
	case OS_QUEUE_TIMEOUT:
		strcpy(local_name, "OS_QUEUE_TIMEOUT");
		break;
	case OS_QUEUE_INVALID_SIZE:
		strcpy(local_name, "OS_QUEUE_INVALID_SIZE");
		break;
	case OS_QUEUE_ID_ERROR:
		strcpy(local_name, "OS_QUEUE_ID_ERROR");
		break;
	case OS_ERR_NAME_TOO_LONG:
		strcpy(local_name, "OS_ERR_NAME_TOO_LONG");
		break;
	case OS_ERR_NO_FREE_IDS:
		strcpy(local_name, "OS_ERR_NO_FREE_IDS");
		break;
	case OS_ERR_NAME_TAKEN:
		strcpy(local_name, "OS_ERR_NAME_TAKEN");
		break;
	case OS_ERR_INVALID_ID:
		strcpy(local_name, "OS_ERR_INVALID_ID");
		break;
	case OS_ERR_NAME_NOT_FOUND:
		strcpy(local_name, "OS_ERR_NAME_NOT_FOUND");
		break;
	case OS_ERR_SEM_NOT_FULL:
		strcpy(local_name, "OS_ERR_SEM_NOT_FULL");
		break;
	case OS_ERR_INVALID_PRIORITY:
		strcpy(local_name, "OS_ERR_INVALID_PRIORITY");
		break;

	default:
		strcpy(local_name, "ERROR_UNKNOWN");
		return_code = OS_ERROR;
	}

	strcpy((char *)err_name, local_name);

	return return_code;
}


/*
 * Abstraction for printf statements
 */
int is_printf_enabled = TRUE;

/* We expose this function so that app components can use it  */
int
emu_is_printf_enabled()
{
	return is_printf_enabled;
}

void
OS_printf(const char *string, ...)
{
	char    s[OS_BUFFER_SIZE];
	va_list arg_ptr;
	int     ret, len = OS_BUFFER_SIZE;

	if (!is_printf_enabled) return;

	va_start(arg_ptr, string);
	ret = vsnprintf(s, len, string, arg_ptr);
	va_end(arg_ptr);
	cos_llprint(s, ret);
}

void
OS_sprintf(char *str, const char *format, ...)
{
	va_list arg_ptr;
	va_start(arg_ptr, format);
	vsprintf(str, format, arg_ptr);
	va_end(arg_ptr);
}

void
OS_printf_disable(void)
{
	is_printf_enabled = FALSE;
}

void
OS_printf_enable(void)
{
	is_printf_enabled = TRUE;
}

/*
 * Call to exit the running application
 * Normally embedded applications run forever, but for debugging purposes
 * (unit testing for example) this is needed in order to end the test
 */
void
OS_ApplicationExit(int32 Status)
{
	PANIC("Application exit invoked!");
}
