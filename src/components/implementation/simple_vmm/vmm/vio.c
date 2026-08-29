#include <vmrt.h>
#include "devices/serial/serial.h"
#include "devices/vpci/vpci.h"
#include "devices/vpic/vpic.h"
#include "devices/vrtc/vrtc.h"
#include "devices/vps2/vps2.h"
#include "devices/vpci/virtio_net_io.h"


/*
 * Unemulated devices.
 *
 * On real x86 a read from an I/O port no device answers floats the bus high --
 * all ones -- and a write goes nowhere. Drivers rely on this to probe for
 * absent hardware; arch/x86/kernel/i8237.c is a plain example:
 *
 *     if (dma_inb(DMA_PAGE_0) == 0xFF)
 *             return -ENODEV;
 *
 * This VMM used to VM_PANIC instead, so any probe of a device we do not
 * emulate killed the guest, and guest kernels had to be patched not to probe.
 * Behaving like hardware removes that whole class of problem.
 *
 * Build with -DVMM_STRICT_IO to get the panic back when you want an unexpected
 * access to be loud rather than absorbed.
 */
static u8_t port_reported[8192];  /* one bit per port, 65536 ports */

static int
port_first_report(u16_t port)
{
	u8_t mask = 1 << (port & 7);
	u16_t idx = port >> 3;

	if (port_reported[idx] & mask) return 0;
	port_reported[idx] |= mask;

	return 1;
}

static void
unhandled_port(u16_t port_id, int dir, int sz, struct vmrt_vm_vcpu *vcpu)
{
#ifdef VMM_STRICT_IO
	printc("port id:%x, sz:%d, dir:%d\n", port_id, sz, dir);
	VM_PANIC(vcpu);
#else
	if (port_first_report(port_id)) {
		printc("vmm: no device at port %x (%s); reads return all-ones\n",
		       port_id, dir == IO_IN ? "read" : "write");
	}

	if (dir == IO_IN) {
		u64_t ax = vcpu->shared_region->ax;

		/* Only the accessed width is written; the rest of RAX is preserved. */
		if (sz == IO_BYTE)      ax = (ax & ~0xFFULL)   | 0xFFULL;
		else if (sz == IO_WORD) ax = (ax & ~0xFFFFULL) | 0xFFFFULL;
		else                    ax = 0xFFFFFFFFULL;

		vcpu->shared_region->ax = ax;
	}
	/* Writes to nothing are discarded. */
#endif
}

void 
io_handler(struct vmrt_vm_vcpu *vcpu)
{
	volatile struct vm_vcpu_shared_region *regs = vcpu->shared_region;
	u64_t qualification = regs->qualification;

	u16_t port_id = (qualification >> 16) & 0XFFFF;
	u8_t access_sz = qualification & 7;
	u8_t access_dir = (qualification >> 3) & 1;

	assert(!((qualification >> 4) & 1));
	assert(!((qualification >> 5) & 1));

	switch (access_sz)
	{
	case 0:
		access_sz = IO_BYTE;
		break;
	case 1:
		access_sz = IO_WORD;
		break;
	case 3:
		access_sz = IO_LONG;
		break;
	default:
		VM_PANIC(vcpu);
		
	}
	if (port_id <= SERIAL_PORT_MAX && port_id >= SERIAL_PORT_MIN) {
		serial_handler(port_id, access_dir, access_sz, vcpu);
		goto done;
	}

	/* Fast path for virtio-net processing */
	switch (port_id)
	{
	case VIRTIO_NET_DEV_FEATURES:
	case VIRTIO_NET_GUEST_FEATURES:
	case VIRTIO_NET_QUEUE_ADDR:
	case VIRTIO_NET_QUEUE_SIZE:
	case VIRTIO_NET_QUEUE_SELECT:
	case VIRTIO_NET_QUEUE_NOTIFY:
	case VIRTIO_NET_DEV_STATUS:
	case VIRTIO_NET_ISR:
	case VIRTIO_NET_MAC:
	case VIRTIO_NET_MAC1:
	case VIRTIO_NET_MAC2:
	case VIRTIO_NET_MAC3:
	case VIRTIO_NET_MAC4:
	case VIRTIO_NET_MAC5:
	case VIRTIO_NET_STATUS:
	case VIRTIO_NET_STATUS_H:	
		virtio_net_handler(port_id, access_dir, access_sz, vcpu);
		goto done;	
	default:
		break;
	}

	switch (port_id)
	{
	case CMOS_CMD_PORT:
	case CMOS_DATA_PORT:
		vrtc_handler(port_id, access_dir, access_sz, vcpu);
		goto done;
	case PIC_MASTER_CMD_PORT:
	case PIC_MASTER_DATA_PORT:
	case PIC_SLAVE_CMD_PORT:
	case PIC_SLAVE_DATA_PORT:
	case PIC_ELCR1:
	case PIC_ELCR2:
		vpic_handler(port_id, access_dir, access_sz, vcpu);
		goto done;
	case PS2_CONTROL_PORT_A:
	case PS2_CONTROL_PORT_B:
		ps2_handler(port_id, access_dir, access_sz, vcpu);
		goto done;
	case VPCI_CONFIG_ADDRESS:
	case VPCI_CONFIG_DATA0:
	case VPCI_CONFIG_DATA1:
	case VPCI_CONFIG_DATA2:
	case VPCI_CONFIG_DATA3:
	case VPCI_MECHANISM_CONTROL_REG:
		vpci_handler(port_id, access_dir, access_sz, vcpu);
		goto done;
	}

	unhandled_port(port_id, access_dir, access_sz, vcpu);
done:
	GOTO_NEXT_INST(regs);
	
	return;
}