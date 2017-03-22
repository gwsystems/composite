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
	if (confirm(cos_spd_id(), pong)) BUG();

	/* Get our buffers*/
	write_buffer = get_write_buf(cos_spd_id());
	read_buffer = get_read_buf(cos_spd_id());
	buf_read = cbuf2buf(read_buffer, 1024);
	buf_write = cbuf2buf(write_buffer, 1024);
	printc("pong confirmed with buffers read (%d) and write(%d)\n", read_buffer, write_buffer);
	
	while (i < 10) {	
		printc("pong calling read\n");
		data = nread(cos_spd_id(), pong, 4);
		printc("read returned %d and now we have data [%s]\n\n", ret, ((char*) buf_read));

		printc("pong calling write\n");
		ret = nwrite(cos_spd_id(), pong, 4);
		printc("write returned %d\n\n", ret);

		i++;
	}
}
