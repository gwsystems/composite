#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <cbuf.h>
#include <evt.h>
#include <torrent.h>

void cos_init(void)
{
	cbuf_t c1 = cbuf_null();
	td_t t1, t2;
	long evt1, evt2;
	char *params1 = "bar";
	char *params2 = "foo/";
	
	evt1 = evt_create(cos_spd_id());
	evt2 = evt_create(cos_spd_id());

	t1 = tsplit(cos_spd_id(), td_root, params1, strlen(params1)+1, 0, evt1);
	if (t1 < 1) {
		printc("split failed\n");
		return;
	}
	trelease(cos_spd_id(), t1);

	t1 = tsplit(cos_spd_id(), td_root, params2, strlen(params2) + 1, 0, evt1);
	t2 = tsplit(cos_spd_id(), t1, params2, strlen(params2) + 1, 0, evt2);
}
