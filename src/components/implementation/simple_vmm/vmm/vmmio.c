#include <vmrt.h>
#include <instr_emul.h>
#include <acrn_common.h>
#include <vioapic.h>


/*
 * Report each unbacked MMIO page once rather than per access, so a probing
 * driver does not drown the log. See the comment on unhandled_port() in vio.c
 * for why absorbing these is correct rather than lenient.
 */
#define MMIO_REPORT_SLOTS 64
static u64_t   mmio_reported[MMIO_REPORT_SLOTS];
static unsigned mmio_reported_n;
static int     mmio_report_exhausted;

static int
mmio_first_report(u64_t gpa)
{
	u64_t page = gpa & ~(u64_t)(PAGE_SIZE_4K - 1);
	unsigned i;

	if (mmio_report_exhausted) return 0;

	for (i = 0; i < mmio_reported_n; i++) {
		if (mmio_reported[i] == page) return 0;
	}

	if (mmio_reported_n == MMIO_REPORT_SLOTS) {
		/*
		 * Out of slots. Report once more to say we are going quiet -- a
		 * guest probing a large unbacked region would otherwise log on
		 * every access, which is what this table exists to prevent.
		 */
		mmio_report_exhausted = 1;
		printc("vmm: more than %d unbacked mmio pages; further ones not reported\n",
		       MMIO_REPORT_SLOTS);
		return 0;
	}

	mmio_reported[mmio_reported_n++] = page;

	return 1;
}

void 
ept_violation_handler(struct vmrt_vm_vcpu *vcpu)
{
	volatile struct vm_vcpu_shared_region *regs = vcpu->shared_region;
	u64_t qualification = regs->qualification;
	u64_t gpa;
	struct acrn_mmio_request *mmio_req = vcpu->mmio_request;

	gpa = regs->gpa;

	/* Specify if read or write operation */
	if ((qualification & 0x2UL) != 0UL) {
		/* Write operation */
		mmio_req->direction = ACRN_IOREQ_DIR_WRITE;
		mmio_req->value = 0UL;
	} else {
		/* Read operation */
		mmio_req->direction = ACRN_IOREQ_DIR_READ;
	}

	mmio_req->address = gpa;

	if (gpa >= IOAPIC_BASE_ADDR && gpa < IOAPIC_BASE_ADDR + PAGE_SIZE_4K) {
		int ret = decode_instruction(vcpu);

		if (ret > 0) {
			mmio_req->size = (uint64_t)ret;
		} else {
			VM_PANIC(vcpu);
		}

		if (mmio_req->direction == ACRN_IOREQ_DIR_WRITE) {
			if (emulate_instruction(vcpu) != 0) {
				VM_PANIC(vcpu);
			}
		}

		vioapic_mmio_access_handler(vcpu);

		if (mmio_req->direction == ACRN_IOREQ_DIR_READ) {
			/* Emulate instruction and update vcpu register set */
			(void)emulate_instruction(vcpu);
		}
		GOTO_NEXT_INST(regs);
	} else {
		/*
		 * No device backs this physical address. Hardware returns zero
		 * for a read of an unclaimed region and discards the write;
		 * panicking makes any unemulated device fatal to the guest.
		 */
#ifdef VMM_STRICT_IO
		VM_PANIC(vcpu);
#else
		if (mmio_first_report(gpa)) {
			printc("vmm: unbacked mmio at %llx; reads return zero\n",
			       (unsigned long long)gpa);
		}
		if (decode_instruction(vcpu) > 0) {
			if (mmio_req->direction == ACRN_IOREQ_DIR_READ) {
				mmio_req->value = 0;
				(void)emulate_instruction(vcpu);
			}
			GOTO_NEXT_INST(regs);
		} else {
			/* Cannot even decode it -- absorbing that would hide a real bug. */
			VM_PANIC(vcpu);
		}
#endif
	}
}
