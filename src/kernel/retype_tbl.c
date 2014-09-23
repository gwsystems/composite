#include "include/retype_tbl.h"
#include "include/shared/cos_types.h"

struct retype_info     retype_tbl[NUM_CPU]        CACHE_ALIGNED;
struct retype_info_glb glb_retype_tbl[N_MEM_SETS] CACHE_ALIGNED;

void
retype_tbl_init(void)
{
	int i, j;

	/* Alignment & size checks! */
	assert(sizeof(struct retype_info)     % CACHE_LINE == 0);
	assert(sizeof(struct retype_info_glb) % CACHE_LINE == 0);
	assert(sizeof(retype_tbl)     % CACHE_LINE == 0);
	assert(sizeof(glb_retype_tbl) % CACHE_LINE == 0);
	assert((int)retype_tbl     % CACHE_LINE == 0);
	assert((int)glb_retype_tbl % CACHE_LINE == 0);

	assert(sizeof(union refcnt_atom) == sizeof(u32_t));
	assert(RETYPE_ENT_TYPE_SZ + RETYPE_ENT_REFCNT_SZ == 32);

	for (i = 0; i < NUM_CPU; i++) {
		for (j = 0; j < N_MEM_SETS; j++) {
			retype_tbl[i].mem_set[j].refcnt_atom.type    = MEM_UNTYPED;
			retype_tbl[i].mem_set[j].refcnt_atom.ref_cnt = 0;
			retype_tbl[i].mem_set[j].last_unmap          = 0;
		}
	}

	for (i = 0; i < N_MEM_SETS; i++) {
		glb_retype_tbl[i].type = MEM_UNTYPED;
	}

	cos_mem_fence();
	
	return;
}
