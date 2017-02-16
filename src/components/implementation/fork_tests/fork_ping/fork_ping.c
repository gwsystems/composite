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

		confirm(cos_spd_id());
		
		// to ignore fork for now
		printc("calling write\n");
		int writec = nwrite(cos_spd_id(), 0, 0);
		printc("write returned %d\n", writec);
		writec++;
		writec = nread(cos_spd_id(), 0, writec);
		printc("read returned %d\n", writec);
	} else {
		printc("been confirmed\n");

	}
}
