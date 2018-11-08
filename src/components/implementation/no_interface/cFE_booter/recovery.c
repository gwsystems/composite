#include <hypercall.h>
#include <capmgr.h>
#include "cFE_bookkeep.h"

int reboot_requested[MAX_NUM_SPDS] = { 0 };

#define MAX_REQS 16

static inline int
reboot_comp(spdid_t s)
{
	int i, ret, r2, r3;

	assert(s < MAX_NUM_SPDS);
	reboot_requested[s] = 1;

	for (i = 0; i < MAX_NUM_THREADS; i++) {
		struct sl_thd *t = cfe_bookkeep_thd(s, i);

		if (!t) continue;
		sl_thd_stop(i);
	}

	hypercall_comp_reboot(s);
	/* TODO: any capmgr reboot? */

	/* TODO: introspect, reset ip/sp and resume thread appropriately. */
	ret = capmgr_thd_reset_entry(s, sl_thd_thdid(cfe_bookkeep_initthd(s)), 0, 0, &r2, &r3);
	sl_thd_resume(sl_thd_thdid(cfe_bookkeep_initthd(s)));

	/* TODO: resume all threads! */

	return 1;
}

void
reboot_req_fn(arcvcap_t r, void *d)
{
	int *ret;
	unsigned int *head, *tail;
	vaddr_t addr = 0;
	cbuf_t id = channel_shared_page_alloc(CFE_REBOOT_REQ_KEY, &addr);

	assert(id && addr);
	PRINTC("Created reboot request shm: %u\n", id);
	memset((void *)addr, 0, PAGE_SIZE);
	head = (unsigned int *)addr;
	tail = (unsigned int *)(addr + 4);
	ret = (int *)(addr + 8);

	while (1) {
		int pending = cos_rcv(r, 0, NULL);

		assert(*head < MAX_REQS && *tail < MAX_REQS);
		while (*head < *tail) {
			spdid_t s = *((spdid_t *)(addr + 12 + ((*head)*sizeof(spdid_t))));

			PRINTC("Request for reboot: %u\n", s);
			assert(s);
			*ret = reboot_comp(s);

			PRINTC("Reboot done!\n");
			(*head)++;
			assert(*head < MAX_REQS && *tail < MAX_REQS);
		}
	}
}


