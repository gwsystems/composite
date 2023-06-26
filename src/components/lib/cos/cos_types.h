#pragma once

#include <cos_chal_types.h>

/***
 * This file includes all of the types that should be shared between
 * user and kernel level. It attempts to avoid architecture-specific
 * differences by assuming that `word_t`/`uword_t` are used where a
 * data-item should be sized to a word.
 *
 * It should be safe to include this file in any user-level or
 * kernel-level code (besides assembly files).
 */

/* Utility types */

typedef unsigned long  u64_t;
typedef unsigned int   u32_t;
typedef unsigned short u16_t;
typedef unsigned char  u8_t;
/* We're assuming here that `long` is 4 bytes (8 bytes) on 32 (64)-bit architectures */
typedef unsigned long  uword_t;	          /* Unsigned machine word */
typedef long           word_t;	          /* Signed machine word */

typedef u16_t          coreid_t;          /* A specific core. */
typedef uword_t        thdid_t;	          /* A thread's id. */

typedef uword_t        inv_token_t;       /* the token passed to a server when synchronously invoked */
typedef uword_t        id_token_t;        /* a token that holds an id, either for a thread, or an endpoint */
typedef uword_t        sync_token_t;      /* the token used to count, thus detect stale, dispatches */

typedef u64_t          cos_prio_t;        /* Priority value with lower values meaning higher priority */
#define COS_PRIO_LOW   (~0)

typedef u64_t          cos_cycles_t;      /* accounting cycle count */
typedef u64_t          cos_time_t;	  /* absolute time */
typedef u8_t           cos_thd_state_t;   /* thread state, reported to scheduler */

typedef uword_t        cos_cap_t;
typedef u32_t          cos_op_bitmap_t;   /* bitmap where each bit designates an allowed operation on a resource */

typedef uword_t        vaddr_t;           /* opaque, user-level virtual address */

#include <cos_compiler.h>

struct ulk_invstk_entry {
	cos_cap_t sinv_cap;
	vaddr_t sp;
} __attribute__((packed));

#define ULK_INVSTK_NUM_ENT 15
#define ULK_INVSTK_SZ 256ul

struct ulk_invstk {
	u64_t top, pad;
	struct ulk_invstk_entry stk[ULK_INVSTK_NUM_ENT];
};

#include <cos_consts.h>

#define ULK_STACKS_PER_PAGE (COS_PAGE_SIZE / sizeof(struct ulk_invstk))

COS_STATIC_ASSERT(ULK_INVSTK_SZ == sizeof(struct ulk_invstk),
	"User-level invocation stack");
