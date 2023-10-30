#pragma once

/* Abstraction layer for vm support*/
#include <vmx_vmcs.h>

typedef enum {
	VM_THD_STATE_STOPPED    = 1,
	VM_THD_STATE_RUNNING    = 1 << 1, 
} vm_thd_state_t;

struct vm_vcpu_context {
	vm_thd_state_t state;
	struct vmx_vmcs vmcs;
};

struct thread;

void vm_env_init(void);
void vm_thd_init(struct thread *thd, void *vm_pgd);
void vm_thd_exec(struct thread *thd);
int  vm_thd_page_set(struct thread *thd, u32_t page_type, void *page);
