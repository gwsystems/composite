#include <print.h>
#include <cos_component.h>
#include <cbuf.h>
#include <cbuf_mgr.h>
#include <voter.h>

void cos_init(void) {
	int i = 0;
	int ret;
	void *data;
	void *buf_read, *buf_write;
	cbuf_t read_buffer, write_buffer;
	
	printc("pong init\n");
	if (replica_confirm(cos_spd_id())) BUG();

	/* Get our buffers*/
	write_buffer = get_write_buf(cos_spd_id());
	read_buffer = get_read_buf(cos_spd_id());
	buf_read = cbuf2buf(read_buffer, 1024);
	buf_write = cbuf2buf(write_buffer, 1024);
	printc("pong confirmed with buffers read (%d) and write(%d)\n", read_buffer, write_buffer);
	
	confirm_fork(cos_spd_id());
	
	while (i < 7) {	
		printc("\ni = %d, pong calling read from spdid %d\n", i, cos_spd_id());
		ret = nread(cos_spd_id(), 0, 4);
		assert(ret);
		printc("Thread %d: read returned %d and now we have data [%s] - expected abc\n\n", cos_get_thd_id(), ret, ((char*) buf_read));

		printc("\ni = %d, pong calling write\n", i);
		memcpy(buf_write, "xyz\0", 4);
		ret = nwrite(cos_spd_id(), 1, 4);
		assert(!ret);
		printc("Thread %d: write returned %d\n\n", cos_get_thd_id(), ret);

		i++;
	}
	
	printc("Spdid %d finished.\n", cos_spd_id());
}
