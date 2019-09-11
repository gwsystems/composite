/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 * 2010 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * 2012 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * - refactor to abstract loading over sections.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef CL_MACROS_H
#define CL_MACROS_H

#define printl(lvl, format, args...)            \
	{                                       \
		if (lvl <= print_lvl) {         \
			printf(format, ##args); \
			fflush(stdout);         \
		}                               \
	}

#define NUM_ATOMIC_SYMBS 10
#define NUM_KERN_SYMBS 1

#define CAP_CLIENT_STUB_DEFAULT       "SS_ipc_client_marshal_args"
#define CAP_CLIENT_STUB_RETS          "SS_ipc_client_marshal_args_rets"
#define CAP_CLIENT_STUB_POSTPEND      "_call"
#define CAP_SERVER_STUB_POSTPEND_RETS "_rets_inv"
#define CAP_SERVER_STUB_POSTPEND      "_inv"

#define BASE_SERVICE_ADDRESS SERVICE_START
#define DEFAULT_SERVICE_SIZE SERVICE_SIZE

#define LINKER_BIN "/usr/bin/ld"
#define STRIP_BIN "/usr/bin/strip"
#define GCC_BIN "gcc"

#define UNDEF_SYMB_TYPE 0x1
#define EXPORTED_SYMB_TYPE 0x2
#define MAX_SYMBOLS (1024 * 512)
#define MAX_TRUSTED 64
#define MAX_SYMB_LEN 512
#define MAX_FILE_NAME_LEN 1024

#define bfd_sect_size(bfd, x) (bfd_get_section_size(x) / bfd_octets_per_byte(bfd))

//#define ROUND_UP_TO_PAGE(a) (((vaddr_t)(a)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
//#deinfe ROUND_UP_TO_CACHELINE(a) (((vaddr_t)(a)+CACHE_LINE-1) & ~(CACHE_LINE-1))

//#define INIT_STR_SZ 116
#define INIT_STR_SZ 52

#define ADDR2VADDR(a) ((a - new_sect_start) + new_vaddr_start)

#define MAX_SCHEDULERS 3

#define STUB_PROG_LEN 128

#endif
