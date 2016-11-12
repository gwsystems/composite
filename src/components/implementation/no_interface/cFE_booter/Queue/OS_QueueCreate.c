#include <stdio.h>
#include <string.h>
#include <cos_componant.h>
#include <cos_kernel_api.h>

/**
*	Description: (From NASA's OSAL Library API documentation)
*
* This is the function used to create a queue in the operating system. Depending on the
* underlying operating system, the memory for the queue will be allocated automatically or
* allocated by the code that sets up the queue. Queue names must be unique; if the name
* already exists this function fails. Names cannot be NULL.
**/

int32 OS_QueueCreate( uint32 *queue_id, const char *queue_name, uint32 queue_depth, uint32 data_size, uint32 flags ){

}