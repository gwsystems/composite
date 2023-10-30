#pragma once

#include <shared/cos_config.h>
#include <chal_config.h>
#include <chal_cpu.h>

#define BIT(x)	((1U) << (x))

#define CR_GET(crx, val_ptr) { __asm__ volatile ("mov %%" #crx ", %0" : "=r"(*(val_ptr))); }
#define CR_SET(crx, val) { __asm__ volatile ("mov %0, %%" #crx : : "r"(val)); }

static inline u64_t
msr_get(u32_t msr)
{
	u32_t low, high;
	readmsr(msr, &low, &high);

	return (((u64_t)high << 32U) | low);
}

static inline void
msr_set(u32_t msr, u64_t val)
{
	u32_t low, high;
	writemsr(msr, val, (val >> 32));
}

static inline u64_t
get_gdt_base(void)
{
	struct gdt {
		u16_t limit;
		u64_t base;
	}__attribute__((packed));

	struct gdt gdt;
	__asm__ volatile ("sgdt (%%rax);" : :"a"(&gdt) :"memory");

	return gdt.base;
}

static inline u32_t
fix_reserved_ctrl_bits(u32_t msr, u32_t ctrl_bits)
{
	u64_t val;
	u32_t ret;

	val = msr_get(msr);

	ret = ctrl_bits & (val >> 32);
	ret = ret | (u32_t)val; 

	return ret;
}
