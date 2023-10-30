## VMRT runtime library

A simple VM lib for Composite VMM managing virtual machines. Currently, it includes interfaces of VM creation, VCPU execution and simple exception handler mechanisms.


## VM abstraction data structure

**TODO: the current vcpu and vm_comp structures don't include the new capabilities for the kernel resources, such as lapic page capability, VMM & kernel shared region capability, vm exit exception handler capability. There should be a api in the future to combine these capabilities and call to the kernel to initialize vm thread and vm component.**

```c
struct vmrt_vm_vcpu {
	struct vm_vcpu_shared_region *shared_region;
	u64_t next_timer;
	void *lapic_page;

	/*
	 * A opaque pointer points to VMM's specific implementation of lapic.
	 * A VMM is responsible to have its own apic implementation.
	 */
	void *vlapic;

	thdid_t tid;
	thdid_t handler_tid;
	thdcap_t cap;

	u8_t cpuid;
	u8_t coreid;
	struct vmrt_vm_comp *vm;
};

struct vmrt_vm_comp {
	compid_t comp_id;
	void *guest_addr;
	word_t guest_mem_sz;

	u64_t lapic_access_page;

	char name[VMRT_VM_NAME_SIZE];

	u8_t num_vpu;
	struct vmrt_vm_vcpu vcpus[VMRT_VM_MAX_VCPU];
};
```

## Interfaces

### VM creation

```c
void
vmrt_vm_create(struct vmrt_vm_comp *vm, char *name, u8_t num_vcpu, u64_t vm_mem_sz)
{
	/* 1. VM component creation */
	vm_id = capmgr_vm_comp_create(vm_mem_sz);

	/* 2. Initialize vm members */
	assert(num_vcpu < VMRT_VM_MAX_VCPU);
	vm->num_vpu = num_vcpu;
	vm->guest_mem_sz = vm_mem_sz;  

	strcpy((char *)&vm->name, name);
}

```

### VCPU initialization

```c
void
vmrt_vm_vcpu_init(struct vmrt_vm_comp *vm, u32_t vcpu_nr)
{
	/* 1. VCPU exception handler thread creation */
	handler_tid = sched_thd_create(vmrt_vm_exception_handler, vcpu);

	/* 2. VCPU thread creation */
	capmgr_thd_create_ext(vm->comp_id, vcpu_nr + 1, &tid);

	/* 3. Bind the exception handler to the VCPU thread */
	capmgr_vm_thd_exception_handler_set(tid, handler_tid);

	/* TODO: create new capabilities for shared resources such as lapic page and combine them into a single api */
	vcpu->next_timer = ~0ULL;

	vcpu->vm = vm;
}
```

### Loading data into/from VM
```c
void
vmrt_vm_data_copy_to(struct vmrt_vm_comp *vm, char *data, u64_t size, paddr_t gpa)
{
	char *host_va = GPA2HVA(gpa, vm);
	memcpy(host_va, data, size);
}

void
vmrt_vm_data_copy_from(struct vmrt_vm_comp *vm, char *data, u64_t size, paddr_t gpa)
{
	char *host_va = GPA2HVA(gpa, vm);
	memcpy(data, host_va, size);
}
```

### Exception handler

```c
void
vmrt_vm_exception_handler(struct vmrt_vm_vcpu *vcpu)
{
	shared_region = vcpu->shared_region;
	while (1)
	{
		vmrt_vm_vcpu_resume(vcpu);

		/* 1. Get exit reason */
		u64_t reason = shared_region->reason;

		/* 2. handle the reason */
		vmrt_handle_reason(vcpu, reason);

		/* 3. inject necessary interrupt such as timer */
		rdtscll(curr_tsc);
		if (curr_tsc >= vcpu->next_timer) {
			lapic_intr_inject(vcpu, vector, 0);
		}
	}
}
```