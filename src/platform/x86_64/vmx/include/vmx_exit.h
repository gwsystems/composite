#pragma once

struct vm_vcpu_shared_region;
struct thread;

void vmx_exit_handler_asm(void);
void vmx_resume(struct thread *thd);
void vmx_exit_handler(struct vm_vcpu_shared_region *regs);
