#ifndef AED_IOCTL_H
#define AED_IOCTL_H

/*
 * This is the task_struct counterpart in the executive.  It contains
 * the child's register state which is copied to the kernel when
 * invoking that child, the file descriptor pointing to the correct
 * address space for the child, the child's id (like the pid), and a
 * generic data section which can be defined per executive that the
 * kernel will ignore.  Example uses of data are to hold a mapping
 * table of file descriptors in the guest to those in the executive.
 */
typedef struct child_context {
	int id;
	int procmm_fd;
	struct pt_regs regs;
	void *data;
} child_context_t;

/*
 * Communication structure from the executive to the kernel containing
 * the address range taken by the executive.
 */
typedef struct executive_mem_limit {
	unsigned long lower, size;
} executive_mem_limit_t;

struct mmap_args {
	void *start;
	int length;
	int prot;
	int flags;
	int fd;
	int offset;
};

#include "../../../kernel/include/shared/cos_types.h"
struct cap_info {
	int cap_handle, rel_offset;
	int owner_spd_handle, dest_spd_handle;
	isolation_level_t il;
	int flags;
	vaddr_t ST_serv_entry;
	vaddr_t SD_cli_stub, SD_serv_stub;
	vaddr_t AT_cli_stub, AT_serv_stub;
};

struct spd_info {
	int spd_handle, num_caps;
	vaddr_t ucap_tbl;
	unsigned long lowest_addr;
	unsigned long size;
	unsigned long mem_size;
	vaddr_t upcall_entry;
	vaddr_t atomic_regions[10];
};

struct cos_thread_info {
	int spd_handle, sched_handle;
};

struct spd_sched_info {
	int spd_sched_handle, spd_parent_handle;
	vaddr_t sched_shared_page;
};

#define AED_PROMOTE_TRUSTED _IO(0, 1)
#define AED_DEMOTE_TRUSTED  _IO(0, 2)
#define AED_TESTING         _IO(0, 3)
#define AED_GET_REGSTATE    _IOW(0, 4, unsigned long)
#define AED_CTXT_SWITCH     _IOR(0, 5, unsigned long)
#define AED_COPY_MM         _IOR(0, 6, unsigned long)
#define AED_PROMOTE_EXECUTIVE _IOR(0, 7, unsigned long)
#define AED_SWITCH_MM       _IOR(0, 8, unsigned long)
#define AED_CREATE_MM       _IO(0, 9)
#define AED_EXECUTIVE_MMAP  _IOR(0, 10, unsigned long)

/* composite additions */
#define AED_TEST            _IOR(0, 11, unsigned long)

#define AED_CREATE_SPD      _IOR(0, 12, unsigned long)
#define AED_SPD_ADD_CAP     _IOR(0, 13, unsigned long)
#define AED_CREATE_THD      _IOR(0, 14, unsigned long)
#define AED_CAP_CHANGE_ISOLATION _IOR(0, 15, unsigned long)
#define AED_PROMOTE_SCHED   _IOR(0, 16, unsigned long)
#define AED_EMULATE_PREEMPT _IOR(0, 17, unsigned long)
#define AED_DISABLE_SYSCALLS _IO(0,19)
#define AED_ENABLE_SYSCALLS  _IO(0,20)
#define AED_RESTORE_HW_ENTRY _IO(0,21)
#define AED_INIT_BOOT       _IOR(0, 22, unsigned long)
#define AED_INIT_BOOT_THD    _IO(0, 23)


#ifndef __KERNEL__

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/ioctl.h>

static inline int aed_open_cntl_fd()
{
	int fd = open("/proc/aed", O_RDONLY);
	
	/* 
	 * This should really be dealt with in the function that uses
	 * it, but for now, I want to fix all the test cases in one
	 * fell swoop.
	 */
	if (fd < 0) {
		printf("Module aed not inserted.  Please load.\n");
		exit(-1);
	}

	return fd;
}


/* composite fns */

static inline int cos_create_spd(int cntl_fd, struct spd_info *spdi)
{
	int ret;

	if ((ret = ioctl(cntl_fd, AED_CREATE_SPD, spdi)) < 0) {
		perror("Could not create spd\n");
		printf("ioctl returned %d\n", ret);
		exit(-1);
	}

	return ret;
}

static inline int cos_init_booter(int cntl_fd, struct spd_info *spdi)
{
	int ret;

	if ((ret = ioctl(cntl_fd, AED_INIT_BOOT, spdi)) < 0) {
		perror("Could not initialize llbooter\n");
		printf("ioctl returned %d\n", ret);
		exit(-1);
	}

	return ret;
}

static inline int cos_create_init_thd(int cntl_fd)
{
	int ret;

	if ((ret = ioctl(cntl_fd, AED_INIT_BOOT_THD, 0)) < 0) {
		perror("Could not create init thread for llbooter\n");
		printf("ioctl returned %d\n", ret);
		exit(-1);
	}

	return ret;
}

static inline int cos_spd_add_cap(int cntl_fd, struct cap_info *capi)
{
 	int ret;

	if ((ret = ioctl(cntl_fd, AED_SPD_ADD_CAP, capi)) < 0) {
		perror("Could not create capability\n");
		printf("ioctl returned %d\n", ret);
		exit(-1);
	}

	return ret;
}

static inline int cos_create_thd(int cntl_fd, struct cos_thread_info *thdi)
{
 	int ret;
	
	if ((ret = ioctl(cntl_fd, AED_CREATE_THD, thdi))) {
		perror("Could not create thread\n");
		printf("ioctl returned %d\n", ret);
		exit(-1);
	}

	return ret;
}

static inline int cos_restore_hw_entry(int cntl_fd)
{
	int ret;

	if ((ret = ioctl(cntl_fd, AED_RESTORE_HW_ENTRY, 0)) == -1) {
		perror("ioctl to restore hw entries: ");
		printf("\nIoctl returned %d.\n", ret);
		exit(-1);
	}

	return ret;
}

static inline int cos_promote_to_scheduler(int cntl_fd, int sched_handle, 
					   int parent_sched_handle, vaddr_t notification_page)
{
 	int ret;
	struct spd_sched_info spd_sched = {
		.spd_sched_handle = sched_handle, 
		.spd_parent_handle = parent_sched_handle,
		.sched_shared_page = notification_page
	};

	if ((ret = ioctl(cntl_fd, AED_PROMOTE_SCHED, &spd_sched))) {
		perror("Could not promote scheduler\n");
		printf("ioctl returned %d\n", ret);
		exit(-1);
	}

	return ret;
}

/* end composite fns */

/*
 * FIXME: All the following translation functions (from api to ioctl
 * calls) are crap because errors cause exits.  This should really
 * really really be fixed to make this api usable.
 */

static inline int create_new_mm(int cntl_fd)
{
	int mm_fd = ioctl(cntl_fd, AED_CREATE_MM, mm_fd);

	if (mm_fd < 0) {
		perror("error creating new mm: ");
		exit(-1);
	}

	return mm_fd;
}

static inline void release_mm(int cntl_fd, int mm_fd)
{
	/* FIXME: make ioctl for releasing mms  */
	return;
}

static inline int get_new_mm()
{
	//int mm_fd;

	printf("Warning: get_new_mm is an old API.  "
	       "Use create_new_mm(cntl_fd) instead.\n");

	return -1;
/*
	if ((mm_fd = open("/proc/aedmm", O_RDWR)) == -1) {
		perror("opening /proc/aedmm");
		exit(-1);
	}

	return mm_fd;
*/
}

/* composite addition */
static inline void aed_set_test_value(int cntl_fd, unsigned long val)
{
	int ret;

	if ((ret = ioctl(cntl_fd, AED_TEST, val))) {
		perror("Could not switch mm.\n");
		printf("ioctl returned %d", ret);
		exit(-1);
	}

	return;
}

static inline void aed_switch_mm(int cntl_fd, int mm_fd)
{
	int ret;

	if ((ret = ioctl(cntl_fd, AED_SWITCH_MM, mm_fd))) {
		perror("Could not switch mm: ");
		printf("\nioctl returned %d", ret);
		exit(-1);
	}

	return;
}

static inline void aed_promote_to_executive(int cntl_fd, unsigned long lower_addr, unsigned long size)
{
	executive_mem_limit_t trusted_mem;
	int ret;

	trusted_mem.lower = lower_addr;
	trusted_mem.size  = size;
	
	if ((ret = ioctl(cntl_fd, AED_PROMOTE_EXECUTIVE, &trusted_mem))) {
		perror("Could not promote to executive.\n");
		printf("ioctl returned %d", ret);
		exit(-1);
	}

	return;
}

static inline void aed_get_reg_state(int cntl_fd, child_context_t *ct)
{
	int ret;

	if ((ret = ioctl(cntl_fd, AED_GET_REGSTATE, &(ct->regs)))) {
		perror("ioctl on aed regstate");
		printf("Ioctl returned %d.\n", ret);
		exit(-1);
	}

	return;
}

static inline void aed_copy_current_mm(int cntl_fd, int mm_fd)
{
	int ret;

	/* copy the current memory state into the new mm */
	if ((ret = ioctl(cntl_fd, AED_COPY_MM, mm_fd))) {
		perror("ioctl on aed copy mm");
		printf("Ioctl returned %d.\n", ret);
		exit(-1);
	}
	
	return;
}

static inline void aed_ctxt_switch_ioctl(int cntl_fd, child_context_t *ct)
{
	int ret;
	if ((ret = ioctl(cntl_fd, AED_CTXT_SWITCH, ct))) {
		perror("ioctl on aed ctxt switch: ");
		printf("\nIoctl returned %d.\n", ret);
		exit(-1);
	}
	
	return;
}

static inline void *aed_mmap(int cntl_fd, void *start, size_t length, 
			     int prot, int flags, int fd, off_t offset)
{
	struct mmap_args args;
	int ret;

	args.start = start;
	args.length = length;
	args.prot = prot;
	args.flags = flags;
	args.fd = fd;
	args.offset = offset;

	if ((ret = ioctl(cntl_fd, AED_EXECUTIVE_MMAP, &args)) == -1) {
		perror("ioctl on aed mmap: ");
		printf("\nIoctl returned %d.\n", ret);
		exit(-1);
	}

	return (void*)ret;
}

static inline int aed_munmap(void *start, size_t len)
{
	printf("aed_munmap not currently implemented. "
	       "See aed_mmap for possible implementation strategies.\n");

	/* errno = EINVAL; */
	return -1;
}

static inline void aed_disable_syscalls(int cntl_fd)
{
	int ret;

	if ((ret = ioctl(cntl_fd, AED_DISABLE_SYSCALLS, 0)) == -1) {
		perror("ioctl to disable syscalls: ");
		printf("\nIoctl returned %d.\n", ret);
		exit(-1);
	}

	return;
}


static inline void aed_enable_syscalls(int cntl_fd)
{
	int ret;

	if ((ret = ioctl(cntl_fd, AED_ENABLE_SYSCALLS, 0)) == -1) {
		perror("ioctl to enable syscalls: ");
		printf("\nIoctl returned %d.\n", ret);
		exit(-1);
	}

	return;
}

#endif /* __KERNEL__ */

#endif /* AED_IOCTL_H */
