#include "types.h"
#include "vm.h"
#include "printk.h"
#include "mm.h"

extern uintptr_t end;
uintptr_t placement_address = (uintptr_t)&end;

static void *
kmalloc_int(size_t size, int align, uintptr_t *phys)
{
    uintptr_t tmp;
     
    if (align && (placement_address & 0xFFFFF000)) {
        placement_address &= 0xFFFFF000;
        placement_address += PAGE_SIZE;
    }
    
    if (phys)
        *phys = placement_address;

    tmp = placement_address;
    placement_address += size;
    
    return (void *)tmp;
}

void *
kmalloc_a(size_t size)
{
    return kmalloc_int(size, 1, NULL);
}

void *
kmalloc_p(size_t size, uintptr_t *phys)
{
    return kmalloc_int(size, 0, phys);
}

void *
kmalloc_ap(size_t size, uintptr_t *phys)
{
    return kmalloc_int(size, 1, phys);
}

void *
kmalloc(size_t size)
{
    return kmalloc_int(size, 0, NULL);
}
