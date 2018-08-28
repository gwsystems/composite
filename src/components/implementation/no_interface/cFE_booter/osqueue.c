#include "cFE_util.h"

#include "gen/common_types.h"
#include "gen/osapi.h"

#include <cos_debug.h>

#include <sl_lock.h>

#include <string.h>
#include <event_trace.h>

#define MAX_QUEUE_DATA_SIZE (1024 * 1024)

struct sl_lock queue_lock = SL_LOCK_STATIC_INIT();

/* The main queue data structure. */
struct queue {
	int32 used;

	/* The number of elements allowed in the queue */
	uint32 depth;

	/* The size, in bytes, of each element in the queue */
	uint32 data_size;

	char name[OS_MAX_API_NAME];

	uint32 head;
	uint32 tail;
	thdid_t wait_thd;

	struct sl_lock lock;
};

struct queue queues[OS_MAX_QUEUES];

char queue_data[OS_MAX_QUEUES][MAX_QUEUE_DATA_SIZE];

int32
OS_QueueCreate(uint32 *queue_id, const char *queue_name, uint32 queue_depth, uint32 data_size, uint32 flags)
{
	int32  i, ret = OS_SUCCESS;
	uint32 qid;

	if (queue_id == NULL || queue_name == NULL) { return OS_INVALID_POINTER; }

	if (strlen(queue_name) >= OS_MAX_API_NAME) { return OS_ERR_NAME_TOO_LONG; }

	assert(queue_depth * data_size <= MAX_QUEUE_DATA_SIZE);
	sl_lock_take(&queue_lock);
	/* Check to see if the name is already taken */
	for (i = 0; i < OS_MAX_QUEUES; i++) {
		if ((queues[i].used == TRUE) && strcmp((char *)queue_name, queues[i].name) == 0) {
			ret = OS_ERR_NAME_TAKEN;
			goto done;
		}
	}

	/* Find a free queue ID */
	for (qid = 0; qid < OS_MAX_QUEUES; qid++) {
		if (queues[qid].used == FALSE) { break; }
	}

	/* Fail if there are too many queues */
	if (qid >= OS_MAX_QUEUES || queues[qid].used == TRUE) {
		ret = OS_ERR_NO_FREE_IDS;
		goto done;
	}

	/* OS_ERROR may also be returned in the event that an OS call fails, but none are used here */

	sl_lock_init(&(queues[qid].lock));
	*queue_id                   = qid;
	queues[*queue_id].used      = TRUE;
	queues[*queue_id].depth     = queue_depth;
	queues[*queue_id].data_size = data_size;
	queues[*queue_id].wait_thd  = 0;
	strcpy(queues[*queue_id].name, queue_name);

done:
	sl_lock_release(&queue_lock);
	return ret;
}

int32
OS_QueueDelete(uint32 queue_id)
{
	int32 ret = OS_SUCCESS;

	if (queue_id > OS_MAX_QUEUES) { return OS_ERR_INVALID_ID; }

	sl_lock_take(&queue_lock);
	if (queues[queue_id].used == FALSE) {
		ret = OS_ERR_INVALID_ID;
		goto done;
	}

	/* Reset all values in the queue */
	queues[queue_id].used      = FALSE;
	queues[queue_id].depth     = 0;
	queues[queue_id].data_size = 0;
	strcpy(queues[queue_id].name, "");
	queues[queue_id].head = 0;
	queues[queue_id].tail = 0;

done:
	sl_lock_release(&queue_lock);

	return ret;
}

int32
OS_QueuePoll(uint32 queue_id, void *data, uint32 size, uint32 *size_copied)
{
	int32 ret = OS_SUCCESS;
	uint32 i;

	sl_lock_take(&queues[queue_id].lock);
	/* Check if there are messages to be received */
	if (queues[queue_id].head == queues[queue_id].tail) {
		ret = OS_QUEUE_EMPTY;
		goto done;
	}

	if (size < queues[queue_id].data_size) {
		ret = OS_QUEUE_INVALID_SIZE;
		goto done;
	}

	struct queue *cur = &queues[queue_id];

	/* Walk through the bytes at the head of the queue and write them to buffer `data` */
	for (i = 0; i < size; i++) { *((char *)data + i) = queue_data[queue_id][cur->head * cur->data_size + i]; }

	/* Advance the queue head, wrapping if it is passed `depth` */
	cur->head = (cur->head + 1) % cur->depth;

done:
	sl_lock_release(&queues[queue_id].lock);

	return ret;
}

#define OS_QUEUE_POLL_US (2000)

int32
OS_QueueGet(uint32 queue_id, void *data, uint32 size, uint32 *size_copied, int32 timeout_ms)
{
	uint32 i;
	int    result = OS_ERROR;
	cycles_t timeout_cycs = 0, start_cycs = 0, poll_cycs = sl_usec2cyc(OS_QUEUE_POLL_US);

	EVTTR_QUEUE_DEQ_START((short)queue_id);
	if (queue_id > OS_MAX_QUEUES) {
		result = OS_ERR_INVALID_ID;
		goto ret;
	}

	if (data == NULL || size_copied == NULL) {
		result = OS_INVALID_POINTER;
		goto ret;
	}

	if (queues[queue_id].used == FALSE) {
		result = OS_ERR_INVALID_ID;
		goto ret;
	}

	queues[queue_id].wait_thd = cos_thdid();

	if (timeout_ms == OS_CHECK) {
		result = OS_QueuePoll(queue_id, data, size, size_copied);

		goto ret;
	}

	start_cycs = sl_now();
	if (timeout_ms == OS_PEND) timeout_cycs = 0;
	else                       timeout_cycs = sl_usec2cyc(timeout_ms * 1000);

	while (!timeout_cycs || (sl_now() - start_cycs) < timeout_cycs) {
		result = OS_QueuePoll(queue_id, data, size, size_copied);

		if (result == OS_SUCCESS) break;
		sl_thd_block_timeout(0, sl_now() + poll_cycs);
	}

	if (result != OS_SUCCESS && timeout_ms != OS_CHECK && timeout_ms != OS_PEND) result = OS_QUEUE_TIMEOUT;

ret:
	EVTTR_QUEUE_DEQ_END((short)queue_id);

	return result;
}

/*
 * This function is used to send data on an existing queue. The flags can be used to specify
 * the behavior of the queue if it is full.
 */
int32
OS_QueuePut(uint32 queue_id, const void *data, uint32 size, uint32 flags)
{
	int32         result = OS_SUCCESS;
	uint32        i;
	struct queue *cur;
	thdid_t       wake_thd = 0;

	EVTTR_QUEUE_ENQ_START((short)queue_id);
	if (queue_id > OS_MAX_QUEUES) {
		result = OS_ERR_INVALID_ID;
		goto ret;
	}

	if (queues[queue_id].used == FALSE) {
		result = OS_ERR_INVALID_ID;
		goto ret;
	}

	if (data == NULL) {
		result = OS_INVALID_POINTER;
		goto ret;
	}

	sl_lock_take(&queues[queue_id].lock);
	/* Check if space remains in the queue */
	if ((queues[queue_id].tail + 1) % queues[queue_id].depth == queues[queue_id].head) {
		result = OS_QUEUE_FULL;
		goto done;
	}

	cur = &queues[queue_id];

	/* Walk through the bytes in `data` and write them to the tail of the specified queue */
	for (i = 0; i < size; i++) { queue_data[queue_id][cur->tail * cur->data_size + i] = *((char *)data + i); }

	/* Advance the queue tail, wrapping if it is past `depth` */
	cur->tail = (cur->tail + 1) % cur->depth;
	wake_thd  = queues[queue_id].wait_thd;

done:
	sl_lock_release(&queues[queue_id].lock);

ret:
	EVTTR_QUEUE_ENQ_END((short)queue_id);
	if (likely(wake_thd && wake_thd != cos_thdid())) sl_thd_wakeup(wake_thd);

	return result;
}


int32
OS_QueueGetIdByName(uint32 *queue_id, const char *queue_name)
{
	uint32 i;
	uint32 queue_found = FALSE;

	if (queue_id == NULL || queue_name == NULL) { return OS_INVALID_POINTER; }

	if (strlen(queue_name) > OS_MAX_API_NAME) { return OS_ERR_NAME_TOO_LONG; }

	sl_lock_take(&queue_lock);
	for (i = 0; i < OS_MAX_QUEUES; ++i) {
		if (strcmp(queue_name, queues[i].name) == 0) {
			*queue_id   = i;
			queue_found = TRUE;
			break;
		}
	}

	sl_lock_release(&queue_lock);
	if (queue_found == FALSE) { return OS_ERR_NAME_NOT_FOUND; }

	return OS_SUCCESS;
}

int32
OS_QueueGetInfo(uint32 queue_id, OS_queue_prop_t *queue_prop)
{
	if (queue_id > OS_MAX_QUEUES) { return OS_ERR_INVALID_ID; }

	if (queue_prop == NULL) { return OS_INVALID_POINTER; }

	if (queues[queue_id].used == FALSE) { return OS_ERR_INVALID_ID; }

	/* TODO: Identify creator; `0` is a dummy value */
	queue_prop->creator = 0;

	sl_lock_take(&queues[queue_id].lock);
	strcpy(queue_prop->name, queues[queue_id].name);
	sl_lock_release(&queues[queue_id].lock);

	/*
	 * NOTE: The OSAL documentation claims that there are two additional fields in `OS_queue_prop_t` called `free`
	 * and `id`. These members do not appear in our working version.
	 */

	return OS_SUCCESS;
}
