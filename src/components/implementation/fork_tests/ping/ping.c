#include <print.h>
#include <cos_component.h>
#include <cbuf.h>
#include <cbuf_mgr.h>
#include <voter.h>

#define NUM 10
static int confirm_flag = 0;

void cos_init(void) {
	if (!confirm_flag) {
		printc("ping init\n");
		confirm_flag = 1;
		cbuf_t read_buffer, write_buffer;
		void *buf_read, *buf_write;
		if (confirm(cos_spd_id(), ping)) BUG();

		/* Get our buffers*/
		write_buffer = get_write_buf(cos_spd_id());
		read_buffer = get_read_buf(cos_spd_id());
		buf_read = cbuf2buf(read_buffer, 1024);
		buf_write = cbuf2buf(write_buffer, 1024);
		printc("ping confirmed with buffers read (%d) and write(%d)\n", read_buffer, write_buffer);
		
		/* Start sending data */
		printc("calling write\n");
		memcpy(buf_write, "abc", 3);
		int writec = nwrite(cos_spd_id(), pong, 3);
		printc("write returned %d\n", writec);

		//printc("----second iteration ping----\n");
		//void *data = nread(cos_spd_id(), pong, 3);
		//printc("read returned %s\n", (char *) data);
	} else {
		printc("been confirmed\n");
	}
}
