/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 * 2010 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * 2012 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * - refactor to abstract loading over sections.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include "cl_types.h"
#include "cl_macros.h"

enum
{
	PRINT_NONE = 0,
	PRINT_HIGH,
	PRINT_NORMAL,
	PRINT_DEBUG
} print_lvl = PRINT_HIGH;

const char *COMP_INFO      = "cos_comp_info";
const char *COMP_PLT       = "ST_user_caps";
const char *SCHED_NOTIF    = "cos_sched_notifications";
const char *INIT_COMP      = "c0.o";
char *      ROOT_SCHED     = NULL;   // this is set to the first listed scheduler (*)
const char *INITMM         = "mm.o"; // this is set to the first listed memory manager (#)
const char *MPD_MGR        = "cg.o"; // the component graph!
const char *CONFIG_COMP    = "schedconf.o";
const char *BOOT_COMP      = "boot.o";
const char *LLBOOT_COMP    = "llboot.o";
const char *INIT_FILE      = "initfs.o";
const char *INIT_FILE_NAME = "init.tar";

const char *ATOMIC_USER_DEF[NUM_ATOMIC_SYMBS] = {"cos_atomic_cmpxchg", "cos_atomic_cmpxchg_end",
                                                 "cos_atomic_user1",   "cos_atomic_user1_end",
                                                 "cos_atomic_user2",   "cos_atomic_user2_end",
                                                 "cos_atomic_user3",   "cos_atomic_user3_end",
                                                 "cos_atomic_user4",   "cos_atomic_user4_end"};

const char *SCHED_CREATE_FN = "sched_init";

/*
 * See cos_types.h for the numerical identifiers of each of these
 * fault handlers.
 */
const char *cos_flt_handlers[COS_FLT_MAX] = {"fault_page_fault_handler", "fault_div_zero_handler",
                                             "fault_brkpt_handler",      "fault_overflow_handler",
                                             "fault_range_handler",      "fault_gen_prot_handler",
                                             "fault_linux_handler",      "fault_save_regs_handler",
                                             "fault_flt_notif_handler", "fault_flt_quarantine"};

struct cos_sections section_info[MAXSEC_S + 1] =
  {{.secid = TEXT_S, .cobj_flags = COBJ_SECT_READ | COBJ_SECT_INITONCE, .sname = ".text"},
   {.secid = RODATA_S, .cobj_flags = COBJ_SECT_READ | COBJ_SECT_INITONCE, .coalesce = 1, .sname = ".rodata"},
   {
     .secid      = CTORS_S,
     .cobj_flags = COBJ_SECT_READ | COBJ_SECT_INITONCE,
     .coalesce   = 1,
     .sname      = ".ctors",
   },
   {
     .secid      = DTORS_S,
     .cobj_flags = COBJ_SECT_READ | COBJ_SECT_INITONCE,
     .coalesce   = 1,
     .sname      = ".dtors",
   },
   {
     .secid      = INIT_ARRAY_S,
     .cobj_flags = COBJ_SECT_READ | COBJ_SECT_INITONCE,
     .coalesce   = 1,
     .sname      = ".init_array",
   },
   {
     .secid      = FINI_ARRAY_S,
     .cobj_flags = COBJ_SECT_READ | COBJ_SECT_INITONCE,
     .coalesce   = 1,
     .sname      = ".fini_array",
   },
   {
     .secid      = CRECOV_S,
     .cobj_flags = COBJ_SECT_READ | COBJ_SECT_INITONCE,
     .coalesce   = 1,
     .sname      = ".crecov",
   },
   {.secid = KMEM_S, .cobj_flags = COBJ_SECT_READ | COBJ_SECT_WRITE | COBJ_SECT_KMEM, .sname = ".kmem"},
   {.secid = CINFO_S, .cobj_flags = COBJ_SECT_READ | COBJ_SECT_WRITE | COBJ_SECT_CINFO, .sname = ".cinfo"},
   {.secid = DATA_S, .cobj_flags = COBJ_SECT_READ | COBJ_SECT_WRITE, .sname = ".data"},
   {.secid = BSS_S, .cobj_flags = COBJ_SECT_READ | COBJ_SECT_WRITE | COBJ_SECT_ZEROS, .sname = ".bss"},
   {.secid      = INITONCE_S,
    .cobj_flags = COBJ_SECT_READ | COBJ_SECT_WRITE | COBJ_SECT_ZEROS | COBJ_SECT_INITONCE,
    .sname      = ".initonce"},
   {.secid = INITFILE_S, .cobj_flags = COBJ_SECT_READ | COBJ_SECT_INITONCE, .sname = ".initfile"},
   {.secid = MAXSEC_S, .sname = NULL}};

int          spdid_inc = -1;
u32_t        llboot_mem;
volatile int var;
