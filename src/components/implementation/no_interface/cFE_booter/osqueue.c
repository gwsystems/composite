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
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_QueueDelete(uint32 queue_id)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_QueueGet(uint32 queue_id, void *data, uint32 size,
                  uint32 *size_copied, int32 timeout)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_QueuePut(uint32 queue_id, const void *data, uint32 size,
                  uint32 flags)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_QueueGetIdByName(uint32 *queue_id, const char *queue_name)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_QueueGetInfo(uint32 queue_id, OS_queue_prop_t *queue_prop)
{
    panic("Unimplemented method!"); // TODO: Implement me!
    return 0;
}
