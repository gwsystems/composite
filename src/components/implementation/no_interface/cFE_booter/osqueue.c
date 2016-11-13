#include "cFE_util.h"

#include "gen/osapi.h"
#include "gen/common_types.h"

/*
** Message Queue API
*/

/*
** Queue Create now has the Queue ID returned to the caller.
*/
int32 OS_QueueCreate(uint32 *queue_id, const char *queue_name,
                     uint32 queue_depth, uint32 data_size, uint32 flags)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_QueueDelete(uint32 queue_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_QueueGet(uint32 queue_id, void *data, uint32 size,
                  uint32 *size_copied, int32 timeout)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_QueuePut(uint32 queue_id, const void *data, uint32 size,
                  uint32 flags)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_QueueGetIdByName(uint32 *queue_id, const char *queue_name)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_QueueGetInfo(uint32 queue_id, OS_queue_prop_t *queue_prop)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
