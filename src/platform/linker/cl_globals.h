/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 * 2010 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * 2012 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * - refactor to abstract loading over sections.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Linker and loader for the Composite system: takes a collection of
 * services with their trust relationships explicitly expressed,
 * dynamically generates their stub code to connect them, communicates
 * capability information info with the runtime system, creates the
 * user-level static capability structures, and loads the services
 * into the current address space which will be used as a template for
 * the run-time system for creating each service protection domain
 * (ie. copying the entries in the pgd to new address spaces.
 *
 * This is trusted code, and any mistakes here compromise the entire
 * system.  Essentially, control flow is restricted/created here.
 *
 * Going by the man pages, I think I might be going to hell for using
 * strtok so much.  Suffice to say, don't multithread this program.
 */

#include "cl_types.h"
#include "cl_macros.h"

extern enum { PRINT_NONE = 0, PRINT_HIGH, PRINT_NORMAL, PRINT_DEBUG } print_lvl;

extern const char *COMP_INFO;
extern const char *COMP_PLT;
extern const char *SCHED_NOTIF;
extern const char *INIT_COMP;
extern char *      ROOT_SCHED;
extern const char *INITMM;
extern const char *MPD_MGR;
extern const char *CONFIG_COMP;
extern const char *BOOT_COMP;
extern const char *LLBOOT_COMP;
extern const char *INIT_FILE;
extern const char *INIT_FILE_NAME;
extern const char *ATOMIC_USER_DEF[NUM_ATOMIC_SYMBS];
extern const char *SCHED_CREATE_FN;
extern const char *cos_flt_handlers[COS_FLT_MAX];

extern struct cos_sections section_info[MAXSEC_S + 1];

extern int     spdid_inc;
extern u32_t   llboot_mem;
volatile int   var;
extern vaddr_t SS_ipc_client_marshal;
// extern vaddr_t DS_ipc_client_marshal;
