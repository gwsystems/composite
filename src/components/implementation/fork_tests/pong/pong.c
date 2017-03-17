#include <print.h>
#include <cos_component.h>
#include <cbuf.h>
#include <cbuf_mgr.h>
#include <voter.h>

#define NUM 10
static int confirm_flag = 0;

void cos_init(void) {
	if (!confirm_flag) {
		printc("pong init\n");
		confirm_flag = 1;

		void *buf_read, *buf_write;
		cbuf_t read_buffer, write_buffer;
		if (confirm(cos_spd_id(), pong)) BUG();

		/* Get our buffers*/
		write_buffer = get_write_buf(cos_spd_id());
		read_buffer = get_read_buf(cos_spd_id());
		buf_read = cbuf2buf(read_buffer, 1024);
		buf_write = cbuf2buf(write_buffer, 1024);
		printc("pong confirmed with buffers read (%d) and write(%d)\n", read_buffer, write_buffer);
		
		// to ignore fork for now
		printc("calling read\n");
		int ret = nread(cos_spd_id(), ping, 4);
		printc("read returned %d and now we have data [%s]\n", ret, ((char*) buf_read));

		//printc("----second iteration pong----\n");
		//int r = nwrite(cos_spd_id(), ping, "opq", 3);
		//printc("write returned %d\n", r);
	} else {
		printc("been confirmed\n");

	}
}
