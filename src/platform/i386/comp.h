#include <shared/cos_types.h>

struct spd_info {
        int spd_handle, num_caps;
        vaddr_t ucap_tbl;
        unsigned long lowest_addr;
        unsigned long size;
        unsigned long mem_size;
        vaddr_t upcall_entry;
        vaddr_t atomic_regions[10];
};

extern int kern_boot_comp(unsigned long booter_sz);
