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

	printc("ping init - spdid %d\n", cos_spd_id());
	if (confirm(cos_spd_id())) BUG();

	/* Get our buffers*/
	write_buffer = get_write_buf(cos_spd_id());
	read_buffer = get_read_buf(cos_spd_id());
	buf_read = cbuf2buf(read_buffer, 1024);
	buf_write = cbuf2buf(write_buffer, 1024);
	printc("ping confirmed with buffers read (%d) and write(%d)\n", read_buffer, write_buffer);

	/* Start sending data */
	while (i < 3) {
		printc("\nping calling write\n");
		memcpy(buf_write, "abc\0", 4);
		ret = nwrite(cos_spd_id(), 0, 4);
		printc("write returned %d\n\n", ret);

		printc("\nping calling read\n");
		ret = nread(cos_spd_id(), 1, 4);
		printc("read returned %d and now we have data [%s] - expected \n\n", ret, ((char*) buf_read));

		i++;
	}
}
