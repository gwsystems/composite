#include "cFE_util.h"

#include "gen/osapi.h"
#include "gen/common_types.h"

/*
** Timer API
*/

struct {
	int used;
	OS_timer_prop_t props;
} timers[OS_MAX_TIMERS];

int32
OS_TimerAPIInit(void)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

/* TODO: Verify this API really insn't necessary */
int32
OS_TimerCreate(uint32 *timer_id, const char *timer_name, uint32 *clock_accuracy, OS_TimerCallback_t callback_ptr)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_TimerAdd(uint32 *timer_id, const char *timer_name, uint32 timebase_id, OS_ArgCallback_t callback_ptr,
            void *callback_arg)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_TimerSet(uint32 timer_id, uint32 start_time, uint32 interval_time)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_TimerDelete(uint32 timer_id)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}


int32
OS_TimerGetIdByName(uint32 *timer_id, const char *timer_name)
{
	PANIC("Unimplemented method!"); /* TODO: Implement me! */
	return 0;
}

int32
OS_TimerGetInfo(uint32 timer_id, OS_timer_prop_t *timer_prop)
{
	return OS_ERR_NAME_NOT_FOUND;
}

/* We don't implement the TimeBase API */
int32
OS_TimeBaseCreate(uint32 *timer_id, const char *timebase_name, OS_TimerSync_t external_sync)
{
	return OS_ERR_NOT_IMPLEMENTED;
}

int32
OS_TimeBaseSet(uint32 timer_id, uint32 start_time, uint32 interval_time)
{
	return OS_ERR_NOT_IMPLEMENTED;
}

int32
OS_TimeBaseDelete(uint32 timer_id)
{
	return OS_ERR_NOT_IMPLEMENTED;
}

int32
OS_TimeBaseGetIdByName(uint32 *timer_id, const char *timebase_name)
{
	return OS_ERR_NOT_IMPLEMENTED;
}
