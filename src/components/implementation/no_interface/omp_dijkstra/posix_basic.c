#include <math.h>
#include <syscall.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/utsname.h>
#include <time.h>

#include <cos_component.h>
#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <cos_types.h>
#include <ps.h>

/*
 * hack for more memory using the most insecure feature in composite: 
 * map random physical addresses to virtual addresses and do whatever with it!
 */
#define START_PHY round_up_to_page(0x00100000 + COS_PHYMEM_MAX_SZ + PAGE_SIZE)
#define PHY_MAX ((512 * 1024 * 1024) + (256 * 1024 * 1024))

static unsigned free_phy_offset = 0;

void *
__alloc_memory(size_t sz)
{
	void *va = NULL;
	struct cos_compinfo *ci = cos_compinfo_get(cos_defcompinfo_curr_get());
	//unsigned off = ps_faa(&free_phy_offset, sz);
	unsigned off;

try_again:
	off = ps_load(&free_phy_offset);

	/* 
	 * first use physical memory hack and 
	 * if we run out, then use heap alloc so 
	 * we don't run out of standard memory first 
	 */
	if (off > PHY_MAX || off + sz > PHY_MAX) {
		va = cos_page_bump_allocn(ci, round_up_to_page(sz));
	} else {
		if (!ps_cas(&free_phy_offset, off, off + sz)) goto try_again;
		/* use physical memory hack! */
		va = cos_hw_map(ci, BOOT_CAPTBL_SELF_INITHW_BASE, START_PHY + off, sz);
	}

	assert(va);
	memset(va, 0, sz);

	return va;
}

//#include <memmgr.h>

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

	//addr = (void *)memmgr_heap_page_allocn(pages);
	addr = __alloc_memory(length);
//	addr = (void *)cos_page_bump_allocn(cos_compinfo_get(cos_defcompinfo_curr_get()), round_up_to_page(length));
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

	if (syscall_num == __NR_brk || syscall_num == __NR_munmap) {
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
