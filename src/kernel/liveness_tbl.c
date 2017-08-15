#include "include/liveness_tbl.h"

void
ltbl_init(void)
{
	int i;

	for (i = 0; i < LTBL_ENTS; i++) {
		__liveness_tbl[i].epoch           = 0;
		__liveness_tbl[i].deact_timestamp = 0;
	}
}
