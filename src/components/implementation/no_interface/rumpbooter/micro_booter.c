#include <stdio.h>
#include <string.h>
#include <cos_component.h>

#include <cobj_format.h>
#include <cos_kernel_api.h>

#include "rumpcalls.h"
#include "cos_init.h"

int
prints(char *s)
{
    int len = strlen(s);
	  cos_print(s, len);
	  return len;
}

int __attribute__((format(printf,1,2)))
printc(char *fmt, ...)
{
	  char s[128];
	  va_list arg_ptr;
	  int ret, len = 128;

	  va_start(arg_ptr, fmt);
	  ret = vsnprintf(s, len, fmt, arg_ptr);
	  va_end(arg_ptr);
	  cos_print(s, ret);

	  return ret;
}

#ifndef assert
/* On assert, immediately switch to the "exit" thread */
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); cos_thd_switch();} } while(0)
#endif

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

struct cos_compinfo booter_info;

static void
thd_fn(void *d)
{
	printc("\tNew thread %d with argument %d\n", cos_thdid(), (int)d);
	cos_thd_switch(BOOT_CAPTBL_SELF_INITTHD_BASE);
	printc("Error, shouldn't get here!\n");
}

#define TEST_NTHDS 5
static void
test_thds(void)
{
	thdcap_t ts[TEST_NTHDS];
	int i;

	for (i = 0 ; i < TEST_NTHDS ; i++) {
		ts[i] = cos_thd_alloc(&booter_info, booter_info.comp_cap, thd_fn, (void *)i);
		assert(ts[i]);
		printc("switchto %d\n", (int)ts[i]);
		cos_thd_switch(ts[i]);
	}

	printc("test done\n");
}

static void
test_mem(void)
{
	char *p = cos_page_bump_alloc(&booter_info);

	assert(p);
	strcpy(p, "victory");

	printc("Page allocation: %s\n", p);
}

struct data {
	thdcap_t prev; // Thread to switch back to
	unsigned short int thdid;
};

void
rumptest_thd_fn(void *param)
{
	struct data *thd_meta = (struct data*)param;

	printc("In rumptest_thd_fn\n");
	printc("thdid, should be 0: %d\n", thd_meta->thdid);

	printc("fetching thd id\n");
	thd_meta->thdid = cos_thdid();
	printc("thdid, is now: %d\n", thd_meta->thdid);

	printc("switching back to old thread");
	cos_thd_switch(thd_meta->prev);
	printc("Error: this should not print");
}

void
test_rumpthread(void)
{
	thdcap_t new_thdcap;
	thdcap_t current_thdcap;
	void *thd_meta;

	current_thdcap = BOOT_CAPTBL_SELF_INITTHD_BASE;

	struct data info;
	info.prev = current_thdcap;

	thd_meta = &info;
	//cos_thd_fn_t func_ptr = rumptest_thd_fn;

	new_thdcap = cos_thd_alloc(&booter_info, booter_info.comp_cap, rumptest_thd_fn, thd_meta);
	cos_thd_switch(new_thdcap);

	printc("switched back to old thread, thdid: %d\n", info.thdid);
}

void
cos_init(void)
{
	printc("\nMicro Booter started.\n");

	cos_meminfo_init(&booter_info.mi, BOOT_MEM_PM_BASE, COS_MEM_USER_PA_SZ,
			 BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ);
	cos_compinfo_init(&booter_info, BOOT_CAPTBL_SELF_PT, BOOT_CAPTBL_SELF_CT, BOOT_CAPTBL_SELF_COMP,
			  (vaddr_t)cos_get_heap_ptr(), BOOT_CAPTBL_FREE, &booter_info);

	test_thds();
	test_mem();

	printc("\nMicro Booter done.\n");

	printc("\nRump Sched Test Start\n");
	test_rumpthread();
	printc("\nRump Sched Test End\n");

	printc("\nRumpKernel Boot Start.\n");
	cos2rump_setup();
	cos_run(NULL);
	printc("\nRumpKernel Boot done.\n");

	BUG();

	return;
}
