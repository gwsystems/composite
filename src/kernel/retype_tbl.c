/**
 * Copyright 2014 by Qi Wang, interwq@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 */

#include "include/retype_tbl.h"
#include "include/chal.h"
#include "include/shared/cos_types.h"
#include "include/shared/cos_errno.h"
#include "cc.h"
#include <assert.h>

struct retype_info     retype_tbl[NUM_CPU] CACHE_ALIGNED;
struct retype_info_glb glb_retype_tbl[N_MEM_SETS] CACHE_ALIGNED;

/* called to increment or decrement the refcnt. */
static inline int
mod_ref_cnt(void *pa, const int op, const int type_check)
{
	u32_t                idx, old_v;
	struct retype_entry *retype_entry;
	union refcnt_atom    local_u;

	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	retype_entry = GET_RETYPE_ENTRY(idx);
	old_v = local_u.v = retype_entry->refcnt_atom.v;
	/* only allow to ref user or kernel typed memory set */
	if (unlikely((local_u.type != RETYPETBL_USER) && (local_u.type != RETYPETBL_KERN))) return -EPERM;

	/* Do type check if type passed in. */
	if (type_check >= 0 && type_check != local_u.type) return -EPERM;
	if (local_u.ref_cnt == RETYPE_REFCNT_MAX) return -EOVERFLOW;
	if (op) {
		local_u.ref_cnt = local_u.ref_cnt + 1;
	} else {
		local_u.ref_cnt = local_u.ref_cnt - 1;
		rdtscll(retype_entry->last_unmap);
		cos_mem_fence();
		//		cos_inst_bar();
	}
	cos_mem_fence();

	if (retypetbl_cas(&(retype_entry->refcnt_atom.v), old_v, local_u.v) != CAS_SUCCESS) return -ECASFAIL;

	return 0;
}

/* This only does reference for kernel typed memory. */
int
retypetbl_kern_ref(void *pa)
{
	return mod_ref_cnt(pa, 1, RETYPETBL_KERN);
}

int
retypetbl_ref(void *pa)
{
	return mod_ref_cnt(pa, 1, -1);
}

int
retypetbl_deref(void *pa)
{
	return mod_ref_cnt(pa, 0, -1);
}

static inline int
mod_mem_type(void *pa, const mem_type_t type)
{
	int                     i, ret;
	u32_t                   idx, old_type;
	struct retype_info_glb *glb_retype_info;

	assert(pa); /* cannot be NULL: kernel image takes that space */
	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	glb_retype_info = GET_GLB_RETYPE_ENTRY(idx);
	old_type        = glb_retype_info->type;

	/* only can retype untyped mem sets. */
	if (unlikely(old_type != RETYPETBL_UNTYPED)) {
		if (old_type == type)
			return -EEXIST;
		else
			return -EPERM;
	}

	/* Kernel memory needs to be kernel accessible: pa2va returns
	 * null if it's not. */
	if (type == RETYPETBL_KERN && chal_pa2va((paddr_t)pa) == NULL) return -EINVAL;

	ret = retypetbl_cas(&(glb_retype_info->type), RETYPETBL_UNTYPED, RETYPETBL_RETYPING);
	if (ret != CAS_SUCCESS) return -ECASFAIL;
	cos_mem_fence();

	/* Set the retyping flag successfully. Now nobody else can
	 * change this memory set. Update the per-core retype entries
	 * next. */
	for (i = 0; i < NUM_CPU; i++) {
		retype_tbl[i].mem_set[idx].refcnt_atom.type = type;
	}
	cos_mem_fence();

	/* Now commit the change to the global entry. */
	ret = retypetbl_cas(&(glb_retype_info->type), RETYPETBL_RETYPING, type);
	assert(ret == CAS_SUCCESS);

	return 0;
}

int
retypetbl_retype2user(void *pa)
{
	return mod_mem_type(pa, RETYPETBL_USER);
}

int
retypetbl_retype2kern(void *pa)
{
	return mod_mem_type(pa, RETYPETBL_KERN);
}

/* implemented in pgtbl.c */
int tlb_quiescence_check(u64_t unmap_time);

int
retypetbl_retype2frame(void *pa)
{
	struct retype_info_glb *glb_retype_info;
	union refcnt_atom       local_u;
	u32_t                   old_v, idx;
	u64_t                   last_unmap;
	int                     cpu, ret, ref_sum;
	mem_type_t              old_type;

	PA_BOUNDARY_CHECK();

	idx = GET_MEM_IDX(pa);
	assert(idx < N_MEM_SETS);

	glb_retype_info = GET_GLB_RETYPE_ENTRY(idx);
	old_type        = glb_retype_info->type;
	/* only can retype untyped mem sets. */
	if (unlikely(old_type != RETYPETBL_USER && old_type != RETYPETBL_KERN)) return -EPERM;

	/* lock down the memory set we are going to retype. */
	ret = retypetbl_cas(&(glb_retype_info->type), old_type, RETYPETBL_RETYPING);
	if (ret != CAS_SUCCESS) return ret;

	last_unmap = 0;
	for (ref_sum = 0, cpu = 0; cpu < NUM_CPU; cpu++) {
		/* Keep in mind that, ref_cnt on each core could be
		 * negative. */
		old_v = local_u.v = retype_tbl[cpu].mem_set[idx].refcnt_atom.v;
		assert(local_u.type == old_type || local_u.type == RETYPETBL_RETYPING);

		local_u.type = RETYPETBL_RETYPING;
		ret          = retypetbl_cas(&(retype_tbl[cpu].mem_set[idx].refcnt_atom.v), old_v, local_u.v);
		if (ret != CAS_SUCCESS) cos_throw(restore_all, -ECASFAIL);

		ref_sum += local_u.ref_cnt;
		/* for tlb quiescence check */
		if (last_unmap < retype_tbl[cpu].mem_set[idx].last_unmap)
			last_unmap = retype_tbl[cpu].mem_set[idx].last_unmap;
	}
	/* only can retype when there's no more mapping */
	if (ref_sum != 0) cos_throw(restore_all, -EINVAL);

	/* before retype any type of memory back to untyped, we need
	 * to make sure TLB quiescence has been achieved after the
	 * last unmapping of any pages in this memory set. */
	if (!tlb_quiescence_check(last_unmap)) cos_throw(restore_all, -EQUIESCENCE);
	cos_mem_fence();

	/**************************/
	/**** Legit to retype! ****/
	/**************************/
	/* we already locked all entries. feel free to change here. */
	for (cpu = 0; cpu < NUM_CPU; cpu++) {
		retype_tbl[cpu].mem_set[idx].refcnt_atom.ref_cnt = 0;
		retype_tbl[cpu].mem_set[idx].refcnt_atom.type    = RETYPETBL_UNTYPED;
	}
	cos_mem_fence();

	/* and commit to the global entry. */
	ret = retypetbl_cas(&(glb_retype_info->type), RETYPETBL_RETYPING, RETYPETBL_UNTYPED);
	assert(ret == CAS_SUCCESS);

	return 0;
restore_all:
	/* Something went wrong: CAS, TLB quiescence or ref_sum
	 * non-zero. Clean-up here.*/

	/* restore per-core retype entries. (is this necessary?) */
	/* cpu is the one we tried to modify but fail. So we need to
	 * restore [0, cpu-1]. */
	for (cpu--; cpu >= 0; cpu--) {
		old_v = local_u.v = retype_tbl[cpu].mem_set[idx].refcnt_atom.v;
		local_u.type      = old_type;
		retypetbl_cas(&(retype_tbl[cpu].mem_set[idx].refcnt_atom.v), old_v, local_u.v);
	}
	/* and global entry. */
	{
		/* Restore the global entry to old type before return:
		 * only us can change it. */
		int ret_local = retypetbl_cas(&(glb_retype_info->type), RETYPETBL_RETYPING, old_type);
		assert(ret_local == CAS_SUCCESS);
	}

	return ret;
}

void
retype_tbl_init(void)
{
	int i, j;

	/* Alignment & size checks! */
	assert(sizeof(struct retype_info) % CACHE_LINE == 0);
	assert(sizeof(struct retype_info_glb) % CACHE_LINE == 0);
	assert(sizeof(retype_tbl) % CACHE_LINE == 0);
	assert(sizeof(glb_retype_tbl) % CACHE_LINE == 0);
	assert((int)retype_tbl % CACHE_LINE == 0);
	assert((int)glb_retype_tbl % CACHE_LINE == 0);

	assert(sizeof(union refcnt_atom) == sizeof(u32_t));
	assert(RETYPE_ENT_TYPE_SZ + RETYPE_ENT_REFCNT_SZ == 32);
	assert(CACHE_LINE % sizeof(struct retype_entry) == 0);

	for (i = 0; i < NUM_CPU; i++) {
		for (j = 0; j < N_MEM_SETS; j++) {
			retype_tbl[i].mem_set[j].refcnt_atom.type    = RETYPETBL_UNTYPED;
			retype_tbl[i].mem_set[j].refcnt_atom.ref_cnt = 0;
			retype_tbl[i].mem_set[j].last_unmap          = 0;
		}
	}

	for (i = 0; i < N_MEM_SETS; i++) {
		glb_retype_tbl[i].type = RETYPETBL_UNTYPED;
	}

	cos_mem_fence();

	return;
}
