#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include "shared/cos_types.h"

#define MULTIBOOT_MAGIC 0x1BADB002
#define MULTIBOOT_EAX_MAGIC 0x2BADB002
#define MULTIBOOT_FLAG_MEM 0x001
#define MULTIBOOT_FLAG_DEVICE 0x002
#define MULTIBOOT_FLAG_CMDLINE 0x004
#define MULTIBOOT_FLAG_MODS 0x008
#define MULTIBOOT_FLAG_AOUT 0x010
#define MULTIBOOT_FLAG_ELF 0x020
#define MULTIBOOT_FLAG_MMAP 0x040
#define MULTIBOOT_FLAG_CONFIG 0x080
#define MULTIBOOT_FLAG_LOADER 0x100
#define MULTIBOOT_FLAG_APM 0x200
#define MULTIBOOT_FLAG_VBE 0x400

#define MULTIBOOT_FLAGS_REQUIRED (MULTIBOOT_FLAG_MMAP | MULTIBOOT_FLAG_MODS)

struct multiboot {
	u32_t flags;
	u32_t mem_lower;
	u32_t mem_upper;
	u32_t boot_device;
	u32_t cmdline;
	u32_t mods_count;
	u32_t mods_addr;
	u32_t num;
	u32_t size;
	u32_t addr;
	u32_t shndx;
	u32_t mmap_length;
	u32_t mmap_addr;
	u32_t drives_length;
	u32_t drives_addr;
	u32_t config_table;
	u32_t boot_loader_name;
	u32_t apm_table;
	u32_t vbe_control_info;
	u32_t vbe_mode_info;
	u32_t vbe_mode;
	u32_t vbe_interface_seg;
	u32_t vbe_interface_off;
	u32_t vbe_interface_len;
} __attribute__((packed));

struct multiboot_mod_list {
	u32_t mod_start;
	u32_t mod_end;
	u32_t cmdline;
	u32_t pad;
};

struct multiboot_mem_list {
	u32_t size;
	u64_t addr;
	u64_t len;
	u32_t type;
} __attribute__((packed));

#endif /* MULTIBOOT_H */
