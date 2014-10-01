#ifndef _VM_H_
#define _VM_H_

#define USER_STACK 0x3000000

#include "string.h"
#include "assert.h"
#include <shared/cos_types.h>
#include <shared/consts.h>
#include <pgtbl.h>

typedef u32_t pte_t;		// Page Table Entry
typedef pte_t pt_t[1024];	// Page Table
typedef u32_t ptd_t[1024];	// Page Table Directory

void paging__init(u32_t memory_size, u32_t nmods, u32_t *mods);

void ptd_init(ptd_t pt);

#endif
