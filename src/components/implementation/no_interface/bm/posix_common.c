#include <math.h>
#include <syscall.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <time.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_types.h>
#include <ps.h>

#include <memmgr.h>

FILE *FileOpen(const char *Path, const char *File, const char *CtrlCode)
{
    assert(0);
    return NULL;
}

// HACK: The hack to end all hacks
void *
cos_mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
	void *ret=0;

	if (addr != NULL) {
		printc("parameter void *addr is not supported!\n");
		errno = ENOTSUP;
		return MAP_FAILED;
	}
	if (fd != -1) {
		printc("file mapping is not supported!\n");
		errno = ENOTSUP;
		return MAP_FAILED;
	}

    int pages;
    if (length % 4096) {
        pages = length / 4096 + 1;
    } else {
        pages = length / 4096;
    }

	addr = (void *)memmgr_heap_page_allocn(pages);
	if (!addr){
		ret = (void *) -1;
	} else {
		ret = addr;
	}

	if (ret == (void *)-1) {  /* return value comes from man page */
		printc("mmap() failed!\n");
		/* This is a best guess about what went wrong */
		errno = ENOMEM;
	}
	return ret;
}

long
cos_syscall_handler(int syscall_num, long a, long b, long c, long d, long e, long f, long g)
{
   if (syscall_num == __NR_clock_gettime) {
        microsec_t microseconds = ps_tsc() / cos_hw_cycles_per_usec(BOOT_CAPTBL_SELF_INITHW_BASE);
        time_t seconds = microseconds / 1000000;
        long rest = microseconds % 1000000;

        *((struct timespec *)b) = (struct timespec) {seconds, rest};
        return 0;
    }

    if (syscall_num == __NR_mmap || syscall_num == __NR_mmap2) {
        return (long)cos_mmap((void *)a, (size_t)b, (int)c, (int)d, (int)e, (off_t)f);
    }

    if (syscall_num == __NR_brk) {
        return 0;
    }

    printc("Unimplemented syscall number %d\n", syscall_num);
    assert(0);
    return 0;
}

// Hack around thread local data
static int cancelstate = 0;

int
pthread_setcancelstate(int new, int *old)
{
    if (new > 2) return EINVAL;

    if (old) *old = cancelstate;
    cancelstate = new;
    return 0;
}
