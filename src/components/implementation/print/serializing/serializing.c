/***
 * A low-level printing serializer to avoid heavy interleaving of
 * printouts over slow serial lines, especially when threads print on
 * multiple cores. This component has no dependencies, but this means
 * that it
 *
 * 1. uses busy wait (i.e. spinning) to handle contention, thus can
 *    waste huge amounts of resources, and
 * 2. should only be used as a debugging facility.
 *
 * Another implementation of this API might include time-bounded
 * flushing of buffers, and blocking to handle contention, but doing
 * so would require depending on the scheduler, thus prohibit the
 * debugging range.
 */

#include <cos_component.h>
#include <llprint.h>
#include <ps.h>

#include <string.h>


#define PRINT_STRLEN 180

struct ps_lock lock;

struct print_buffer {
	thdid_t thdid;
	compid_t compid;
	char string[PRINT_STRLEN];
	size_t offset;
} data[NUM_CPU];

static void
flush_buf(struct print_buffer *b)
{
	assert(b->offset + 1 < PRINT_STRLEN);
	b->string[b->offset] = '\0';
	printc("[%d %ld %ld] %s", cos_coreid(), b->thdid, b->compid, b->string);
	b->offset = 0;
}

static void
release_buf(struct print_buffer *buf)
{
	/* Give up the buffer! */
	buf->thdid  = 0;
	buf->compid = 0;
	buf->offset = 0;
}

int
print_str_chunk(u32_t chunk0, u32_t chunk1, u32_t chunk2, int len_left)
{
	struct print_buffer *buf = NULL;
	u32_t chunks[4] = {chunk0, chunk1, chunk2, 0}; /* the `0` adds the '\0' */
	char *str = (char *)chunks;
	coreid_t coreid = cos_coreid();
	compid_t compid = (compid_t)cos_inv_token();
	thdid_t thdid   = cos_thdid();
	int i, len = len_left;

	if (len <= 0) return 0;
	if (len > (int)(sizeof(u32_t) * 3)) len = (int)(sizeof(u32_t) * 3);

	/* Each core has a buffer. Interleaving on a core is less likely, so we save buffer memory. */
	buf = &data[coreid];

	ps_lock_take(&lock);

	/*
	 * If we preempted another thread locally, flush out its
	 * prints so that we see the interleaving.
	 */
	if (buf->thdid == 0 && buf->compid == 0) {
		buf->thdid  = thdid;
		buf->compid = compid;
	}
	if (buf->thdid != thdid || buf->compid != compid) {
		flush_buf(buf);
		release_buf(buf);
	}

	/* Is the buffer out of room? Flush it around 128 bytes. */
	if (buf->offset + len + 1 >= PRINT_STRLEN) {
		flush_buf(buf);
		/* Note: the offset was just reset to 0 */
	}
	/* Add to the buffer... */
	memcpy(&buf->string[buf->offset], str, len);
	buf->offset             += len;
	buf->string[buf->offset] = '\0';
	/* printc statement has sent all data, output it! */
	if (len == len_left) {
		flush_buf(buf);
		release_buf(buf);
	}

	ps_lock_release(&lock);

	return len;
}
