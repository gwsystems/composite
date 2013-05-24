#include "types.h"
#include "gdt.h"

struct gdt_entry {
    uint16_t limit_low;     // Lower 16 bits of the limit
    uint16_t base_low;      // Lower 16 bits of the base
    uint8_t base_middle;    // Next 8 bits of the base
    uint8_t access;         // Access flags to determine ring
    uint8_t granularity;    
    uint8_t base_high;      // Last 8 bits of the base
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;     // Upper 16 bits of all selecor limits
    uint32_t base;      // Address of the first gdt entry
} __attribute__((packed));


extern void gdt_flush(uint32_t);

#define NUM_GDT_ENTRIES 5

static struct gdt_entry gdt_entries[NUM_GDT_ENTRIES];
struct gdt_ptr gdt_ptr;

static void 
gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran) {

    /* setup the base address */
    gdt_entries[num].base_low    = (base & 0xFFFF);
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;

    /* Set up the limits */
    gdt_entries[num].limit_low   = (limit & 0xFFFF);
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;

    /* Set granularity */
    gdt_entries[num].granularity |= (gran & 0xF0);
    
    /* Assign access flags */
    gdt_entries[num].access = access;
}

void
gdt__init(void)
{
    gdt_ptr.limit = (sizeof(struct gdt_entry) * NUM_GDT_ENTRIES) - 1;
    gdt_ptr.base = (uintptr_t)&gdt_entries;
    
    /* NULL */
    gdt_set_gate(0, 0, 0, 0, 0);

    /* Code Segemnt */
    gdt_set_gate(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

    /* Data Segemnt */
    gdt_set_gate(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

    /* User mode code */
    gdt_set_gate(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

    /* User mode data */
    gdt_set_gate(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

    gdt_flush((uintptr_t)&gdt_ptr);
}

