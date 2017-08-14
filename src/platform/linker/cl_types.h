/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 * 2010 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * 2012 The George Washington University, Gabriel Parmer, gparmer@gwu.edu
 * - refactor to abstract loading over sections.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#ifndef CL_TYPES_H
#define CL_TYPES_H

#include "cl_macros.h"
#include <bfd.h>
#include <cobj_format.h>

typedef enum { LLBOOT_COMPN, LLBOOT_SCHED, LLBOOT_MM, LLBOOT_PRINT, LLBOOT_BOOT } llboot_component_ids;

struct sec_info {
	asection *s;
	int       offset;
};

typedef enum {
	TEXT_S,
	RODATA_S,
	CTORS_S,
	DTORS_S,
	INIT_ARRAY_S,
	FINI_ARRAY_S,
	CRECOV_S,
	KMEM_S,
	CINFO_S,
	DATA_S,
	BSS_S,
	INITONCE_S,
	INITFILE_S,
	MAXSEC_S
} sec_type_t;

/*
 * TODO: add structure containing all information about sections, so
 * that they can be created algorithmically, in a loop, instead of
 * this hard-coding crap.
 */
struct cos_sections {
	sec_type_t      secid;
	int             cobj_flags, coalesce; /* should this section be output with the previous? */
	char *          sname, *ld_output;
	struct sec_info srcobj, ldobj;
	unsigned long   start_addr, len;
};

typedef int (*observer_t)(asymbol *, void *data);

struct service_symbs;
struct dependency;

struct symb {
	char *                name;
	int                   modifier_offset;
	vaddr_t               addr;
	struct service_symbs *exporter;
	struct symb *         exported_symb;
};

struct symb_type {
	int         num_symbs;
	struct symb symbs[MAX_SYMBOLS];

	struct service_symbs *parent;
};

struct dependency {
	struct service_symbs *dep;
	char *                modifier;
	int                   mod_len;
	/* has a capability been created for this dependency */
	int resolved;
};

typedef enum { SERV_SECT_RO, SERV_SECT_DATA, SERV_SECT_BSS, SERV_SECT_INITONCE, SERV_SECT_NUM } serv_sect_type;

struct service_section {
	unsigned long offset;
	int           size;
};

struct service_symbs {
	char *        obj, *init_str;
	unsigned long lower_addr, size, allocated, heap_top;
	unsigned long mem_size; /* memory used */

	struct service_section sections[SERV_SECT_NUM];

	int                 is_composite_loaded, already_loaded;
	struct cobj_header *cobj;

	int                   is_scheduler;
	struct service_symbs *scheduler;

	struct spd *          spd;
	struct symb_type      exported, undef;
	int                   num_dependencies;
	struct dependency     dependencies[MAX_TRUSTED];
	struct service_symbs *next;
	int                   depth;

	void *extern_info;
};

typedef enum { TRANS_CAP_NIL = 0, TRANS_CAP_FAULT, TRANS_CAP_SCHED } trans_cap_t;

struct cap_ret_info {
	struct symb *         csymb, *ssymbfn, *cstub, *sstub;
	struct service_symbs *serv;
	u32_t                 fault_handler;
};

/* Edge description of components.  Mirrored in mpd_mgr.c */
struct comp_graph {
	short int client, server;
};

/* struct is 64 bytes, so we can have 64 entries in a page. */
struct component_init_str {
	unsigned int spdid, schedid;
	int          startup;
	char         init_str[INIT_STR_SZ];
} __attribute__((packed));

struct component_traits {
	int sched, composite_loaded;
};

struct spd_info {
	int           spd_handle, num_caps;
	vaddr_t       ucap_tbl;
	unsigned long lowest_addr;
	unsigned long size;
	unsigned long mem_size;
	vaddr_t       upcall_entry;
	vaddr_t       atomic_regions[10];
};

struct cap_info {
	int               cap_handle, rel_offset;
	int               owner_spd_handle, dest_spd_handle;
	isolation_level_t il;
	int               flags;
	vaddr_t           ST_serv_entry;
	vaddr_t           SD_cli_stub, SD_serv_stub;
	vaddr_t           AT_cli_stub, AT_serv_stub;
};


#endif
