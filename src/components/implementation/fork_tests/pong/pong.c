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

		confirm(cos_spd_id(), pong);
		
		// to ignore fork for now
		printc("calling read\n");
		int readc = nread(cos_spd_id(), ping, 0);
		printc("read returned %d\n", readc);
		readc++;

		printc("----second iteration pong----\n");
		readc = nwrite(cos_spd_id(), ping, readc);
		printc("write returned %d\n", readc);
	} else {
		printc("been confirmed\n");

	}
}
