#include <vm_vcpu_context.h>
#include <thd.h>
#include <vmx.h>

/* Abstraction layer for VM support */
#ifdef CONFIG_VMX 

void
vm_env_init(void)
{
	vmx_env_init();
}

void
vm_thd_init(struct thread *thd, void *vm_pgd)
{
	vmx_thd_init(thd, vm_pgd);
}

int
vm_thd_page_set(struct thread *thd, u32_t page_type, void *page)
{
	return vmx_thd_page_set(thd, page_type, page);
}

void
vm_thd_exec(struct thread *thd)
{
	vmx_thd_start_or_resume(thd);
}

#else

void vm_env_init(void) {}
void vm_thd_init(struct thread *thd, void *vm_pgd) {}
int vm_thd_page_set(struct thread *thd, u32_t page_type, void *page) {return 0;}
void vm_thd_exec(struct thread *thd) {}

#endif