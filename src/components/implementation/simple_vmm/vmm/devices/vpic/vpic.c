#include <cos_types.h>
#include <cos_debug.h>
#include <vmrt.h>
#include "vpic.h"

/* TODO: implement necessary functions needed by VM */
struct chip_8259 {
	u8_t master_cmd;
	u8_t master_data;
	u8_t is_master_init;
	u8_t master_offset;

	u8_t slave_cmd;
	u8_t slave_data;
	u8_t is_slave_init;
	u8_t slave_offset;

	u8_t interrupt_pending;
};

static struct chip_8259 chip;

void
vpic_handler(u16_t port, int dir, int sz, struct vmrt_vm_vcpu *vcpu)
{
	assert(sz == IO_BYTE);
	if (dir == IO_IN) {
		switch (port)
		{
		case PIC_MASTER_DATA_PORT:
			vcpu->shared_region->ax = chip.master_data;
			break;
		case PIC_SLAVE_DATA_PORT:
			vcpu->shared_region->ax = chip.slave_data;
			break;
		case PIC_MASTER_CMD_PORT:
			vcpu->shared_region->ax = chip.master_cmd;
			break;
		default:
			printc("vpic_handler: port: 0x%x, dir: %d\n", port, dir);
			VM_PANIC(vcpu);
			break;
		}
	} else {
		u8_t val = vcpu->shared_region->ax;
		switch (port)
		{
		case PIC_MASTER_CMD_PORT:
			chip.master_cmd = val;
			if (chip.master_cmd & ICW1_INIT) {
				chip.is_master_init = 1;
			}
			if (chip.master_cmd == PIC_EOI) {
				chip.interrupt_pending = 0;
			}
			break;
		case PIC_MASTER_DATA_PORT:
			chip.master_data = val;
			if (chip.is_master_init) {
				chip.master_offset = chip.master_data;
				chip.is_master_init = 0;
			}
			break;
		case PIC_SLAVE_CMD_PORT:
			chip.slave_cmd = val;
			if (chip.slave_cmd & ICW1_INIT) {
				chip.is_slave_init = 1;
			}
			if (chip.slave_cmd == PIC_EOI) {
				chip.interrupt_pending = 0;
			}
			break;
		case PIC_SLAVE_DATA_PORT:
			chip.slave_data = val;
			if (chip.is_slave_init) {
				chip.slave_offset = chip.slave_data;
				chip.is_slave_init = 0;
			}
			break;
		default:
			assert(0);
			break;
		}

	}
}

static void __attribute__((constructor))
init(void)
{
	memset(&chip, 0, sizeof(chip));
}
