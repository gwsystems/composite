#include "cFE_util.h"

#include "gen/osapi.h"
#include "gen/common_types.h"

/*
** Timer API
*/

int32  OS_TimerAPIInit(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TimeBaseCreate(uint32 *timer_id, const char *timebase_name, OS_TimerSync_t external_sync)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TimeBaseSet(uint32 timer_id, uint32 start_time, uint32 interval_time)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TimeBaseDelete(uint32 timer_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TimeBaseGetIdByName(uint32 *timer_id, const char *timebase_name)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}


int32 OS_TimerCreate(uint32 *timer_id, const char *timer_name, uint32 *clock_accuracy, OS_TimerCallback_t callback_ptr)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TimerAdd(uint32 *timer_id, const char *timer_name, uint32 timebase_id, OS_ArgCallback_t  callback_ptr, void *callback_arg)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TimerSet(uint32 timer_id, uint32 start_time, uint32 interval_time)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TimerDelete(uint32 timer_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}


int32 OS_TimerGetIdByName(uint32 *timer_id, const char *timer_name)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_TimerGetInfo(uint32  timer_id, OS_timer_prop_t *timer_prop)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
