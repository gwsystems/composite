#include <cos_component.h>
#include <cos_debug.h>
#include <print.h>
#include <net_if.h>
#include <sched.h>

void cos_init(void)
{
	u64_t t;
	u32_t u;
	struct cos_array *arr;
	int sz = 1600 + sizeof(struct cos_array);

	arr = cos_argreg_alloc(sz);
	BUG_ON(netif_event_create(cos_spd_id()));
	while (1) {
		arr->sz = sz;
		BUG_ON(netif_event_wait(cos_spd_id(), arr));
		rdtscll(t);
		u = netif_upcall_cyc();
		if (u && (int)u != 1 && (int)u != -1) {
			printc("rcv %u\n", (u32_t)t - u);
		}
	}
	cos_argreg_free(arr);
	netif_event_release(cos_spd_id());
}

void hack(void) { sched_block(cos_spd_id(), 0); }
