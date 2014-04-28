#include <liveness_tbl.h>

struct liveness_entry __liveness_tbl[LTBL_ENTS];

void
ltbl_init(void)
{
	int i;
	u64_t tsc;
	/* FIXME: rdtscll(tsc); */
	tsc = 0;
	for (i = 0 ; i < LTBL_ENTS ; i++) {
		__liveness_tbl[i].epoch = 0;
		__liveness_tbl[i].free_timestamp = tsc;
	}
}
