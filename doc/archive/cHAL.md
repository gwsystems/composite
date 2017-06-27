The _Composite_ Hardware Abstraction Layer
==========================================

The _Composite_ Hardware Abstraction Layer, or _Hijack_ Abstraction
Layer -- _cHAL_ -- is the layer that defines the platform-specific
functionality that requires specific implementations not only for
different architectures (e.g. x86-32 vs. -64), but also when booting
from the bare-metal versus using the Hijack techniques.  This file
documents the functions that must be implemented within the platform
code, and how they interact with the _Composite_ kernel proper.

This file is going to include many stages of the _cHAL_, starting from
rough notes on which functions are going to be in it, and proceeding
to a final stage that includes only the set of interfaces, and their
perscribed use and documentation.  The path to rational documentation
will be documented itself in the commit history.

Notes on the Hardware and Kernel Control Paths
----------------------------------------------

**Kernel call-backs.** The hardware-entry points for system calls,
faults, exceptions, and interrupts must be in the _cHAL_.  Only the
platform-specific code will know how to configure registers,
etc. (this raises the question of how we can have platform-specific
user-level code that understands the register passing conventions, but
that is for another day), let alone how to configure the hardware to
respect a given set of entry-points.  This implies that we must define
the interface within the _Composite_ kernel that is invoked by the
platform sepecific code.  This requirement spans beyond just the
hardware-specific entry points, and includes high-level interrupts
such as the timer interrupt, handling network interrupts, and handling
translator events.  However, this does define (as far as I know right
now) the full set of call-backs into the kernel from the
platorm-specific code.

**Platform services.** The set of services provided by platform
specific code are hinted at by the set of undefined functions within
the kernel.  See below for a list of these.


On Directory/Code Structure
---------------------------

We need not only to produce and object that the kernel can be linked
with that satisfies all of the kernel's undefined functions, but also
an include directory that includes not only the interface for the
platform services, but also some of their implementations.  The
services that are required to be implemented in header files is small,
and constitutes only the small set of functions required on the
invocation fast-path (for now, I only know about the
`switch_host_pg_tbls` function).  Unfortunately, this _does_ mean that
we can't just have a single static include directory that includes the
prototypes for the functional interface between kernel and platform
services, but also include files that include specific
implementations.  This will probably require a symlink for the include
directory.

The include directory should be in `src/platform/include`, with a
`src/platform/include/platform_specific.h` file that is a symlink to
the appropriate file in e.g. `src/platform/linux/`.  That file is
included from `chal.h`.

The `extern`ed Functions from `src/kernel/*`
------------------------------------------ 

**inv.c**
```
extern void switch_host_pg_tbls(paddr_t pt);
extern int pgtbl_add_entry(paddr_t pgtbl, vaddr_t vaddr, paddr_t paddr); 
extern vaddr_t pgtbl_vaddr_to_kaddr(paddr_t pgtbl, unsigned long addr);
extern paddr_t pgtbl_rem_ret(paddr_t pgtbl, vaddr_t va);
extern unsigned long __pgtbl_lookup_address(paddr_t pgtbl, unsigned long addr);
extern void __pgtbl_or_pgd(paddr_t pgtbl, unsigned long addr, unsigned long val);
extern void pgtbl_print_path(paddr_t pgtbl, unsigned long addr);
extern void
copy_pgtbl_range(paddr_t pt_to, paddr_t pt_from, 
		 unsigned long lower_addr, unsigned long size);
extern int host_can_switch_pgtbls(void);

extern int switch_thread_data_page(int old_thd, int new_thd);
extern int host_attempt_brand(struct thread *brand);
extern void *va_to_pa(void *va);
static const struct cos_trans_fns *trans_fns = NULL;
extern int user_struct_fits_on_page(unsigned long addr, unsigned int size);
extern int host_attempt_brand(struct thread *brand);
extern void host_idle(void);

extern int cos_syscall_idle(void);
extern int cos_syscall_switch_thread(void);
extern void cos_syscall_brand_wait(int spd_id, unsigned short int bid, int *preempt);
extern void cos_syscall_brand_upcall(int spd_id, int thread_id_flags);
extern int cos_syscall_buff_mgmt(void);
extern void cos_syscall_upcall(void);
```

**spd.c**
```
extern int spd_free_mm(struct spd *spd);

extern vaddr_t pgtbl_vaddr_to_kaddr(paddr_t pgtbl, unsigned long addr);
extern int pgtbl_add_middledir_range(paddr_t pt, unsigned long vaddr, long size);
extern int pgtbl_rem_middledir_range(paddr_t pt, unsigned long vaddr, long size);
extern void zero_pgtbl_range(paddr_t pt, unsigned long lower_addr, unsigned long size);
extern void copy_pgtbl_range(paddr_t pt_to, paddr_t pt_from,
			     unsigned long lower_addr, unsigned long size);
extern int pgtbl_entry_absent(paddr_t pt, unsigned long addr);
extern vaddr_t kern_pgtbl_mapping;
extern void copy_pgtbl_range_nocheck(paddr_t pt_to, paddr_t pt_from,
				     unsigned long lower_addr, unsigned long size);

extern void *va_to_pa(void *va);
extern void *pa_to_va(void *pa);
```

**thread.c**
```
extern void *va_to_pa(void *va);
extern void thd_publish_data_page(struct thread *thd, vaddr_t page);

extern int host_in_syscall(void);
extern int host_in_idle(void);
```
**mmap.c**
```
extern void *cos_alloc_page(void);
extern void *cos_free_page(void *page);

extern void *va_to_pa(void *va);
extern void *pa_to_va(void *pa);
```

***general***
```
memcpy
memset
printk
```

Programmatically Determining Undefined Symbols in the Kernel
------------------------------------------------------------

`cd src/kernel/; ld *.o -o all.o 2> undef.txt; grep "undefined reference to" undef.txt | sed "s/.*\<replace with backtick>\(.*\)'.*/\1/" | sort | uniq ; rm undef.txt`

results in the following list of undefined functions (@
`8d21a01302cd12e7fb32f73306d8a2a2a28b562e`):

```
copy_pgtbl_range
copy_pgtbl_range_nocheck
cos_alloc_page
cos_free_page
cos_syscall_brand_upcall
cos_syscall_brand_wait
cos_syscall_buff_mgmt
cos_syscall_idle
cos_syscall_switch_thread
cos_syscall_upcall
host_attempt_brand
host_can_switch_pgtbls
host_idle
host_in_idle
host_in_syscall
kern_pgtbl_mapping
memcpy
memset
pa_to_va
pgtbl_add_entry
pgtbl_add_middledir_range
pgtbl_entry_absent
pgtbl_rem_middledir_range
pgtbl_rem_ret
pgtbl_vaddr_to_kaddr
printk
switch_host_pg_tbls
switch_thread_data_page
thd_publish_data_page
va_to_pa
warn_slowpath_null
zero_pgtbl_range
```
