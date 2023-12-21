#include <vm_vcpu_context.h>
#include <thd.h>
#include <vmx.h>
#include <vm.h>

/* Abstraction layer for VM support */
#ifdef CONFIG_VMX 

void
vm_env_init(void)
{
	vmx_env_init();
}

void
vm_thd_init(struct thread *thd, void *vm_pgd, struct cap_vm_vmcb *vmcb)
{
	vmx_thd_init(thd, vm_pgd, vmcb);
}

void
vm_thd_exec(struct thread *thd)
{
	vmx_thd_start_or_resume(thd);
}

#else

void vm_env_init(void) {}
void vm_thd_init(struct thread *thd, void *vm_pgd, struct cap_vm_vmcb *vmcb) {}
void vm_thd_exec(struct thread *thd) {}

#endif