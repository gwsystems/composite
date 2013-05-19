#ifndef _VM_H_
#define _VM_H_

#include "types.h"

//#define PAGE_SIZE 4096
#define PAGE_SIZE 0x1000

struct page {
    uint32_t present  : 1;  /* Page is present in memory */
    uint32_t rw       : 1;  /* Read-only if clear, read-write if set */
    uint32_t user     : 1;  /* Supervisor level only if clear */
    uint32_t accessed : 1;  /* Has the page been accessed since last refresh */
    uint32_t dirty    : 1;  /* Has the page been written too? */
    uint32_t unused   : 7;  /* unused */
    uint32_t frame    : 20; /* fram adress shifted right 12 bits */
};

typedef struct page page_t;

struct page_table {
    page_t pages[1024];
};

struct page_directory {
    struct page_table *tables[1024];
    uint32_t tables_physical[1024];
    uint32_t physical_addr;
};

void paging__init(size_t memory_size);


#endif
