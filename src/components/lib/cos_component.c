/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */
#include <stdio.h>

#include <sys/auxv.h>

#include <consts.h>
#include <cos_component.h>
#include <cos_debug.h>
#include <cos_kernel_api.h>

#include <ps.h>

CWEAKSYMB int cos_sched_notifications;

CWEAKSYMB int
main(void)
{
	return 0;
}

CWEAKSYMB void
cos_init(void *arg)
{
	main();
}

/* Intended to be implement by libraries */
CWEAKSYMB void
pre_syscall_default_setup()
{
}

/* Intended to be overriden by components */
CWEAKSYMB void
pre_syscall_setup()
{
	pre_syscall_default_setup();
}

CWEAKSYMB void
syscall_emulation_setup()
{
}

CWEAKSYMB long
cos_syscall_handler(int syscall_num, long a, long b, long c, long d, long e, long f, long g)
{
	printc("Default syscall handler callled (syscall: %d), faulting!", syscall_num);
	assert(0);
	return 0;
}

__attribute__((regparm(1))) long
__cos_syscall(int syscall_num, long a, long b, long c, long d, long e, long f, long g)
{
	return cos_syscall_handler(syscall_num, a, b, c, d, e, f, g);
}

CWEAKSYMB void
libc_initialization_handler()
{
}

/* TODO: Make this a weak symbol (currently doing so makes this fail) */
void __init_libc(char **envp, char *pn);

void
libc_init()
{
	/* The construction of this is:
	 * evn1, env2, ..., NULL, auxv_n1, auxv_1, auxv_n2, auxv_2 ..., NULL
	 * TODO: Figure out a way to set AT_HWCAP / AT_SYSINFO
	 */
	static char *envp[] = {
                               "USER=composite_user",
                               "LANG=en_US.UTF-8",
                               "HOME=/home/composite_user",
                               "LOGNAME=composite_user",
                               NULL,
                               (char *)AT_PAGESZ,
                               (char *)PAGE_SIZE, /* Page size */
                               (char *)AT_UID,
                               (char *)1000, /* User id */
                               (char *)AT_EUID,
                               (char *)1000, /* Effective user id */
                               (char *)AT_GID,
                               (char *)1000, /* Group id */
                               (char *)AT_EGID,
                               (char *)1000, /* Effective group id */
                               (char *)AT_SECURE,
                               (char *)0, /* Whether the program is being run under sudo */
                               NULL
	};
	char *program_name = "composite component";
	__init_libc(envp, program_name);
}

CWEAKSYMB void
cos_upcall_exec(void *arg)
{
}

CWEAKSYMB int
cos_async_inv(struct usr_inv_cap *ucap, int *params)
{
	return 0;
}

CWEAKSYMB int
cos_thd_entry_static(u32_t idx)
{
	assert(0);
	return 0;
}

const char *cos_print_str[PRINT_LEVEL_MAX] = {
	"ERR:",
	"WARN:",
	"DBG:",
};

cos_print_level_t cos_print_level   = PRINT_ERROR;
int               cos_print_lvl_str = 0;

CWEAKSYMB void
cos_print_level_set(cos_print_level_t lvl, int print_str)
{
	cos_print_level   = lvl;
	cos_print_lvl_str = print_str;
}

/*
 * Cos thread creation data structures.
 */
struct __thd_init_data __thd_init_data[COS_THD_INIT_REGION_SIZE] CACHE_ALIGNED;

static void
cos_thd_entry_exec(u32_t idx)
{
	void (*fn)(void *);
	void *data;

	fn   = __thd_init_data[idx].fn;
	data = __thd_init_data[idx].data;
	/* and release the entry... might need a barrier here. */
	__thd_init_data[idx].data = NULL;
	__thd_init_data[idx].fn   = NULL;

	(fn)(data);
}

CWEAKSYMB void
cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	static int first = 1;

	if (first) {
		first = 0;
		cos_print_level_set(PRINT_DEBUG, 1);
		/* The syscall enumlator might need something to be setup before it can work */
		pre_syscall_setup();
		/* libc needs syscall emulation to work */
		syscall_emulation_setup();
		/* With all that setup, we can invoke the libc_initialization_handler */
		libc_initialization_handler();

		constructors_execute();
	}

	/*
	 * if it's the first component.. wait for timer calibration
	 * NOTE: for "fork"ing components and not updating "spdid"s, this call will just fail and should be fine.
	 */
	if (cos_spd_id() == 0) {
		cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
	}

	switch (t) {
	case COS_UPCALL_THD_CREATE:
		/* New thread creation method passes in this type. */
		{
			/* A new thread is created in this comp. */

			/* arg1 is the thread init data. 0 means
			 * bootstrap. */
			if (arg1 == 0) {
				cos_init(NULL);
			} else {
				u32_t idx = (int)arg1 - 1;
				if (idx >= COS_THD_INIT_REGION_SIZE) {
					/* This means static defined entry */
					cos_thd_entry_static(idx - COS_THD_INIT_REGION_SIZE);
				} else {
					/* Execute dynamic allocated entry. */
					cos_thd_entry_exec(idx);
				}
			}
			return;
		}
	default:
		/* fault! */
		assert(0);
		return;
	}
	return;
}

CWEAKSYMB void *
cos_get_vas_page(void)
{
	char *h;
	long  r;
	do {
		h = cos_get_heap_ptr();
		r = (long)h + PAGE_SIZE;
	} while (cos_cmpxchg(&cos_comp_info.cos_heap_ptr, (long)h, r) != r);
	return h;
}

CWEAKSYMB void
cos_release_vas_page(void *p)
{
	cos_set_heap_ptr_conditional(p + PAGE_SIZE, p);
}

extern const vaddr_t cos_atomic_cmpxchg, cos_atomic_cmpxchg_end, cos_atomic_user1, cos_atomic_user1_end,
  cos_atomic_user2, cos_atomic_user2_end, cos_atomic_user3, cos_atomic_user3_end, cos_atomic_user4,
  cos_atomic_user4_end;
extern const vaddr_t cos_upcall_entry;

extern const vaddr_t cos_ainv_entry;

CWEAKSYMB vaddr_t ST_user_caps;

/*
 * Much of this is either initialized at load time, or passed to the
 * loader though this structure.
 */
struct cos_component_information cos_comp_info __attribute__((
  section(".cinfo"))) = {.cos_this_spd_id         = 0,
                         .cos_heap_ptr            = 0,
                         .cos_heap_limit          = 0,
                         .cos_stacks.freelists[0] = {.freelist = 0, .thd_id = 0},
                         .cos_upcall_entry        = (vaddr_t)&cos_upcall_entry,
                         .cos_async_inv_entry     = (vaddr_t)&cos_ainv_entry,
                         .cos_user_caps           = (vaddr_t)&ST_user_caps,
                         .cos_ras  = {{.start = (vaddr_t)&cos_atomic_cmpxchg, .end = (vaddr_t)&cos_atomic_cmpxchg_end},
                                     {.start = (vaddr_t)&cos_atomic_user1, .end = (vaddr_t)&cos_atomic_user1_end},
                                     {.start = (vaddr_t)&cos_atomic_user2, .end = (vaddr_t)&cos_atomic_user2_end},
                                     {.start = (vaddr_t)&cos_atomic_user3, .end = (vaddr_t)&cos_atomic_user3_end},
                                     {.start = (vaddr_t)&cos_atomic_user4, .end = (vaddr_t)&cos_atomic_user4_end}},
                         .cos_poly = {
                           0,
                         }};

/* FIXME: ck linking says undefined. checked online and made sure -fno-PIC is set through --without-pic configuration but this still occurs. */
void *_GLOBAL_OFFSET_TABLE_ = NULL;
