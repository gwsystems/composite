#ifndef _VM_H_
#define _VM_H_

#include "types.h"
#include "shared/consts.h"

typedef uint32_t u32_t;

typedef u32_t pte_t;		// Page Table Entry
typedef pte_t pt_t[1024];	// Page Table
typedef u32_t ptd_t[1024];	// Page Table Directory

/* 4kB page structure:
0:	P	Present				0 = not present		1 = present
1:	RW	Read/Write			0 = read only		1 = read/write
2:	US	User/Super			0 = supervisor only	1 = user mode
3:	PWT	Page-level write-through	4.9
4:	PCD	Page-level cache disable	4.9
5:	A	Accessed			0 = not accessed	1 = accessed
6:	D	Dirty				0 = not written		1 = written
7:	PAT	4.9.2
8:	G	Global				0 = local		1 = global
9-11:	Ignored
12-31:	FRAME	Physical address of page
*/

#define PAGE_P		1
#define PAGE_RW		1 << 1
#define PAGE_US		1 << 2
#define PAGE_RSVD	1 << 3
#define PAGE_PWT	1 << 3
#define PAGE_ID		1 << 4
#define PAGE_PCD	1 << 4
#define PAGE_A		1 << 5
#define PAGE_D		1 << 6
#define PAGE_PAT	1 << 7
#define PAGE_G		1 << 8
#define PAGE_FRAME	0xfffff000

/* 4MB super page structure:
0:      P       Present                         0 = not present         1 = present
1:      RW      Read/Write                      0 = read only           1 = read/write
2:      US      User/Super                      0 = supervisotir only   1 = user mode
3:      PWT     Page-level write-through        4.9
4:      PCD     Page-level cache disable        4.9
5:      A       Accessed                        0 = not accessed        1 = accessed
6:      D       Dirty                           0 = not written         1 = written
7:      PS	Page size			0 = is page table entry	1 = is 4MB super page
8:      G       Global                          0 = local               1 = global
9-11:   Ignored
12:	PAT	4.9.2
13-31:	Confusing
*/

void paging__init(size_t memory_size, uint32_t nmods, uint32_t *mods);

void *chal_va2pa(void *address);
void *chal_pa2va(void *address);

void switch_user_mode(void);

void ptd_init(ptd_t pt);
void ptd_load(ptd_t pt);
void ptd_copy_global(ptd_t dst, ptd_t src);

#endif
