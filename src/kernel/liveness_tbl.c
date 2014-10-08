#include "include/liveness_tbl.h"

struct liveness_entry __liveness_tbl[LTBL_ENTS];

void
ltbl_init(void)
{
	int i;

	for (i = 0 ; i < LTBL_ENTS ; i++) {
		__liveness_tbl[i].epoch = 0;
		__liveness_tbl[i].deact_timestamp = 0;
	}
}
