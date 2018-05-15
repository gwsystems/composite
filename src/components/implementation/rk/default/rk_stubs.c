#include <cos_component.h>
#include <string.h>
#include <rumpcalls.h>
#include <sys/socket.h>
#include <elf.h>
#include <stdio.h>

Elf64_Dyn _DYNAMIC [1];

#define PRINT_STUB(name) printc("RK_STUB: %s\n", name);

/* Define weak symbols that will be replaced with RK */
size_t bmk_memsize __attribute__((weak));
struct cos_rumpcalls crcalls __attribute__((weak));

void bmk_isr(int which) __attribute__((weak));
void bmk_mainthread(void *cmdline) __attribute__((weak));
void bmk_memalloc_init(void) __attribute__((weak));
void bmk_pgalloc_loadmem(unsigned long min, unsigned long max) __attribute__((weak));
void bmk_sched_init(void) __attribute__((weak));
void __attribute__((noreturn))
bmk_sched_startmain(void (*mainfun)(void *), void *args) __attribute__((weak));
thdid_t get_cos_thdid(struct bmk_thread *thread) __attribute__((weak));
ssize_t rump___sysimpl_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t fromlenaddr) __attribute__((weak));
ssize_t rump___sysimpl_sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen) __attribute__((weak));
int rump___sysimpl_socket30(int domain, int type, int protocol) __attribute__((weak));
void set_cos_thddata(struct bmk_thread *thread, capid_t thd, thdid_t tid) __attribute__((weak));

void bmk_isr(int which) { PRINT_STUB("bmk_isr") }
void bmk_mainthread(void *cmdline) { PRINT_STUB("bmk_mainthread") }
void bmk_memalloc_init(void) { PRINT_STUB("bmk_memalloc_init") }
void bmk_pgalloc_loadmem(unsigned long min, unsigned long max) { PRINT_STUB("bmk_pgalloc_loadmem") }
void bmk_sched_init(void) { PRINT_STUB("bmk_sched_init") }

void __attribute__((noreturn))
bmk_sched_startmain(void (*mainfun)(void *), void *args)
{
	PRINT_STUB("bmk_sched_startmain")
	while (1);
}

thdid_t
get_cos_thdid(struct bmk_thread *thread)
{
	PRINT_STUB("get_cos_thdid")
	return -1;
}

ssize_t
rump___sysimpl_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t fromlenaddr)
{
	PRINT_STUB("rump___sysimpl_recvfrom")
	return -1;
}

ssize_t
rump___sysimpl_sendto(int s, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
	PRINT_STUB("rump___sysimpl_sendto")
	return -1;
}

int
rump___sysimpl_socket30(int domain, int type, int protocol)
{
	PRINT_STUB("rump___sysimpl_socket30")
	return -1;
}

void set_cos_thddata(struct bmk_thread *thread, capid_t thd, thdid_t tid) { PRINT_STUB("set_cos_thddata") }

void
cos_llprint(char *s, int len)
{
        call_cap(PRINT_CAP_TEMP, (int)s, len, 0, 0);
}

int
prints(char *s)
{
        size_t len = strlen(s);

        cos_llprint(s, len);

        return len;
}

int  __attribute__((format(printf, 1, 2)))
printc(char *fmt, ...)
{
        char    s[128];
        va_list arg_ptr;
        size_t  ret, len = 128;

        va_start(arg_ptr, fmt);
        ret = vsnprintf(s, len, fmt, arg_ptr);
        va_end(arg_ptr);
        cos_llprint(s, ret);

        return ret;
}
