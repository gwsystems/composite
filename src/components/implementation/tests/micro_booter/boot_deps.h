#include <stdio.h>
#include <string.h>
#include <cos_component.h>
#include <sched_hier.h>

static int
prints(char *s)
{
    int len = strlen(s);
	  cos_print(s, len);
	  return len;
}

static int __attribute__((format(printf,1,2)))
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
#define assert(node) do { if (unlikely(!(node))) { debug_print("assert error in @ "); cos_switch_thread(PERCPU_GET(llbooter)->alpha, 0);} } while(0)
#endif

#define PRINT_FN prints
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))
#define BUG() do { debug_print("BUG @ "); *((int *)0) = 0; } while (0);

void cos_init(void);

#include <cpu_ghz.h>

int
sched_init(void)
{
	assert(cos_cpuid() < NUM_CPU_COS);

	if (cos_cpuid() == INIT_CORE) {
		cos_init();
		assert(PERCPU_GET(llbooter)->init_thd);
	}

	/* calling return cap */
	call_cap(0, 0, 0, 0, 0);

	return 0;
}

int
sched_isroot(void) { return 1; }

void
sched_exit(void) { BUG(); }

int
sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff)
{ BUG(); return 0; }

int
sched_child_cntl_thd(spdid_t spdid) { BUG(); return 0; }

int
sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd) { BUG(); return 0; }
