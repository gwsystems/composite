#pragma once

#include <cos_kernel_api.h>
#include <consts.h>

#define VMRT_VM_NAME_SIZE (32)
#define VMRT_VM_MAX_VCPU (16)

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
	u64_t pending_req;
};

struct vmrt_vm_comp {
	compid_t comp_id;
	void *guest_addr;
	word_t guest_mem_sz;

	u64_t lapic_access_page;

	char name[VMRT_VM_NAME_SIZE];

	u8_t num_vpu;
	struct vmrt_vm_vcpu vcpus[VMRT_VM_MAX_VCPU];

	int wire_mode;
};

#define VMRT_GPA2HVA(gpa, vm, offset) { ((gpa - offset) + vm->guest_addr) }
#define GPA2HVA(gpa, vm) VMRT_GPA2HVA(gpa, vm, PAGE_SIZE_4K)

typedef void vmrt_exception_handler(struct vmrt_vm_vcpu *vcpu);

void vmrt_vm_create(struct vmrt_vm_comp *vm, char *name, u8_t num_vcpu, u64_t vm_mem_sz);
void vmrt_vm_mem_init(struct vmrt_vm_comp *vm, void *mem);
void vmrt_vm_vcpu_init(struct vmrt_vm_comp *vm, u32_t vcpu_nr);
void vmrt_vm_vcpu_resume(struct vmrt_vm_vcpu *vcpu);
void vmrt_vm_data_copy_to(struct vmrt_vm_comp *vm, char *image, u64_t size, paddr_t gpa);
void vmrt_vm_exception_handler(void *vcpu);
void vmrt_vm_vcpu_start(struct vmrt_vm_vcpu *vcpu);
static inline struct vmrt_vm_vcpu *vmrt_get_vcpu(struct vmrt_vm_comp *vm, u32_t vcpu_nr) { return &vm->vcpus[vcpu_nr]; }

void lapic_intr_inject(struct vmrt_vm_vcpu *vcpu, u8_t vector, int autoeoi);

#define INCBIN(name, file) \
    __asm__( \
            ".global incbin_" STR(name) "_start\n" \
            ".balign 16\n" \
            "incbin_" STR(name) "_start:\n" \
            ".incbin \"" file "\"\n" \
            \
            ".global incbin_" STR(name) "_end\n" \
            ".balign 1\n" \
            "incbin_" STR(name) "_end:\n" \
            ".byte 0\n" \
    ); \
    extern const __attribute__((aligned(16))) void* incbin_ ## name ## _start; \
    extern const void* incbin_ ## name ## _end; \

#define GOTO_NEXT_INST(vcpu) { (vcpu)->ip += (vcpu)->inst_length; }

static inline void
chal_cpuid(u32_t *a, u32_t *b, u32_t *c, u32_t *d)
{
	asm volatile("cpuid" : "+a"(*a), "+b"(*b), "+c"(*c), "+d"(*d));
}

void vmrt_dump_vcpu(struct vmrt_vm_vcpu *vcpu);

#define VM_PANIC(vcpu) {	\
    vmrt_dump_vcpu(vcpu);	\
    assert(0);		\
}

#define IO_OUT 0
#define IO_IN 1

enum {
	IO_BYTE = 0,
	IO_WORD,
	IO_LONG,
};

#define LAPIC_BASE_ADDR (0xFEE00000)
