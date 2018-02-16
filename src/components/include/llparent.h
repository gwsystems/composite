#ifndef LLPARENT_H
#define LLPARENT_H

#include <cos_component.h>
#include <cos_kernel_api.h>

extern void *__inv_llparent_entry(int a, int b, int c);

typedef enum {
	LLPARENT_CHILD_INIT = 0,
} llparent_op_t;

static inline int
llparent_child_init(void)
{
	/* invoke my parent and ask to init it's data-structs for my comp */
	return cos_sinv(BOOT_CAPTBL_PARENT_SINV_CAP, LLPARENT_CHILD_INIT, cos_spd_id(), 0, 0);
}

static inline int
__llparent_child_init_intern(spdid_t s)
{
	/* init done from "this" component's child "s" */
	return 0;
}

u32_t
llparent_entry(u32_t op, u32_t a, u32_t b, u32_t c, u32_t *r2, u32_t *r3)
{
	switch(op) {
	case LLPARENT_CHILD_INIT:
	{
		__llparent_child_init_intern(a);
		break;
	}
	default: assert(0);
	}
}

#endif /* LLPARENT_H */
