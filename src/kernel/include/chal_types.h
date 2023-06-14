#pragma once

#include <types.h>

typedef u32_t   prot_domain_tag_t; /* additional architecture protection domain context (e.g. PKRU, ASID) */
typedef uword_t cos_cap_t;
typedef u32_t   cos_op_bitmap_t;   /* bitmap where each bit designates an allowed operation on a resource */
