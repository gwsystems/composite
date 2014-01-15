/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_component.h>

//long stkmgr_stack_space[ALL_TMP_STACKS_SZ];

/* FIXME: we want to get rid of this page, which was used for the
 * cos_sched_data_area. But for some reason the system won't load if
 * we remove this page. */
char temp[4096] __attribute__((aligned(4096)));

int cos_sched_notifications __attribute__((weak));

__attribute__ ((weak))
void cos_init(void *arg)
{
	return;
}

__attribute__ ((weak))
void __alloc_libc_initilize(void)
{
	return;
}

__attribute__ ((weak))
void cos_upcall_exec(void *arg)
{
	return;
}

__attribute__ ((weak))
int cos_async_inv(struct usr_inv_cap *ucap, int *params) 
{
	return 0;
}

__attribute__ ((weak))
void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BOOTSTRAP:
	{
		static int first = 1;
		if (first) { 
			first = 0; 
			__alloc_libc_initilize(); 
			constructors_execute();
		}
		cos_init(arg1);
		break;
	}
	default:
		/* fault! */
		*(int*)NULL = 0;
		return;
	}
	return;
}

__attribute__ ((weak))
int main(void)
{
	return 0;
}

__attribute__((weak)) 
void *cos_get_vas_page(void)
{
	char *h;
	long r;
	do {
		h = cos_get_heap_ptr();
		r = (long)h+PAGE_SIZE;
	} while (cos_cmpxchg(&cos_comp_info.cos_heap_ptr, (long)h, r) != r);
	return h;
}

__attribute__((weak)) 
void cos_release_vas_page(void *p)
{
	cos_set_heap_ptr_conditional(p + PAGE_SIZE, p);
}

extern const vaddr_t cos_atomic_cmpxchg, cos_atomic_cmpxchg_end, 
	cos_atomic_user1, cos_atomic_user1_end, 
	cos_atomic_user2, cos_atomic_user2_end, 
	cos_atomic_user3, cos_atomic_user3_end, 
	cos_atomic_user4, cos_atomic_user4_end;
extern const vaddr_t cos_upcall_entry;

extern const vaddr_t cos_ainv_entry;

__attribute__((weak)) vaddr_t ST_user_caps;

/* 
 * Much of this is either initialized at load time, or passed to the
 * loader though this structure.
 */
struct cos_component_information cos_comp_info __attribute__((section(".cinfo"))) = {
	.cos_this_spd_id = 0,
	.cos_heap_ptr = 0,
	.cos_heap_limit = 0,
	.cos_stacks.freelists[0] = {.freelist = 0, .thd_id = 0},
	.cos_upcall_entry = (vaddr_t)&cos_upcall_entry,
	.cos_async_inv_entry = (vaddr_t)&cos_ainv_entry,
	.cos_user_caps = (vaddr_t)&ST_user_caps,
	.cos_ras = {{.start = (vaddr_t)&cos_atomic_cmpxchg, .end = (vaddr_t)&cos_atomic_cmpxchg_end}, 
		    {.start = (vaddr_t)&cos_atomic_user1, .end = (vaddr_t)&cos_atomic_user1_end},
		    {.start = (vaddr_t)&cos_atomic_user2, .end = (vaddr_t)&cos_atomic_user2_end},
		    {.start = (vaddr_t)&cos_atomic_user3, .end = (vaddr_t)&cos_atomic_user3_end},
		    {.start = (vaddr_t)&cos_atomic_user4, .end = (vaddr_t)&cos_atomic_user4_end}},
	.cos_poly = {0, }
};
