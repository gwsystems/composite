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

#include <cos_config.h>
#ifdef LINUX_HIGHEST_PRIORITY
#define HIGHEST_PRIO 1
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <signal.h>

#include <bfd.h>

//#include <stdbool.h>
//#include <pthread.h>
#include <sys/wait.h> /* wait for children process termination */
#include <sched.h>

/* composite includes */
//#include <spd.h>
//#include <ipc.h>

#include <cobj_format.h>

enum {PRINT_NONE = 0, PRINT_HIGH, PRINT_NORMAL, PRINT_DEBUG} print_lvl = PRINT_HIGH;

#define printl(lvl,format, args...)				\
	{							\
		if (lvl <= print_lvl) {				\
			printf(format, ## args);		\
			fflush(stdout);				\
		}						\
	}

#define NUM_ATOMIC_SYMBS 10
#define NUM_KERN_SYMBS   1

const char *COMP_INFO      = "cos_comp_info";
const char *SCHED_NOTIF    = "cos_sched_notifications";
const char *INIT_COMP      = "c0.o";
char       *ROOT_SCHED     = NULL;   // this is set to the first listed scheduler (*)
const char *INITMM         = "mm.o"; // this is set to the first listed memory manager (#)
const char *MPD_MGR        = "cg.o"; // the component graph!
const char *CONFIG_COMP    = "schedconf.o";
const char *BOOT_COMP      = "boot.o";
const char *LLBOOT_COMP    = "llboot.o";
const char *INIT_FILE      = "initfs.o";
const char *INIT_FILE_NAME = "init.tar";

typedef enum {
	LLBOOT_COMPN = 1,
	LLBOOT_SCHED = 2,
	LLBOOT_MM    = 3,
	LLBOOT_PRINT = 4,
	LLBOOT_BOOT  = 5
} llboot_component_ids;

const char *ATOMIC_USER_DEF[NUM_ATOMIC_SYMBS] =
{ "cos_atomic_cmpxchg",
  "cos_atomic_cmpxchg_end",
  "cos_atomic_user1",
  "cos_atomic_user1_end",
  "cos_atomic_user2",
  "cos_atomic_user2_end",
  "cos_atomic_user3",
  "cos_atomic_user3_end",
  "cos_atomic_user4",
  "cos_atomic_user4_end" };

#define CAP_CLIENT_STUB_DEFAULT "SS_ipc_client_marshal_args"
#define CAP_CLIENT_STUB_POSTPEND "_call"
#define CAP_SERVER_STUB_POSTPEND "_inv"

const char *SCHED_CREATE_FN = "sched_init";

/*
 * See cos_types.h for the numerical identifiers of each of these
 * fault handlers.
 */
static const char *
cos_flt_handlers[COS_FLT_MAX] = {
	"fault_page_fault_handler",
	"fault_div_zero_handler",
	"fault_brkpt_handler",
	"fault_overflow_handler",
	"fault_range_handler",
	"fault_gen_prot_handler",
	"fault_linux_handler",
	"fault_save_regs_handler",
	"fault_flt_notif_handler",
	"fault_quarantine_handler"
};

static inline int
fault_handler_num(char *fn_name)
{
	int i;

	for (i = 0 ; i < COS_FLT_MAX ; i++) {
		if (!strcmp(cos_flt_handlers[i], fn_name)) return i;
	}
	return -1;
}

#define BASE_SERVICE_ADDRESS SERVICE_START
#define DEFAULT_SERVICE_SIZE SERVICE_SIZE

#define LINKER_BIN "/usr/bin/ld"
#define STRIP_BIN "/usr/bin/strip"
#define GCC_BIN "gcc"

struct sec_info {
	asection *s;
	int offset;
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
	sec_type_t secid;
	int cobj_flags, coalesce; 		/* should this section be output with the previous? */
	char *sname, *ld_output;
	struct sec_info srcobj, ldobj;
	unsigned long start_addr, len;
};

struct cos_sections section_info[MAXSEC_S+1] = {
	{
		.secid      = TEXT_S,
		.cobj_flags = COBJ_SECT_READ | COBJ_SECT_INITONCE,
		.sname      = ".text"
	},
	{
		.secid      = RODATA_S,
		.cobj_flags = COBJ_SECT_READ | COBJ_SECT_INITONCE,
		.coalesce   = 1,
		.sname      = ".rodata"
	},
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
	{
		.secid      = KMEM_S,
		.cobj_flags = COBJ_SECT_READ | COBJ_SECT_WRITE | COBJ_SECT_KMEM,
		.sname      = ".kmem"
	},
	{
		.secid      = CINFO_S,
		.cobj_flags = COBJ_SECT_READ | COBJ_SECT_WRITE | COBJ_SECT_CINFO,
		.sname      = ".cinfo"
	},
	{
		.secid      = DATA_S,
		.cobj_flags = COBJ_SECT_READ | COBJ_SECT_WRITE,
		.sname      = ".data"
	},
	{
		.secid      = BSS_S,
		.cobj_flags = COBJ_SECT_READ | COBJ_SECT_WRITE | COBJ_SECT_ZEROS,
		.sname      = ".bss"
	},
	{
		.secid      = INITONCE_S,
		.cobj_flags = COBJ_SECT_READ | COBJ_SECT_WRITE | COBJ_SECT_ZEROS | COBJ_SECT_INITONCE,
		.sname      = ".initonce"
	},
	{
		.secid      = INITFILE_S,
		.cobj_flags = COBJ_SECT_READ | COBJ_SECT_INITONCE,
		.sname      = ".initfile"
	},
	{
		.secid      = MAXSEC_S,
		.sname      = NULL
	}
};
static struct cos_sections *
cos_sect_get(sec_type_t id)
{
	int i;
	static int first = 1;

	assert(id <= MAXSEC_S);
	if (first) {
		for (i = 0 ; section_info[i].secid != MAXSEC_S ; i++) {
			assert(section_info[i].secid == (unsigned int)i);
		}
	}
	return &section_info[id];
}
static struct cos_sections *csg(sec_type_t id) { return cos_sect_get(id); }

char script[64];
char tmp_exec[128];

int spdid_inc = -1;

#define UNDEF_SYMB_TYPE 0x1
#define EXPORTED_SYMB_TYPE 0x2
#define MAX_SYMBOLS 1024
#define MAX_TRUSTED 32
#define MAX_SYMB_LEN 256

typedef int (*observer_t)(asymbol *, void *data);

struct service_symbs;
struct dependency;

struct symb {
	char *name;
	int modifier_offset;
	vaddr_t addr;
	struct service_symbs *exporter;
	struct symb *exported_symb;
};

struct symb_type {
	int num_symbs;
	struct symb symbs[MAX_SYMBOLS];

	struct service_symbs *parent;
};

struct dependency {
	struct service_symbs *dep;
	char *modifier;
	int mod_len;
	/* has a capability been created for this dependency */
	int resolved;
};

typedef enum {
	SERV_SECT_RO,
	SERV_SECT_DATA,
	SERV_SECT_BSS,
	SERV_SECT_INITONCE,
	SERV_SECT_NUM
} serv_sect_type;

struct service_section {
	unsigned long offset;
	int size;
};

struct service_symbs {
	char *obj, *init_str;
	unsigned long lower_addr, size, allocated, heap_top;
	unsigned long mem_size; /* memory used */

	struct service_section sections[SERV_SECT_NUM];

	int is_composite_loaded, already_loaded;
	struct cobj_header *cobj;

	int is_scheduler;
	struct service_symbs *scheduler;

	struct spd *spd;
	struct symb_type exported, undef;
	int num_dependencies;
	struct dependency dependencies[MAX_TRUSTED];
	struct service_symbs *next;
	int depth;

	void *extern_info;
};

typedef enum {
	TRANS_CAP_NIL = 0,
	TRANS_CAP_FAULT,
	TRANS_CAP_SCHED
} trans_cap_t;

static int service_get_spdid(struct service_symbs *ss);
static int is_loaded_by_llboot(struct service_symbs *s)
{
	return (!(strstr(s->obj, INIT_COMP) || strstr(s->obj, LLBOOT_COMP)));
//	return s->is_composite_loaded;
}

static int is_loaded_by_boot(struct service_symbs *s)
{
	return s->is_composite_loaded;
}

static int is_loaded_by_boot_layer(struct service_symbs *s, int layer)
{
	if (!layer) return is_loaded_by_llboot(s);
	return (s->is_composite_loaded == layer);
}

static inline trans_cap_t
is_transparent_capability(struct symb *s, int *fltn) {
	char *n = s->name;
	*fltn = 0;

	if (s->modifier_offset) {
		printf("%s -> %s.\n", n, n+s->modifier_offset);
	}
	if (!strcmp(n, SCHED_CREATE_FN)) return TRANS_CAP_SCHED;
	if (-1 != (*fltn = fault_handler_num(n + s->modifier_offset)))  return TRANS_CAP_FAULT;
	return TRANS_CAP_NIL;
}

static unsigned long getsym(bfd *obj, char* symbol)
{
	long storage_needed;
	asymbol **symbol_table;
	long number_of_symbols;
	int i;

	storage_needed = bfd_get_symtab_upper_bound (obj);

	if (storage_needed <= 0){
		printl(PRINT_DEBUG, "no symbols in object file\n");
		exit(-1);
	}

	symbol_table = (asymbol **) malloc (storage_needed);
	number_of_symbols = bfd_canonicalize_symtab(obj, symbol_table);

	//notes: symbol_table[i]->flags & (BSF_FUNCTION | BSF_GLOBAL)
	for (i = 0; i < number_of_symbols; i++) {
		if(!strcmp(symbol, symbol_table[i]->name)){
			return symbol_table[i]->section->vma + symbol_table[i]->value;
		}
	}

	printl(PRINT_DEBUG, "Unable to find symbol named %s\n", symbol);
	return 0;
}

#ifdef DEBUG
static void print_syms(bfd *obj)
{
	long storage_needed;
	asymbol **symbol_table;
	long number_of_symbols;
	int i;

	storage_needed = bfd_get_symtab_upper_bound (obj);

	if (storage_needed <= 0){
		printl(PRINT_DEBUG, "no symbols in object file\n");
		exit(-1);
	}

	symbol_table = (asymbol **) malloc (storage_needed);
	number_of_symbols = bfd_canonicalize_symtab(obj, symbol_table);

	//notes: symbol_table[i]->flags & (BSF_FUNCTION | BSF_GLOBAL)
	for (i = 0; i < number_of_symbols; i++) {
		printl(PRINT_DEBUG, "name: %s, addr: %d, flags: %s, %s%s%s, in sect %s%s%s.\n",
		       symbol_table[i]->name,
		       (unsigned int)(symbol_table[i]->section->vma + symbol_table[i]->value),
		       (symbol_table[i]->flags & BSF_GLOBAL) ? "global" : "local",
		       (symbol_table[i]->flags & BSF_FUNCTION) ? "function" : "data",
		       symbol_table[i]->flags & BSF_SECTION_SYM ? ", section": "",
		       symbol_table[i]->flags & BSF_FILE ? ", file": "",
		       symbol_table[i]->section->name,
		       bfd_is_und_section(symbol_table[i]->section) ? ", undefined" : "",
		       symbol_table[i]->section->flags & SEC_RELOC ? ", relocate": "");
		//if(!strcmp(executive_entry_symbol, symbol_table[i]->name)){
		//return symbol_table[i]->section->vma + symbol_table[i]->value;
		//}
	}

	free(symbol_table);
	//printl(PRINT_DEBUG, "Unable to find symbol named %s\n", executive_entry_symbol);
	//return -1;
	return;
}
#endif

static void
findsections(asection *sect, PTR obj, int ld)
{
	struct cos_sections *css = obj;
	int i;

	for (i = 0 ; css[i].secid < MAXSEC_S ; i++) {
		if (!strcmp(css[i].sname, sect->name)) {
			if (ld) css[i].ldobj.s  = sect;
			else    css[i].srcobj.s = sect;
			return;
		}
	}
}
static void
findsections_srcobj(bfd *abfd, asection *sect, PTR obj) { findsections(sect, obj, 0); }
static void
findsections_ldobj(bfd *abfd, asection *sect, PTR obj) { findsections(sect, obj, 1); }

static int calc_offset(int offset, asection *sect)
{
	int align;

	if (!sect) return offset;
	align = (1 << sect->alignment_power) - 1;

	if (offset & align) {
		offset -= offset & align;
		offset += align + 1;
	}

	return offset;
}

static int calculate_mem_size(int first, int last)
{
	int offset = 0;
	int i;

	for (i = first; i < last; i++){
		asection *s = section_info[i].srcobj.s;

		if (s == NULL) {
			printl(PRINT_DEBUG, "Warning: could not find section for sectno %d @ %p.\n",
			       i, &(section_info[i].srcobj.s));
			continue;
		}
		offset = calc_offset(offset, s);
		section_info[i].srcobj.offset = offset;
		offset += bfd_get_section_size(s);
	}

	return offset;
}

static void emit_address(FILE *fp, unsigned long addr)
{
	fprintf(fp, ". = 0x%x;\n", (unsigned int)addr);
}

static void emit_section(FILE *fp, char *sec)
{
	/*
	 * The kleene star after the section will collapse
	 * .rodata.str1.x for all x into the only case we deal with
	 * which is .rodata
	 */
//	fprintf(fp, ".%s : { *(.%s*) }\n", sec, sec);
	fprintf(fp, "%s : { *(%s*) }\n", sec, sec);
}

/* Look at sections and determine sizes of the text and
 * and data portions of the object file once loaded */

static int genscript(int with_addr)
{
	FILE *fp;
	static unsigned int cnt = 0;
	int i;

	sprintf(script, "/tmp/loader_script.%d", getpid());
	sprintf(tmp_exec, "/tmp/loader_exec.%d.%d.%d", with_addr, getpid(), cnt);
	cnt++;

	fp = fopen(script, "w");
	if(fp == NULL){
		perror("fopen failed");
		exit(-1);
	}

	fprintf(fp, "SECTIONS\n{\n");

	for (i = 0 ; section_info[i].secid != MAXSEC_S ; i++) {
		if (with_addr && !section_info[i].coalesce) {
			emit_address(fp, section_info[i].start_addr);
		}
		if (section_info[i].ld_output) {
			fprintf(fp, "%s", section_info[i].ld_output);
		} else {
			emit_section(fp, section_info[i].sname);
		}
	}
	if (with_addr) emit_address(fp, 0);
	emit_section(fp, ".eh_frame");

	fprintf(fp, "}\n");
	fclose(fp);

	return 0;
}

static void run_linker(char *input_obj, char *output_exe)
{
	char linker_cmd[256];
	sprintf(linker_cmd, LINKER_BIN " -T %s -o %s %s", script, output_exe,
		input_obj);
	printl(PRINT_DEBUG, "%s\n", linker_cmd);
	fflush(stdout);
	system(linker_cmd);
}

#define bfd_sect_size(bfd, x) (bfd_get_section_size(x)/bfd_octets_per_byte(bfd))

int set_object_addresses(bfd *obj, struct service_symbs *obj_data)
{
	struct symb_type *st = &obj_data->exported;
	int i;

/* debug:
	unsigned int *retaddr;
	char **blah;
	typedef int (*fn_t)(void);
	fn_t fn;
	char *str = "hi, this is";

	if ((retaddr = (unsigned int*)getsym(obj, "ret")))
		printl(PRINT_DEBUG, "ret %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((retaddr = (unsigned int*)getsym(obj, "blah")))
		printl(PRINT_DEBUG, "blah %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((retaddr = (unsigned int*)getsym(obj, "zero")))
		printl(PRINT_DEBUG, "zero %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((blah = (char**)getsym(obj, "str")))
		printl(PRINT_DEBUG, "str %x is %s.\n", (unsigned int)blah, *blah);
	if ((retaddr = (unsigned int*)getsym(obj, "other")))
		printl(PRINT_DEBUG, "other %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((fn = (fn_t)getsym(obj, "foo")))
		printl(PRINT_DEBUG, "retval from foo: %d (%d).\n", fn(), (int)*str);
*/
	for (i = 0 ; i < st->num_symbs ; i++) {
		char *symb = st->symbs[i].name;
		unsigned long addr = getsym(obj, symb);
/*
		printl(PRINT_DEBUG, "Symbol %s at address 0x%x.\n", symb,
		       (unsigned int)addr);
*/
		if (addr == 0) {
			printl(PRINT_DEBUG, "Symbol %s has invalid address.\n", symb);
			return -1;
		}

		st->symbs[i].addr = addr;
	}

	return 0;
}

vaddr_t get_symb_address(struct symb_type *st, const char *symb)
{
	int i;

	for (i = 0 ; i < st->num_symbs ; i++ ) {
		if (!strcmp(st->symbs[i].name, symb)) {
			return st->symbs[i].addr;
		}
	}
	return 0;
}

static void
section_info_init(struct cos_sections *cs)
{
	int i;

	for (i = 0 ; cs[i].secid != MAXSEC_S ; i++) {
		cs[i].srcobj.s = cs[i].ldobj.s = NULL;
		cs[i].srcobj.offset = cs[i].ldobj.offset = 0;
	}
}

static int make_cobj_symbols(struct service_symbs *s, struct cobj_header *h);
static int make_cobj_caps(struct service_symbs *s, struct cobj_header *h);

static int
load_service(struct service_symbs *ret_data, unsigned long lower_addr, unsigned long size)
{
	bfd *obj, *objout;
	int sect_sz, offset, tot_static_mem = 0;
	void *ret_addr;
	char *service_name = ret_data->obj;
	struct cobj_header *h;
	int i;

	section_info_init(&section_info[0]);
	if (!service_name) {
		printl(PRINT_DEBUG, "Invalid path to executive.\n");
		return -1;
	}

	printl(PRINT_NORMAL, "Processing object %s:\n", service_name);

	/*
	 * First Phase: We need to learn about the object.  We need to
	 * get the addresses of each section that we care about,
	 * figure out proper alignments, and lengths of each section.
	 * This data will later be used to link the object into those
	 * locations.
	 */
	genscript(0);
	run_linker(service_name, tmp_exec);

	obj = bfd_openr(tmp_exec, "elf32-i386");
	if(!obj){
		bfd_perror("object open failure");
		return -1;
	}
	if(!bfd_check_format(obj, bfd_object)){
		printl(PRINT_DEBUG, "Not an object file!\n");
		return -1;
	}
	/*
	 * Initialize some section info (note that only sizes of
	 * sections are relevant now, as we haven't yet linked in
	 * their proper addresses.
	 */
	bfd_map_over_sections(obj, findsections_srcobj, section_info);

	/* Lets calculate and store starting addresses/alignments */
	offset = 0;
	sect_sz = 0;
	assert(round_to_page(lower_addr) == lower_addr);
	for (i = 0 ; csg(i)->secid < MAXSEC_S ; i++) {
		csg(i)->start_addr = lower_addr + offset;
		if (csg(i)->srcobj.s) {
			vaddr_t align_diff;

			if (csg(i)->srcobj.s->alignment_power) {
				align_diff          = round_up_to_pow2(csg(i)->start_addr,
								       1<<(csg(i)->srcobj.s->alignment_power)) -
					                               csg(i)->start_addr;
				offset             += align_diff;
				csg(i)->start_addr += align_diff;
			}
			printl(PRINT_DEBUG, "\t section %d, offset %d, align %x, start addr %x, align_diff %d\n",
			       i, offset, csg(i)->srcobj.s->alignment_power, (unsigned int)csg(i)->start_addr, (int)align_diff);

			sect_sz = calculate_mem_size(i, i+1);
			/* make sure we're following the object's
			 * alignment constraints */
			assert(!(csg(i)->start_addr &
				 ((1 << csg(i)->srcobj.s->alignment_power)-1)));
		} else {
			sect_sz = 0;
		}
		csg(i)->len        = sect_sz;

		if (csg(i+1)->coalesce) {
			offset += sect_sz;
		} else {
			offset = round_up_to_page(offset + sect_sz);
		}
		printl(PRINT_DEBUG, "\tSect %d, addr %lx, sz %lx, offset %x\n",
		       i, csg(i)->start_addr, csg(i)->len, offset);
	}

	/* Allocate memory for any components that are Linux-loaded */
	if (!is_loaded_by_llboot(ret_data)) {
		unsigned long tot_sz = 0;
		unsigned long start_addr = csg(0)->start_addr;

		assert(start_addr);
		/* Amount of required memory is the last section's end, minus the start */
		for (i = 0 ; csg(i)->secid < MAXSEC_S ; i++) {
			if (!csg(i)->len) continue;
			tot_sz = round_up_to_page((csg(i)->start_addr - start_addr) + csg(i)->len);
		}
		printl(PRINT_DEBUG, "Total mmap size %lx\n", tot_sz);
		/**
		 * FIXME: needing PROT_WRITE is daft, should write to
		 * file, then map in ro
		 */
		assert(tot_sz);
		tot_static_mem = tot_sz;
		ret_addr = mmap((void*)start_addr, tot_sz,
				PROT_EXEC | PROT_READ | PROT_WRITE,
				MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
				0, 0);
		if (MAP_FAILED == (void*)ret_addr){
			/* If you get outrageous sizes here, there is
			 * a required section missing (such as
			 * rodata).  Add a const. */
			printl(PRINT_DEBUG, "Error mapping 0x%x(0x%x)\n",
			       (unsigned int)start_addr, (unsigned int)tot_sz);
			perror("Couldn't map text segment into address space");
			return -1;
		}
	} else {
		/* Create the cobj for the component if it is composite/booter-loaded */
		u32_t size = 0, obj_size;
		u32_t nsymbs, ncaps, nsects;
		char *mem;
		char cobj_name[COBJ_NAME_SZ], *end;
		int i;

		for (i = 0 ; csg(i)->secid < MAXSEC_S ; i++) {
			tot_static_mem += csg(i)->len;
			if (csg(i)->cobj_flags & COBJ_SECT_ZEROS) continue;
			size += csg(i)->len;
		}
		nsymbs = ret_data->exported.num_symbs;
		ncaps  = ret_data->undef.num_symbs;
		nsects = MAXSEC_S;

		obj_size = cobj_size_req(nsects, size, nsymbs, ncaps);
		mem = malloc(obj_size);
		if (!mem) {
			printl(PRINT_HIGH, "could not allocate memory for composite-loaded %s.\n", service_name);
			return -1;
		}
		strncpy(cobj_name, &service_name[5], COBJ_NAME_SZ);
		end = strstr(cobj_name, ".o.");
		if (!end) end = &cobj_name[COBJ_NAME_SZ-1];
		*end = '\0';
		h = cobj_create(0, cobj_name, nsects, size, nsymbs, ncaps, mem, obj_size,
				ret_data->scheduler ? COBJ_INIT_THD : 0);
		if (!h) {
			printl(PRINT_HIGH, "boot component: couldn't create cobj.\n");
			return -1;
		}
		ret_data->cobj = h;
		assert(obj_size == h->size);
	}
	unlink(tmp_exec);

	/*
	 * Second Phase: Now we know the memory layout of the object,
	 * and have the destination memory for the object's data to be
	 * placed into (either mmaped -- Linux loaded, or cobj -- for
	 * composite/booter-loaded).  Lets link it into the proper
	 * addresses, and copy the resulting object data into the
	 * proper memory locations.
	 */
	genscript(1);
	run_linker(service_name, tmp_exec);
//	unlink(script);
	objout = bfd_openr(tmp_exec, "elf32-i386");
	if(!objout){
		bfd_perror("Object open failure\n");
		return -1;
	}
	if(!bfd_check_format(objout, bfd_object)){
		printl(PRINT_DEBUG, "Not an object file!\n");
		return -1;
	}

	/* Now create the linked objects... */
	bfd_map_over_sections(objout, findsections_ldobj, section_info);

	for (i = 0 ; csg(i)->secid < MAXSEC_S ; i++) {
		printl(PRINT_DEBUG, "\tRetreiving section %d of size %lx @ %lx.\n", i, csg(i)->len, csg(i)->start_addr);

		if (!is_loaded_by_llboot(ret_data)) {
			if (csg(i)->ldobj.s) {
				bfd_get_section_contents(objout, csg(i)->ldobj.s, (char*)csg(i)->start_addr, 0, csg(i)->len);
			}
		} else {
			char *sect_loc;

			if (cobj_sect_init(h, i, csg(i)->cobj_flags, csg(i)->start_addr, csg(i)->len)) {
				printl(PRINT_HIGH, "Could not create section %d in cobj for %s\n", i, service_name);
				return -1;
			}
			if (csg(i)->cobj_flags & COBJ_SECT_ZEROS) continue;
			sect_loc = cobj_sect_contents(h, i);
			printl(PRINT_DEBUG, "\tSection @ %d, size %d, addr %x, sect start %d\n", (u32_t)sect_loc-(u32_t)h,
			       cobj_sect_size(h, i), cobj_sect_addr(h, i), cobj_sect_content_offset(h));
			assert(sect_loc);
			//memcpy(sect_loc, tmp_storage, csg(i)->len);
			if (csg(i)->ldobj.s) {
				bfd_get_section_contents(objout, csg(i)->ldobj.s, sect_loc, 0, csg(i)->len);
			}
		}
	}

	if (set_object_addresses(objout, ret_data)) {
		printl(PRINT_DEBUG, "Could not find all object symbols.\n");
		return -1;
	}

	ret_data->lower_addr = lower_addr;
	ret_data->size       = size;
	ret_data->allocated  = round_up_to_page((csg(MAXSEC_S-1)->start_addr - csg(0)->start_addr) + csg(MAXSEC_S-1)->len);
	ret_data->heap_top   = csg(0)->start_addr + ret_data->allocated;

	if (is_loaded_by_llboot(ret_data)) {
		if (make_cobj_symbols(ret_data, h)) {
			printl(PRINT_HIGH, "Could not create symbols in cobj for %s\n", service_name);
			return -1;
		}
	}


	bfd_close(obj);
	bfd_close(objout);

	printl(PRINT_NORMAL, "Object %s processed as %s with script %s.\n",
	       service_name, tmp_exec, script);
	unlink(tmp_exec);

	return tot_static_mem;
}

/* FIXME: should modify code to use
static struct service_symbs *get_dependency_by_index(struct service_symbs *s,
						     int index)
{
	if (index >= s->num_dependencies) {
		return NULL;
	}

	return s->dependencies[index].dep;
}
*/
static int __add_service_dependency(struct service_symbs *s, struct service_symbs *dep,
				    char *modifier, int mod_len)
{
	struct dependency *d;

	if (!s || !dep || s->num_dependencies == MAX_TRUSTED) {
		return -1;
	}
	if (!is_loaded_by_llboot(s) && is_loaded_by_llboot(dep)) {
		printl(PRINT_HIGH, "Error: Non-Composite-loaded component dependent on composite loaded component.\n");
		return -1;
	}

	d = &s->dependencies[s->num_dependencies];
	d->dep = dep;
	d->modifier = modifier;
	d->mod_len = mod_len;
	d->resolved = 0;
	s->num_dependencies++;

	return 0;
}

static int add_service_dependency(struct service_symbs *s,
				  struct service_symbs *dep)
{
	return __add_service_dependency(s, dep, NULL, 0);
}

static int add_modified_service_dependency(struct service_symbs *s,
					   struct service_symbs *dep,
					   char *modifier, int mod_len)
{
	char *new_mod;

	assert(modifier);
	new_mod = malloc(mod_len+1);
	assert(new_mod);
	memcpy(new_mod, modifier, mod_len);
	new_mod[mod_len] = '\0';

	return __add_service_dependency(s, dep, new_mod, mod_len);
}

static int initialize_service_symbs(struct service_symbs *str)
{
	memset(str, 0, sizeof(struct service_symbs));
	str->exported.parent = str;
	str->undef.parent = str;
	str->next = NULL;
	str->exported.num_symbs = str->undef.num_symbs = 0;
	str->num_dependencies = 0;
	str->depth = -1;

	return 0;
}

struct component_traits {
	int sched, composite_loaded;
};

static void parse_component_traits(char *name, struct component_traits *t, int *off, int layer)
{
	switch(name[*off]) {
	case '*': {
		t->sched = 1;
		if (!ROOT_SCHED) {
			char *r = malloc(strlen(name+1)+1);
			strcpy(r, name+1);
			ROOT_SCHED = r;
		}
		break;
	}
	case '!': t->composite_loaded = layer; break;
	default: /* base case */ return;
	}
	(*off)++;
	parse_component_traits(name, t, off, layer);

	return;
}

static struct service_symbs *alloc_service_symbs(char *obj, int boot_layer)
{
	struct service_symbs *str;
	char *obj_name = malloc(strlen(obj)+1), *cpy, *orig, *pos;
	const char lassign = '(', *rassign = ")", *assign = "=";
	struct component_traits t = {.sched = 0, .composite_loaded = 0};
	int off = 0;

	parse_component_traits(obj, &t, &off, boot_layer);
	assert(t.composite_loaded >= 0);
	assert(obj_name);
	/* Do we have a value assignment (a component copy)?  Syntax
	 * is (newval=oldval),... */
	if (obj[off] == lassign) {
		char copy_cmd[256];
		int ret;

		off++;
		parse_component_traits(obj, &t, &off, boot_layer);

		cpy = strtok_r(obj+off, assign, &pos);
		orig = strtok_r(pos, rassign, &pos);
		sprintf(copy_cmd, "cp %s %s", orig, cpy);
		ret = system(copy_cmd);
		assert(-1 != ret);
		obj = cpy;
		off = 0;
	}
	printl(PRINT_DEBUG, "Processed object %s (%s booted by layer %d)\n", obj, t.sched ? "scheduler " : "",
	       t.composite_loaded);
	str = malloc(sizeof(struct service_symbs));
	if (!str || initialize_service_symbs(str)) {
		return NULL;
	}

	strcpy(obj_name, &obj[off]);
	str->obj = obj_name;

	str->is_scheduler = t.sched;
	str->scheduler = NULL;
	str->is_composite_loaded = t.composite_loaded;
	str->already_loaded = 0;

	return str;
}

static void free_symbs(struct symb_type *st)
{
	int i;

	for (i = 0 ; i < st->num_symbs ; i++) {
		free(st->symbs[i].name);
	}
}

static void free_service_symbs(struct service_symbs *str)
{
	free(str->obj);
	free_symbs(&str->exported);
	free_symbs(&str->undef);
	free(str);

	return;
}

static int obs_serialize(asymbol *symb, void *data)
{
	struct symb_type *symbs = data;
	char *name;

	/* So that we can later add into the exported symbols the user
         * capability table
	 */
	if (symbs->num_symbs >= MAX_SYMBOLS-NUM_KERN_SYMBS) {
		printl(PRINT_DEBUG, "Have exceeded the number of allowed "
		       "symbols for object %s.\n", symbs->parent->obj);
		return -1;
	}

	/* Ignore main */
	if (!strcmp("main", symb->name)) {
		return 0;
	}

	name = malloc(strlen(symb->name) + 1);
	strcpy(name, symb->name);

	symbs->symbs[symbs->num_symbs].name = name;
	symbs->symbs[symbs->num_symbs].addr = 0;
	symbs->num_symbs++;

	return 0;
}

static int for_each_symb_type(bfd *obj, int symb_type, observer_t o, void *obs_data)
{
	long storage_needed;
	asymbol **symbol_table;
	long number_of_symbols;
	int i;

	storage_needed = bfd_get_symtab_upper_bound(obj);

	if (storage_needed <= 0){
		printl(PRINT_DEBUG, "no symbols in object file\n");
		exit(-1);
	}

	symbol_table = (asymbol **) malloc (storage_needed);
	number_of_symbols = bfd_canonicalize_symtab(obj, symbol_table);

	for (i = 0; i < number_of_symbols; i++) {
		/*
		 * Invoke the observer if we are interested in a type,
		 * and the symbol is of that type where type is either
		 * undefined or exported, currently
		 */
		if ((symb_type & UNDEF_SYMB_TYPE &&
		    bfd_is_und_section(symbol_table[i]->section))
		    ||
		    (symb_type & EXPORTED_SYMB_TYPE &&
		    symbol_table[i]->flags & BSF_FUNCTION &&
		    ((symbol_table[i]->flags & BSF_GLOBAL) ||
		     (symbol_table[i]->flags & BSF_WEAK)))) {
			if ((*o)(symbol_table[i], obs_data)) {
				return -1;
			}
		}
	}

	free(symbol_table);

	return 0;
}

/*
 * Fill in the symbols of service_symbs for the object passed in as
 * the tmp_exec
 */
static int obj_serialize_symbols(char *tmp_exec, int symb_type, struct service_symbs *str)
{
 	bfd *obj;
	struct symb_type *st;

	obj = bfd_openr(tmp_exec, "elf32-i386");
	if(!obj){
		printl(PRINT_HIGH, "Attempting to open %s\n", str->obj);
		bfd_perror("Object open failure");
		return -1;
	}

	if(!bfd_check_format(obj, bfd_object)){
		printl(PRINT_DEBUG, "Not an object file!\n");
		return -1;
	}

	if (symb_type == UNDEF_SYMB_TYPE) {
		st = &str->undef;
	} else if (symb_type == EXPORTED_SYMB_TYPE) {
		st = &str->exported;
	} else {
		printl(PRINT_HIGH, "attempt to view unknown symbol type\n");
		exit(-1);
	}
	for_each_symb_type(obj, symb_type, obs_serialize, st);

	bfd_close(obj);

	return 0;
}

static inline void print_symbs(struct symb_type *st)
{
	int i;

	for (i = 0 ; i < st->num_symbs ; i++) {
		printl(PRINT_DEBUG, "%s, ", st->symbs[i].name);
	}

	return;
}

static void print_objs_symbs(struct service_symbs *str)
{
	if (print_lvl < PRINT_DEBUG) return;
	while (str) {
		printl(PRINT_DEBUG, "Service %s:\n\tExported functions: ", str->obj);
		print_symbs(&str->exported);
		printl(PRINT_DEBUG, "\n\tUndefined: ");
		print_symbs(&str->undef);
		printl(PRINT_DEBUG, "\n\n");

		str = str->next;
	}

	return;
}

/*
 * Has this service already been processed?
 */
static inline int service_processed(char *obj_name, struct service_symbs *services)
{
	while (services) {
		if (!strcmp(services->obj, obj_name)) {
			return 1;
		}
		services = services->next;
	}

	return 0;
}

static int
symb_already_undef(struct service_symbs *ss, const char *name)
{
	int i;
	struct symb_type *undef = &ss->undef;

	for (i = 0 ; i < undef->num_symbs ; i++) {
		if (!strcmp(undef->symbs[i].name, name)) return 1;
	}
	return 0;
}

static inline void
__add_symb(const char *name, struct symb_type *exp_undef, int mod_len)
{
	exp_undef->symbs[exp_undef->num_symbs].name = malloc(strlen(name)+1);
	exp_undef->symbs[exp_undef->num_symbs].modifier_offset = mod_len;
	strcpy(exp_undef->symbs[exp_undef->num_symbs].name, name);
	exp_undef->num_symbs++;
	assert(exp_undef->num_symbs <= MAX_SYMBOLS);
}

static inline void add_kexport(struct service_symbs *ss, const char *name)
{
	struct symb_type *ex = &ss->exported;
	__add_symb(name, ex, 0);
	return;
}

static inline void add_undef_symb(struct service_symbs *ss, const char *name, int mod_len)
{
	struct symb_type *ud = &ss->undef;
	__add_symb(name, ud, mod_len);
	return;
}

/*
 * Assume that these are added LAST.  The last NUM_KERN_SYMBS are
 * ignored for most purposes so they must be the actual kern_syms.
 *
 * The kernel needs to know where a few symbols are, add them:
 * user_caps, cos_sched_notifications
 */
static void add_kernel_exports(struct service_symbs *service)
{
	add_kexport(service, COMP_INFO);
	add_kexport(service, SCHED_NOTIF);

	return;
}

/*
 * Obtain the list of undefined and exported symbols for a collection
 * of services.
 *
 * services is an array of comma delimited addresses to the services
 * we wish to get the symbol information for.  Note that all ',' in
 * the services string are replaced with '\0', and that this function
 * is not thread-safe due to use of strtok.
 *
 * Returns a linked list struct service_symbs data structure with the
 * arrays within each service_symbs filled in to reflect the symbols
 * within that service.
 */
static struct service_symbs *prepare_service_symbs(char *services)
{
	struct service_symbs *str, *first;
	const char *init_delim = ",", *serv_delim = ";";
	char *tok, *init_str;
	int len;
	int boot_layer = -1;

	printl(PRINT_DEBUG, "Prepare the list of components.\n");

	tok = strtok(services, init_delim);
	first = str = alloc_service_symbs(tok, boot_layer);
	if (strstr(tok, "boot")) ++boot_layer;
	init_str = strtok(NULL, serv_delim);
	len = strlen(init_str)+1;
	str->init_str = malloc(len);
	assert(str->init_str);
	memcpy(str->init_str, init_str, len);

	do {
		if (obj_serialize_symbols(str->obj, EXPORTED_SYMB_TYPE, str) ||
		    obj_serialize_symbols(str->obj, UNDEF_SYMB_TYPE, str)) {
			printl(PRINT_DEBUG, "Could not operate on object %s: error.\n", tok);
			return NULL;
		}
		add_kernel_exports(str);
		tok = strtok(NULL, init_delim);
		if (tok) {
			str->next = alloc_service_symbs(tok, boot_layer);
			str = str->next;
			if (strstr(tok, "boot")) ++boot_layer;

			init_str = strtok(NULL, serv_delim);
			len = strlen(init_str)+1;
			str->init_str = malloc(len);
			assert(str->init_str);
			memcpy(str->init_str, init_str, len);
		}
	} while (tok);

	return first;
}


/*
 * Find the exporter for a specific symbol from amongst a list of
 * exporters.
 */
static inline
struct service_symbs *find_symbol_exporter_mark_resolved(struct symb *s,
							 struct dependency *exporters,
							 int num_exporters, struct symb **exported)
{
	int i,j;

	for (i = 0 ; i < num_exporters ; i++) {
		struct dependency *exporter;
		struct symb_type *exp_symbs;

		exporter = &exporters[i];
		exp_symbs = &exporter->dep->exported;

		for (j = 0 ; j < exp_symbs->num_symbs ; j++) {
			if (!strcmp(s->name, exp_symbs->symbs[j].name)) {
				*exported = &exp_symbs->symbs[j];
				exporters[i].resolved = 1;
				return exporters[i].dep;
			}
			if (exporter->modifier &&
			    !strncmp(s->name, exporter->modifier, exporter->mod_len) &&
			    !strcmp(s->name + exporter->mod_len, exp_symbs->symbs[j].name)) {
				*exported = &exp_symbs->symbs[j];
				exporters[i].resolved = 1;
				return exporters[i].dep;
			}
		}
	}

	return NULL;
}

static int
create_transparent_capabilities(struct service_symbs *service)
{
	int i, j, fault_found[COS_FLT_MAX], other_found = 0;
	struct dependency *dep = service->dependencies;

	memset(fault_found, 0, sizeof(int) * COS_FLT_MAX);

	for (i = 0 ; i < service->num_dependencies ; i++) {
		struct symb_type *symbs = &dep[i].dep->exported;
		char *modifier = dep[i].modifier;
		int mod_len    = dep[i].mod_len;

		for (j = 0 ; j < symbs->num_symbs ; j++) {
			trans_cap_t r;
			int fltn;

			r = is_transparent_capability(&symbs->symbs[j], &fltn);
			switch (r) {
			case TRANS_CAP_FAULT:
				if (fault_found[fltn]) break;
				fault_found[fltn] = 1;
			case TRANS_CAP_SCHED:
			{
				struct symb_type *st;
				struct symb *s;
				char mod_name[256]; /* arbitrary value... */

//				if (symb_already_undef(service, symbs->symbs[j].name)) break;
				if (r == TRANS_CAP_SCHED) {
					/* This is pretty crap: We
					 * don't want to ad a
					 * capability to a parent
					 * scheduler if we already
					 * have a capability
					 * established, but we _do_
					 * want to create a capability
					 * for other special
					 * symbols. */
					if (dep[i].resolved) continue;
					if (other_found) break;
					other_found = 1;
				}

				assert(strlen(symbs->symbs[j].name) + mod_len < 256);
				strncpy(mod_name, modifier, mod_len);
				mod_name[mod_len] = '\0';
				strcat(mod_name, symbs->symbs[j].name);

				add_undef_symb(service, mod_name, mod_len);
				st = &service->undef;
				s = &st->symbs[st->num_symbs-1];
				s->exporter = dep[i].dep;
				s->exported_symb = &symbs->symbs[j];

				//service->obj, s->exporter->obj, s->exported_symb->name);

				dep[i].resolved = 1;
				break;
			}
			case TRANS_CAP_NIL: break;
			}
		}
		if (!dep[i].resolved) {
			printl(PRINT_HIGH, "Warning: dependency %s-%s "
			       "is not creating a capability.\n",
			       service->obj, dep[i].dep->obj);
		}
	}

	return 0;
}

/*
 * Verify that all symbols can be resolved by the present dependency
 * relations.
 *
 * Assumptions: All exported and undefined symbols are defined for
 * each service (prepare_service_symbs has been called), and that the
 * tree of services has been established designating the dependents of
 * each service (process_dependencies has been called).
 */
static int verify_dependency_completeness(struct service_symbs *services)
{
	struct service_symbs *start = services;
	int ret = 0;
	int i;

	/* for each of the services... */
	for (; services ; services = services->next) {
		struct symb_type *undef_symbs = &services->undef;

		/* ...go through each of its undefined symbols... */
		for (i = 0 ; i < undef_symbs->num_symbs ; i++) {
			struct symb *symb = &undef_symbs->symbs[i];
			struct symb *exp_symb;
			struct service_symbs *exporter;

			/*
			 * ...and make sure they are matched to an
			 * exported function in a service we are
			 * dependent on.
			 */
			exporter = find_symbol_exporter_mark_resolved(symb, services->dependencies,
								      services->num_dependencies, &exp_symb);
			if (!exporter) {
				printl(PRINT_HIGH, "Could not find exporter of symbol %s in service %s.\n",
				       symb->name, services->obj);

				ret = -1;
				goto exit;
			} else {
				symb->exporter = exporter;
				symb->exported_symb = exp_symb;
			}
			/* if (exporter->is_scheduler) { */
			/* 	if (NULL == services->scheduler) { */
			/* 		services->scheduler = exporter; */
			/* 		//printl(PRINT_HIGH, "%s has scheduler %s.\n", services->obj, exporter->obj); */
			/* 	} else if (exporter != services->scheduler) { */
			/* 		printl(PRINT_HIGH, "Service %s is dependent on more than one scheduler (at least %s and %s).  Error.\n", services->obj, exporter->obj, services->scheduler->obj); */
			/* 		ret = -1; */
			/* 		goto exit; */
			/* 	} */
			/* } */
		}
	}

	for (services = start ; services ; services = services->next) {
		create_transparent_capabilities(services);
	}
 exit:
	return ret;
}

static int rec_verify_dag(struct service_symbs *services,
			  int current_depth, int max_depth)
{
	int i;

	/* cycle */
	if (current_depth > max_depth) {
		return -1;
	}

	if (current_depth > services->depth) {
		services->depth = current_depth;
	}

	for (i = 0 ; i < services->num_dependencies ; i++) {
		struct service_symbs *d = services->dependencies[i].dep;

		if (rec_verify_dag(d, current_depth+1, max_depth)) {
			printl(PRINT_HIGH, "Component %s found in cycle\n", d->obj);
			return -1;
		}
	}

	return 0;
}

/*
 * FIXME: does not check for disjoint graphs at this time.
 *
 * The only soundness we can really check here is that services are
 * arranged in a DAG, i.e. that no cycles exist.  O(N^2*E).
 *
 * Assumptions: All exported and undefined symbols are defined for
 * each service (prepare_service_symbs has been called), and that the
 * tree of services has been established designating the dependents of
 * each service (process_dependencies has been called).
 */
static int verify_dependency_soundness(struct service_symbs *services)
{
	struct service_symbs *tmp_s = services;
	int cnt = 0;

	while (tmp_s) {
		cnt++;
		tmp_s = tmp_s->next;
	}

	while (services) {
		if (rec_verify_dag(services, 0, cnt)) {
			printl(PRINT_DEBUG, "Cycle found in dependencies.  Not linking.\n");
			return -1;
		}

		services = services->next;
	}

	return 0;
}

static inline struct service_symbs *get_service_struct(char *name,
						       struct service_symbs *list)
{
	while (list) {
		assert(name);
		assert(list && list->obj);
		if (!strcmp(name, list->obj)) {
			return list;
		}

		list = list->next;
	}

	return NULL;
}

/*
 * Add to the service_symbs structures the dependents.
 *
 * deps is formatted as "sa-sb|sc|...|sn;sd-se|sf|...;...", or a list
 * of "service" hyphen "dependencies...".  In the above example, sa
 * depends on functions within sb, sc, and sn.
 */
static int deserialize_dependencies(char *deps, struct service_symbs *services)
{
	char *next, *current;
	char *serial = "-";
	char *parallel = "|";
	char inter_dep = ';';
	char open_modifier = '[';
	char close_modifier = ']';

	if (!deps) return -1;
	next = current = deps;

	/* go through each dependent-trusted|... relation */
	while (current) {
		struct service_symbs *s, *dep;
		char *tmp;

		next = strchr(current, inter_dep);
		if (next) {
			*next = '\0';
			next++;
		}
		/* the dependent */
		tmp = strtok(current, serial);
		s = get_service_struct(tmp, services);
		if (!s) {
			printl(PRINT_HIGH, "Could not find service %s.\n", tmp);
			return -1;
		}

		/* go through the | invoked services */
		tmp = strtok(NULL, parallel);
		while (tmp) {
			char *mod = NULL;
			/* modifier! */
			if (tmp[0] == open_modifier) {
				mod = tmp+1;
				tmp = strchr(tmp, close_modifier);
				if (!tmp) {
					printl(PRINT_HIGH, "Could not find closing modifier ] in %s\n", mod);
					return -1;
				}
				*tmp = '\0';
				tmp++;
				assert(mod);
				assert(tmp);
			}
			dep = get_service_struct(tmp, services);
			if (!dep) {
				printl(PRINT_HIGH, "Could not find service %s.\n", tmp);
				return -1;
			}
			if (dep == s) {
				printl(PRINT_HIGH, "Reflexive relations not allowed (for %s).\n",
				       s->obj);
				return -1;
			}

			if (!is_loaded_by_llboot(s) && is_loaded_by_llboot(dep)) {
				printl(PRINT_HIGH, "Error: Non-Composite-loaded component %s dependent "
				       "on composite loaded component %s.\n", s->obj, dep->obj);
				return -1;
			}

			if (mod) add_modified_service_dependency(s, dep, mod, strlen(mod));
			else add_service_dependency(s, dep);

			if (dep->is_scheduler) {
				if (NULL == s->scheduler) {
					s->scheduler = dep;
				} else if (dep != s->scheduler) {
					printl(PRINT_HIGH, "Service %s is dependent on more than "
					       "one scheduler (at least %s and %s).  Error.\n",
					       s->obj, dep->obj, s->scheduler->obj);
					return -1;
				}
			}

			tmp = strtok(NULL, parallel);
		}

		current = next;
	}

	return 0;
}

static char *strip_prepended_path(char *name)
{
	char *tmp;

	tmp = strrchr(name, '/');

	if (!tmp) {
		return name;
	} else {
		return tmp+1;
	}
}

/*
 * Produces a number of object files in /tmp named objname.o.pid.o
 * with no external dependencies.
 *
 * gen_stub_prog is the address to the client stub generation prog
 * st_object is the address of the symmetric trust object.
 *
 * This is kind of a big hack.
 */
static void gen_stubs_and_link(char *gen_stub_prog, struct service_symbs *services)
{
	int pid = getpid();
	char tmp_str[2048];

	while (services) {
		int i;
		struct symb_type *symbs = &services->undef;
		char dest[256];
		char tmp_name[256];
		char *obj_name, *orig_name, *str;

		orig_name = services->obj;
		obj_name = strip_prepended_path(services->obj);
		sprintf(tmp_name, "/tmp/%s.%d", obj_name, pid);

/*		if (symbs->num_symbs == 0) {
			sprintf(tmp_str, "cp %s %s.o",
				orig_name, tmp_name);
			system(tmp_str);

			str = malloc(strlen(tmp_name)+3);
			strcpy(str, tmp_name);
			strcat(str, ".o");
			free(services->obj);
			services->obj = str;

			services = services->next;
			continue;
		}
*/
		/* make the command line for an invoke the stub generator */
		strcpy(tmp_str, gen_stub_prog);

		if (symbs->num_symbs > 0) {
			strcat(tmp_str, " ");
			strcat(tmp_str, symbs->symbs[0].name);
		}
		for (i = 1 ; i < symbs->num_symbs ; i++) {
			strcat(tmp_str, ",");
			strcat(tmp_str, symbs->symbs[i].name);
		}

		/* invoke the stub generator */
		sprintf(dest, " > %s_stub.S", tmp_name);
		strcat(tmp_str, dest);
		printl(PRINT_DEBUG, "%s\n", tmp_str);
		system(tmp_str);

		/* compile the stub */
		sprintf(tmp_str, GCC_BIN " -c -o %s_stub.o %s_stub.S",
			tmp_name, tmp_name);
		system(tmp_str);

		/* link the stub to the service */
		sprintf(tmp_str, LINKER_BIN " -r -o %s.o %s %s_stub.o",
			tmp_name, orig_name, tmp_name);
		system(tmp_str);

		/* Make service names reflect their new linked versions */
		str = malloc(strlen(tmp_name)+3);
		strcpy(str, tmp_name);
		strcat(str, ".o");
		free(services->obj);
		services->obj = str;

		sprintf(tmp_str, "rm %s_stub.o %s_stub.S", tmp_name, tmp_name);
		system(tmp_str);

		services = services->next;
	}

	return;
}

static u32_t llboot_mem;
/*
 * Load into the current address space all of the services.
 *
 * FIXME: Load intelligently, from the most trusted to the least in
 * some order instead of randomly.  This will be important when we do
 * dynamically loading.
 *
 * Assumes that a file exists for each service in /tmp/service.o.pid.o
 * (i.e. that gen_stubs_and_link has been called.)
 */
static int load_all_services(struct service_symbs *services)
{
	unsigned long service_addr = BASE_SERVICE_ADDRESS;
	int n_regions;
	long sz;

	while (services) {
		sz = services->mem_size = load_service(services, service_addr, DEFAULT_SERVICE_SIZE);
		if (!sz) return -1;

		if (strstr(services->obj, LLBOOT_COMP)) llboot_mem = sz;

		service_addr += DEFAULT_SERVICE_SIZE;
		/* note this works for the llbooter and root memory manager too */
		if (strstr(services->obj, BOOT_COMP) || strstr(services->obj, INITMM) ||
		    strstr(services->obj, LLBOOT_COMP)) {
			// Give Booter and MM larger VAS
			service_addr += (BOOTER_NREGIONS-1)*DEFAULT_SERVICE_SIZE;
		} else if (sz > DEFAULT_SERVICE_SIZE) {
			n_regions = (sz / DEFAULT_SERVICE_SIZE) + 1;
			service_addr += (n_regions-1) * DEFAULT_SERVICE_SIZE;
		}

		printl(PRINT_DEBUG, "\n");
		services = services->next;
	}

	return 0;
}

static void print_kern_symbs(struct service_symbs *services)
{
	const char *u_tbl = COMP_INFO;

	while (services) {
		vaddr_t addr;

		if ((addr = get_symb_address(&services->exported, u_tbl))) {
			printl(PRINT_DEBUG, "Service %s:\n\tusr_cap_tbl: %x\n",
			       services->obj, (unsigned int)addr);
		}

		services = services->next;
	}
}

/* static void add_spds(struct service_symbs *services) */
/* { */
/* 	struct service_symbs *s = services; */

/* 	/\* first, make sure that all services have spds *\/ */
/* 	while (s) { */
/* 		int num_undef = s->undef.num_symbs; */
/* 		struct usr_inv_cap *ucap_tbl; */

/* 		ucap_tbl = (struct usr_inv_cap*)get_symb_address(&s->exported,  */
/* 								 USER_CAP_TBL_NAME); */
/* 		/\* no external dependencies, no caps *\/ */
/* 		if (!ucap_tbl) { */
/* 			s->spd = spd_alloc(0, MNULL); */
/* 		} else { */
/* //			printl(PRINT_DEBUG, "Requesting %d caps.\n", num_undef); */
/* 			s->spd = spd_alloc(num_undef, ucap_tbl); */
/* 		} */

/* //		printl(PRINT_DEBUG, "Service %s has spd %x.\n", s->obj, (unsigned int)s->spd); */

/* 		s = s->next; */
/* 	} */

/* 	/\* then add the capabilities *\/ */
/* 	while (services) { */
/* 		int i; */
/* 		int num_undef = services->undef.num_symbs; */

/* 		for (i = 0 ; i < num_undef ; i++) { */
/* 			struct spd *owner_spd, *dest_spd; */
/* 			struct service_symbs *dest_service; */
/* 			vaddr_t dest_entry_fn; */
/* 			struct symb *symb; */

/* 			owner_spd = services->spd; */
/* 			symb = &services->undef.symbs[i]; */
/* 			dest_service = symb->exporter; */

/* 			symb = symb->exported_symb; */
/* 			dest_spd = dest_service->spd; */
/* 			dest_entry_fn = symb->addr; */

/* 			if ((spd_add_static_cap(services->spd, dest_entry_fn, dest_spd, IL_ST) == 0)) { */
/* 				printl(PRINT_DEBUG, "Could not add capability for %s to %s.\n",  */
/* 				       symb->name, dest_service->obj); */
/* 			} */
/* 		} */

/* 		services = services->next; */
/* 	} */

/* 	return; */
/* } */

/* void start_composite(struct service_symbs *services) */
/* { */
/* 	struct thread *thd; */

/* 	spd_init(); */
/* 	ipc_init(); */
/* 	thd_init(); */

/* 	add_spds(services); */

/* 	thd = thd_alloc(services->spd); */

/* 	if (!thd) { */
/* 		printl(PRINT_DEBUG, "Could not allocate thread.\n"); */
/* 		return; */
/* 	} */

/* 	thd_set_current(thd); */

/* 	return; */
/* } */

#include "../module/aed_ioctl.h"

/*
 * FIXME: all the exit(-1) -> return NULL, and handling in calling
 * function.
 */
/*struct cap_info **/
int create_invocation_cap(struct spd_info *from_spd, struct service_symbs *from_obj,
			  struct spd_info *to_spd, struct service_symbs *to_obj,
			  int cos_fd, char *client_fn, char *client_stub,
			  char *server_stub, char *server_fn, int flags)
{
	struct cap_info cap;
	struct symb_type *st = &from_obj->undef;

	vaddr_t addr;
	int i;

	/* find in what position the symbol was inserted into the
	 * user-level capability table (which position was opted for
	 * use), so that we can insert the information into the
	 * correct user-capability. */
	for (i = 0 ; i < st->num_symbs ; i++) {
		if (strcmp(client_fn, st->symbs[i].name) == 0) {
			break;
		}
	}

	if (i == st->num_symbs) {
		printl(PRINT_DEBUG, "Could not find the undefined symbol %s in %s.\n",
		       server_fn, from_obj->obj);
		exit(-1);
	}

	addr = (vaddr_t)get_symb_address(&to_obj->exported, server_stub);
	if (addr == 0) {
		printl(PRINT_DEBUG, "Could not find %s in %s.\n", server_stub, to_obj->obj);
		exit(-1);
	}
	cap.SD_serv_stub = addr;
	addr = (vaddr_t)get_symb_address(&from_obj->exported, client_stub);
	if (addr == 0) {
		printl(PRINT_DEBUG, "could not find %s in %s.\n", client_stub, from_obj->obj);
		exit(-1);
	}
	cap.SD_cli_stub = addr;
	addr = (vaddr_t)get_symb_address(&to_obj->exported, server_fn);
	if (addr == 0) {
		printl(PRINT_DEBUG, "could not find %s in %s.\n", server_fn, to_obj->obj);
		exit(-1);
	}
	cap.ST_serv_entry = addr;

	cap.rel_offset = i;
	cap.owner_spd_handle = from_spd->spd_handle;
	cap.dest_spd_handle = to_spd->spd_handle;
	cap.il = 3;
	cap.flags = flags;

	cap.cap_handle = cos_spd_add_cap(cos_fd, &cap);

 	if (cap.cap_handle == 0) {
		printl(PRINT_DEBUG, "Could not add capability # %d to %s (%d) for %s.\n",
		       cap.rel_offset, from_obj->obj, cap.owner_spd_handle, server_fn);
		exit(-1);
	}

	return 0;
}

static struct symb *spd_contains_symb(struct service_symbs *s, char *name)
{
	int i, client_stub;
	struct symb_type *symbs = &s->exported;

	client_stub = strstr(name, CAP_CLIENT_STUB_POSTPEND) != NULL;
	for (i = 0 ; i < symbs->num_symbs ; i++) {
		char *n = symbs->symbs[i].name;
		if (strcmp(name, n) == 0) {
			return &symbs->symbs[i];
		}
		if (client_stub && strstr(n, CAP_CLIENT_STUB_POSTPEND)) {
			int j = 0;

			for (j = 0 ; j < s->num_dependencies ; j++) {
				struct dependency *d = &s->dependencies[j];

				if (!d->modifier || strncmp(d->modifier, name, d->mod_len)) continue;
				if (!strcmp(name+d->mod_len, n)) {
					printl(PRINT_DEBUG,
					       "Found client stub with dependency modification: %s->%s in %s\n",
					       name, n, s->obj);
					return &symbs->symbs[i];
				}
			}
		}
	}
	return NULL;
}

struct cap_ret_info {
	struct symb *csymb, *ssymbfn, *cstub, *sstub;
	struct service_symbs *serv;
	u32_t fault_handler;
};

static int cap_get_info(struct service_symbs *service, struct cap_ret_info *cri, struct symb *symb)
{
	struct symb *exp_symb = symb->exported_symb;
	struct service_symbs *exporter = symb->exporter;
	struct symb *c_stub, *s_stub;
	char tmp[MAX_SYMB_LEN];

	assert(exporter);
	memset(cri, 0, sizeof(struct cap_ret_info));

	if (MAX_SYMB_LEN-1 == snprintf(tmp, MAX_SYMB_LEN-1, "%s%s", symb->name, CAP_CLIENT_STUB_POSTPEND)) {
		printl(PRINT_HIGH, "symbol name %s too long to become client capability\n", symb->name);
		return -1;
	}
	/* gap */
	/* if (s->modifier &&  */
	/*     !strncmp(n, s->modifier, s->mod_len) &&  */
	/*     !strcmp(n + s->mod_len, name)) { */
	/* 	return &symbs->symbs[i]; */
	/* } */

	c_stub = spd_contains_symb(service, tmp);
	if (NULL == c_stub) {
		c_stub = spd_contains_symb(service, CAP_CLIENT_STUB_DEFAULT);
		if (NULL == c_stub) {
			printl(PRINT_HIGH, "Could not find a client stub for function %s in service %s.\n",
			       symb->name, service->obj);
			return -1;
		}
	}

	if (MAX_SYMB_LEN-1 == snprintf(tmp, MAX_SYMB_LEN-1, "%s%s", exp_symb->name, CAP_SERVER_STUB_POSTPEND)) {
		printl(PRINT_HIGH, "symbol name %s too long to become server capability\n", exp_symb->name);
		return -1;
	}

	s_stub = spd_contains_symb(exporter, tmp);
	if (NULL == s_stub) {
		printl(PRINT_HIGH, "Could not find server stub (%s) for function %s in service %s to satisfy %s.\n",
		       tmp, exp_symb->name, exporter->obj, service->obj);
		return -1;
	}

//	printf("spd %s: symb %s, exp %s\n", exporter->obj, symb->name, exp_symb->name);

	cri->csymb = symb;
	cri->ssymbfn = exp_symb;
	cri->cstub = c_stub;
	cri->sstub = s_stub;
	cri->serv = exporter;
	if (exp_symb->modifier_offset) {
		printf("%d: %s\n", service_get_spdid(exporter),
		       exp_symb->name + exp_symb->modifier_offset);
	}
	cri->fault_handler = (u32_t)fault_handler_num(exp_symb->name + exp_symb->modifier_offset);

	return 0;
}

static int create_spd_capabilities(struct service_symbs *service/*, struct spd_info *si*/, int cntl_fd)
{
	int i;
	struct symb_type *undef_symbs = &service->undef;
	struct spd_info *spd = (struct spd_info*)service->extern_info;

	assert(!is_loaded_by_llboot(service));
	for (i = 0 ; i < undef_symbs->num_symbs ; i++) {
		struct symb *symb = &undef_symbs->symbs[i];
		struct cap_ret_info cri;

		if (cap_get_info(service, &cri, symb)) return -1;
		assert(!is_loaded_by_llboot(cri.serv));
		if (create_invocation_cap(spd, service, cri.serv->extern_info, cri.serv, cntl_fd,
					  cri.csymb->name, cri.cstub->name, cri.sstub->name,
					  cri.ssymbfn->name, 0)) {
			return -1;
		}
	}

	return 0;
}

struct spd_info *create_spd(int cos_fd, struct service_symbs *s,
			    long lowest_addr, long size)
{
	struct spd_info *spd;
	struct usr_inv_cap *ucap_tbl;
	vaddr_t upcall_addr;
	long *spd_id_addr, *heap_ptr;
	struct cos_component_information *ci;
	int i;

	assert(!is_loaded_by_llboot(s));
	spd = (struct spd_info *)malloc(sizeof(struct spd_info));
	if (NULL == spd) {
		perror("Could not allocate memory for spd\n");
		return NULL;
	}

	ci = (void*)get_symb_address(&s->exported, COMP_INFO);
	if (ci == NULL) {
		printl(PRINT_DEBUG, "Could not find %s in %s.\n", COMP_INFO, s->obj);
		return NULL;
	}
	upcall_addr = ci->cos_upcall_entry;
	spd_id_addr = (long*)&ci->cos_this_spd_id;
	heap_ptr    = (long*)&ci->cos_heap_ptr;
	ucap_tbl    = (struct usr_inv_cap*)ci->cos_user_caps;

	for (i = 0 ; i < NUM_ATOMIC_SYMBS ; i++) {
		if (i % 2 == 0) {
			spd->atomic_regions[i] = ci->cos_ras[i/2].start;
		} else {
			spd->atomic_regions[i] = ci->cos_ras[i/2].end;
		}
	}

	spd->num_caps = s->undef.num_symbs;
	spd->ucap_tbl = (vaddr_t)ucap_tbl;
	spd->lowest_addr = lowest_addr;
	spd->size = size;
	spd->upcall_entry = upcall_addr;

	spdid_inc++;
	spd->spd_handle = cos_create_spd(cos_fd, spd);
	assert(spdid_inc == spd->spd_handle);
	if (spd->spd_handle < 0) {
		printl(PRINT_DEBUG, "Could not create spd %s\n", s->obj);
		free(spd);
		return NULL;
	}
	printl(PRINT_HIGH, "spd %s, id %d with initialization string \"%s\" @ %x.\n",
	       s->obj, (unsigned int)spd->spd_handle, s->init_str, (unsigned int)spd->lowest_addr);
	*spd_id_addr = spd->spd_handle;
	printl(PRINT_DEBUG, "\tHeap pointer directed to %x.\n", (unsigned int)s->heap_top);
	*heap_ptr = s->heap_top;

	printl(PRINT_DEBUG, "\tFound ucap_tbl for component %s @ %p.\n", s->obj, ucap_tbl);
	printl(PRINT_DEBUG, "\tFound cos_upcall for component %s @ %p.\n", s->obj, (void*)upcall_addr);
	printl(PRINT_DEBUG, "\tFound spd_id address for component %s @ %p.\n", s->obj, spd_id_addr);
	for (i = 0 ; i < NUM_ATOMIC_SYMBS ; i++) {
		printl(PRINT_DEBUG, "\tFound %s address for component %s @ %x.\n",
		       ATOMIC_USER_DEF[i], s->obj, (unsigned int)spd->atomic_regions[i]);
	}

	s->extern_info = spd;

	return spd;
}

void make_spd_scheduler(int cntl_fd, struct service_symbs *s, struct service_symbs *p)
{
	vaddr_t sched_page;
	struct spd_info *spd = s->extern_info, *parent = NULL;
//	struct cos_component_information *ci;
	struct cos_sched_data_area *sched_page_ptr;

	if (p) parent = p->extern_info;

	sched_page_ptr = (struct cos_sched_data_area*)get_symb_address(&s->exported, SCHED_NOTIF);
	sched_page = (vaddr_t)sched_page_ptr;

	printl(PRINT_DEBUG, "Found spd notification page @ %x.  Promoting to scheduler.\n",
	       (unsigned int) sched_page);

	cos_promote_to_scheduler(cntl_fd, spd->spd_handle, (NULL == parent)? -1 : parent->spd_handle, sched_page);

	return;
}

/* Edge description of components.  Mirrored in mpd_mgr.c */
struct comp_graph {
	short int client, server;
};

static int service_get_spdid(struct service_symbs *ss)
{
	if (is_loaded_by_llboot(ss)) {
		return (int)ss->cobj->id;
	} else {
		assert(ss->extern_info);
		return ((struct spd_info*)ss->extern_info)->spd_handle;
	}
}

static int serialize_spd_graph(struct comp_graph *g, int sz, struct service_symbs *ss)
{
	struct comp_graph *edge;
	int g_frontier = 0;

	while (ss) {
		int i, cid, sid;

		if (is_loaded_by_llboot(ss)) {
			ss = ss->next;
			continue;
		}

		assert(ss->extern_info);
		cid = service_get_spdid(ss);
		for (i = 0 ; i < ss->num_dependencies && 0 != cid ; i++) {
			struct service_symbs *dep = ss->dependencies[i].dep;
			assert(dep);

			sid = service_get_spdid(dep);
			if (sid == 0) continue;
			if (g_frontier >= (sz-2)) {
				printl(PRINT_DEBUG, "More edges in component graph than can be serialized into the allocated region: fix cos_loader.c.\n");
				exit(-1);
			}

			edge = &g[g_frontier++];
			edge->client = (short int)cid;
			edge->server = (short int)sid;
			//printl(PRINT_DEBUG, "serialized edge @ %p: %d->%d.\n", edge, cid, sid);
		}

		ss = ss->next;
	}
	edge = &g[g_frontier];
	edge->client = edge->server = 0;

	return 0;
}

int **get_heap_ptr(struct service_symbs *ss)
{
	struct cos_component_information *ci;

	ci = (struct cos_component_information *)get_symb_address(&ss->exported, COMP_INFO);
	if (ci == NULL) {
		printl(PRINT_DEBUG, "Could not find component information struct in %s.\n", ss->obj);
		exit(-1);
	}
	return (int**)&(ci->cos_heap_ptr);
}

/*
 * The only thing we need to do to the mpd manager is to let it know
 * the topology of the component graph.  Progress the heap pointer a
 * page, and serialize the component graph into that page.
 */
static void make_spd_mpd_mgr(struct service_symbs *mm, struct service_symbs *all)
{
	int **heap_ptr, *heap_ptr_val;
	struct comp_graph *g;

	if (is_loaded_by_llboot(mm)) {
		printl(PRINT_HIGH, "Cannot load %s via composite (%s).\n", MPD_MGR, BOOT_COMP);
		return;
	}
	heap_ptr = get_heap_ptr(mm);
	if (heap_ptr == NULL) {
		printl(PRINT_HIGH, "Could not find heap pointer in %s.\n", mm->obj);
		return;
	}
	heap_ptr_val = *heap_ptr;
	g = mmap((void*)heap_ptr_val, PAGE_SIZE, PROT_WRITE | PROT_READ,
			MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (MAP_FAILED == g){
		perror("Couldn't map the graph into the address space");
		return;
	}
	printl(PRINT_DEBUG, "Found mpd_mgr: remapping heap_ptr from %p to %p, serializing graph.\n",
	       *heap_ptr, heap_ptr_val + PAGE_SIZE/sizeof(heap_ptr_val));
	*heap_ptr = heap_ptr_val + PAGE_SIZE/sizeof(heap_ptr_val);

	serialize_spd_graph(g, PAGE_SIZE/sizeof(struct comp_graph), all);
}

static void make_spd_init_file(struct service_symbs *ic, const char *fname)
{
	int fd = open(fname, O_RDWR);
	struct stat b;
	int real_sz, sz, ret;
	int **heap_ptr, *heap_ptr_val;
	int *start;
	struct cos_component_information *ci;

	if (fd == -1) {
		printl(PRINT_HIGH, "Init file component specified, but file %s not found\n", fname);
		perror("Error");
		exit(-1);
	}
	if (fstat(fd, &b)) {
		printl(PRINT_HIGH, "Init file component specified, but error stating file %s not found\n", fname);
		perror("Error");
		exit(-1);
	}
	real_sz = b.st_size;
	sz = round_up_to_page(real_sz);

	if (is_loaded_by_llboot(ic)) {
		printl(PRINT_HIGH, "Cannot load %s via composite (%s).\n", INIT_FILE, BOOT_COMP);
		return;
	}
	heap_ptr = get_heap_ptr(ic);
	if (heap_ptr == NULL) {
		printl(PRINT_HIGH, "Could not find heap pointer in %s.\n", ic->obj);
		return;
	}
	heap_ptr_val = *heap_ptr;
	ci = (void *)get_symb_address(&ic->exported, COMP_INFO);
	if (!ci) {
		printl(PRINT_HIGH, "Could not find component information in %s.\n", ic->obj);
		return;
	}
	ci->cos_poly[0] = (vaddr_t)heap_ptr_val;

	start = mmap((void*)heap_ptr_val, sz, PROT_WRITE | PROT_READ,
		     MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	ret = read(fd, start, real_sz);
	if (real_sz != ret) {
		printl(PRINT_HIGH, "Reading in init file %s: could not retrieve whole file\n", fname);
		perror("error: ");
		exit(-1);
	}
	if (MAP_FAILED == start){
		printl(PRINT_HIGH, "Couldn't map the init file, %s, into address space", fname);
		perror("error:");
		exit(-1);
	}
	printl(PRINT_HIGH, "Found init file component: remapping heap_ptr from %p to %p, mapping in file.\n",
	       *heap_ptr, (char*)heap_ptr_val + sz);
	*heap_ptr = (int*)((char*)heap_ptr_val + sz);
	ci->cos_poly[1] = real_sz;
}

static int make_cobj_symbols(struct service_symbs *s, struct cobj_header *h)
{
	u32_t addr;
	u32_t symb_offset = 0;
	int i;

	struct name_type_map {
		const char *name;
		u32_t type;
	};
	struct name_type_map map[] = {
		{.name = COMP_INFO, .type = COBJ_SYMB_COMP_INFO},
		{.name = NULL, .type = 0}
	};

	/* Create the sumbols */
	printl(PRINT_DEBUG, "%s loaded by Composite -- Symbols:\n", s->obj);
	for (i = 0 ; map[i].name != NULL ; i++) {
		addr = (u32_t)get_symb_address(&s->exported, map[i].name);
		printl(PRINT_DEBUG, "\taddr %x, nsymb %d\n", addr, i);
		if (addr && cobj_symb_init(h, symb_offset++, map[i].type, addr)) {
			printl(PRINT_HIGH, "boot component: couldn't create cobj symb for %s (%d).\n", map[i].name, i);
			return -1;
		}
	}

	return 0;
}

static int make_cobj_caps(struct service_symbs *s, struct cobj_header *h)
{
	int i;
	struct symb_type *undef_symbs = &s->undef;

	printl(PRINT_DEBUG, "%s loaded by Composite -- Capabilities:\n", s->obj);
	for (i = 0 ; i < undef_symbs->num_symbs ; i++) {
		u32_t cap_off, dest_id, sfn, cstub, sstub, fault;
		struct symb *symb = &undef_symbs->symbs[i];
		struct cap_ret_info cri;

		if (cap_get_info(s, &cri, symb)) return -1;

		cap_off = i;
		dest_id = service_get_spdid(cri.serv);
		sfn     = cri.ssymbfn->addr;
		cstub   = cri.cstub->addr;
		sstub   = cri.sstub->addr;
		fault   = cri.fault_handler;

		printl(PRINT_DEBUG, "\tcap %d, off %d, sfn %x, cstub %x, sstub %x\n",
		       i, cap_off, sfn, cstub, sstub);

		if (cobj_cap_init(h, cap_off, cap_off, dest_id, sfn, cstub, sstub, fault)) return -1;

		printl(PRINT_DEBUG, "capability from %s:%d to %s:%d\n", s->obj, s->cobj->id, cri.serv->obj, dest_id);
	}

	return 0;
}

static struct service_symbs *find_obj_by_name(struct service_symbs *s, const char *n);

//#define ROUND_UP_TO_PAGE(a) (((vaddr_t)(a)+PAGE_SIZE-1) & ~(PAGE_SIZE-1))
//#deinfe ROUND_UP_TO_CACHELINE(a) (((vaddr_t)(a)+CACHE_LINE-1) & ~(CACHE_LINE-1))
static void make_spd_config_comp(struct service_symbs *c, struct service_symbs *all);

static int
spd_already_loaded(struct service_symbs *c, int layer)
{
	return c->already_loaded || !is_loaded_by_boot_layer(c, layer);
}

static void
make_spd_boot_schedule(struct service_symbs *comp, struct service_symbs **sched,
		       unsigned int *off, int boot_layer)
{
	int i;

	if (spd_already_loaded(comp, boot_layer)) return;

	for (i = 0 ; i < comp->num_dependencies ; i++) {
		struct dependency *d = &comp->dependencies[i];

		if (!spd_already_loaded(d->dep, boot_layer)) {
			make_spd_boot_schedule(d->dep, sched, off, boot_layer);
		}
		assert(spd_already_loaded(d->dep, boot_layer));
	}
	sched[*off] = comp;
	comp->already_loaded = 1;
	(*off)++;
	printl(PRINT_HIGH, "\t%d: %s\n", *off, comp->obj);
}

//#define INIT_STR_SZ 116
#define INIT_STR_SZ 52
/* struct is 64 bytes, so we can have 64 entries in a page. */
struct component_init_str {
	unsigned int spdid, schedid;
	int startup;
	char init_str[INIT_STR_SZ];
}__attribute__((packed));
static void format_config_info(struct service_symbs *ss, struct component_init_str *data);

static void
make_spd_boot(struct service_symbs *boot, struct service_symbs *all, int layer)
{
	int n = 0, cnt = 0, tot_sz = 0;
	unsigned int off = 0, i;
	struct cobj_header *h, *new_h;
	char *new_end, *new_sect_start;
	u32_t new_vaddr_start;
	u32_t all_obj_sz;
	struct cos_component_information *ci;
	struct service_symbs *first = all;
	/* array to hold the order of initialization/schedule */
	struct service_symbs **schedule;

	if (layer == 1 && service_get_spdid(boot) != LLBOOT_BOOT) {
		printf("Booter component must be component number %d, is %d.\n"
		       "\tSuggested fix: Your first four components should be e.g. "
		       "c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\n", LLBOOT_BOOT, service_get_spdid(boot));
		exit(-1);
	}

	assert(is_loaded_by_llboot(boot));
	assert(is_loaded_by_boot_layer(boot, layer-1));
	assert(boot->cobj->nsect == MAXSEC_S); /* extra section for other components */
	/* Assign ids to the booter-loaded components. */
	for (all = first ; NULL != all ; all = all->next) {
		if (!is_loaded_by_boot_layer(all, layer)) continue;

		h = all->cobj;
		assert(h);
		cnt++;
		tot_sz += h->size;
	}

	schedule = malloc(sizeof(struct service_symbs *) * cnt);
	assert(schedule);
	printl(PRINT_HIGH, "Loaded component's initialization scheduled in the following order:\n");
	for (all = first ; NULL != all ; all = all->next) {
		if (!is_loaded_by_boot_layer(all, layer)) continue;
		make_spd_boot_schedule(all, schedule, &off, layer);
	}

	/* Setup the capabilities for each of the booter-loaded
	 * components */
	for (all = first ; NULL != all ; all = all->next) {
		if (!is_loaded_by_boot_layer(all, layer)) continue;

		if (make_cobj_caps(all, all->cobj)) {
			printl(PRINT_HIGH, "Could not create capabilities in cobj for %s\n", all->obj);
			exit(-1);
		}
	}

	all_obj_sz = 0;
	/* Find the cobj's size */
	for (all = first ; NULL != all ; all = all->next) {
		struct cobj_header *h;

		if (!is_loaded_by_boot_layer(all, layer)) continue;
		printl(PRINT_HIGH, "booter found %s:%d with len %d\n",
		       all->obj, service_get_spdid(all), all->cobj->size)
		n++;

		assert(is_loaded_by_boot(all));
		h = all->cobj;
		assert(h);
		all_obj_sz += round_up_to_cacheline(h->size);
	}
	all_obj_sz  = all_obj_sz;
	all_obj_sz += 3 * PAGE_SIZE; // schedule, config info, and edge info
	h           = boot->cobj;
	assert(h->nsect == MAXSEC_S);
	new_h       = malloc(h->size + all_obj_sz);
	assert(new_h);
	memcpy(new_h, h, h->size);
	printl(PRINT_DEBUG, "booter realloc %p -> %p with size %d\n", h, new_h, h->size)

	/* Initialize the new section */
	{
		struct cobj_sect *s_prev;

		new_h->size += all_obj_sz;

		s_prev = cobj_sect_get(new_h, INITFILE_S);

		cobj_sect_init(new_h, INITFILE_S, csg(INITFILE_S)->cobj_flags,
			       round_up_to_page(s_prev->vaddr + s_prev->bytes),
			       all_obj_sz);
	}
	new_sect_start  = new_end = cobj_sect_contents(new_h, INITFILE_S);
	new_vaddr_start = cobj_sect_get(new_h, INITFILE_S)->vaddr;
#define ADDR2VADDR(a) ((a-new_sect_start)+new_vaddr_start)

	ci = (void *)cobj_vaddr_get(new_h, (u32_t)get_symb_address(&boot->exported, COMP_INFO));
	assert(ci);
	printl(PRINT_DEBUG, "booter cos_comp_info @ %p\n", ci);
	ci->cos_poly[0] = ADDR2VADDR(new_sect_start);

	/* copy the cobjs */
	for (all = first ; NULL != all ; all = all->next) {
		struct cobj_header *h;

		if (!is_loaded_by_boot_layer(all, layer)) continue;
		h = all->cobj;
		assert(h);
		memcpy(new_end, h, h->size);
		new_end += round_up_to_cacheline(h->size);
 	}
	assert((u32_t)(new_end - new_sect_start) + 3*PAGE_SIZE ==
	       cobj_sect_get(new_h, INITFILE_S)->bytes);

	all = first;
	ci->cos_poly[1] = (vaddr_t)n;

	ci->cos_poly[2] = ADDR2VADDR(new_end);
	serialize_spd_graph((struct comp_graph*)new_end, PAGE_SIZE/sizeof(struct comp_graph), all);

	new_end += PAGE_SIZE;
	ci->cos_poly[3] = ADDR2VADDR(new_end);
	format_config_info(all, (struct component_init_str*)new_end);

	assert(off < PAGE_SIZE/sizeof(unsigned int)); /* schedule must fit into page. */
	new_end += PAGE_SIZE;
	ci->cos_poly[4] = ADDR2VADDR(new_end);
	for (i = 0 ; i < off ; i++) {
		((int *)new_end)[i] = service_get_spdid(schedule[i]);
	}
	((int *)new_end)[off] = 0;

	new_end += PAGE_SIZE;
	ci->cos_heap_ptr = round_up_to_page(ADDR2VADDR(new_end));

	boot->cobj = new_h;

	printl(PRINT_HIGH, "boot component %s:%d has new section @ %x:%x at address %x, \n\t"
	       "with n %d, graph @ %x, config info @ %x, schedule %x, and heap %x\n",
	       boot->obj, service_get_spdid(boot), (unsigned int)cobj_sect_get(new_h, 3)->vaddr,
	       (int)cobj_sect_get(new_h, 3)->bytes, (unsigned int)ci->cos_poly[0], (unsigned int)ci->cos_poly[1],
	       (unsigned int)ci->cos_poly[2], (unsigned int)ci->cos_poly[3], (unsigned int)ci->cos_poly[4], (unsigned int)ci->cos_heap_ptr);
}

static void make_spd_boot_recursive(struct service_symbs *s,
		struct service_symbs *services,
		int boot_layer)
{
	struct service_symbs *b = s;
	int boot_cnt = 0;
	if (!boot_layer) return;
	do {
		if (strstr(s->obj, "boot")) {
			++boot_cnt;
			if (boot_layer == boot_cnt)
				make_spd_boot(s, services, boot_layer);
		}
	} while ((s = s->next));
	make_spd_boot_recursive(b, services, boot_layer - 1);
}

static void
spd_assign_ids(struct service_symbs *all)
{
	struct cobj_header *h;

	/* Assign ids to the booter-loaded components. */
	for (; NULL != all ; all = all->next) {
		if (!is_loaded_by_llboot(all)) continue;

		h = all->cobj;
		assert(h);
		spdid_inc++;
		h->id = spdid_inc;
	}
}

static void
make_spd_llboot(struct service_symbs *boot, struct service_symbs *all)
{
	volatile int **heap_ptr;
	int *heap_ptr_val, n = 0;
	struct cobj_header *h;
	char *mem;
	u32_t obj_size;
	struct cos_component_information *ci;
	struct service_symbs *first = all;

	if (service_get_spdid(boot) != LLBOOT_COMPN) {
		printf("Low-Level Booter component must be component number %d, but is %d instead.\n"
		       "\tSuggested fix: Your first four components should be e.g. "
		       "c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;boot.o, ;\n",
		       LLBOOT_COMPN, service_get_spdid(boot));
		exit(-1);
	}

	/* Setup the capabilities for each of the booter-loaded
	 * components */
	all = first;
	for (all = first ; NULL != all ; all = all->next) {
		if (!is_loaded_by_llboot(all)) continue;

		if (make_cobj_caps(all, all->cobj)) {
			printl(PRINT_HIGH, "Could not create capabilities in cobj for %s\n", all->obj);
			exit(-1);
		}
	}

	heap_ptr = (volatile int **)get_heap_ptr(boot);
	ci = (void *)get_symb_address(&boot->exported, COMP_INFO);
	ci->cos_poly[0] = (vaddr_t)*heap_ptr;

	for (all = first ; NULL != all ; all = all->next) {
		vaddr_t map_addr;
		int map_sz;

		if (!is_loaded_by_llboot(all) || is_loaded_by_boot(all)) continue;
		n++;

		heap_ptr_val = (int*)*heap_ptr;
		assert(is_loaded_by_llboot(all));
		h = all->cobj;
		assert(h);

		obj_size = round_up_to_cacheline(h->size);
		map_addr = round_up_to_page(heap_ptr_val);
		map_sz = (int)obj_size - (int)(map_addr-(vaddr_t)heap_ptr_val);
		if (map_sz > 0) {
			mem = mmap((void*)map_addr, map_sz, PROT_WRITE | PROT_READ,
				   MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
			if (MAP_FAILED == mem) {
				perror("Couldn't map test component into the boot component");
				exit(-1);
			}
		}
		printl(PRINT_HIGH, "boot component: placing %s:%d @ %p, copied from %p:%d\n",
		       all->obj, service_get_spdid(all), heap_ptr_val, h, obj_size);
		memcpy(heap_ptr_val, h, h->size);
		*heap_ptr = (void*)(((int)heap_ptr_val) + obj_size);
	}
	all = first;
	*heap_ptr = (int*)(round_up_to_page((int)*heap_ptr));
	ci->cos_poly[1] = (vaddr_t)n;

	ci->cos_poly[2] = ((unsigned int)*heap_ptr);
	make_spd_mpd_mgr(boot, all);

	ci->cos_poly[3] = ((unsigned int)*heap_ptr);
	make_spd_config_comp(boot, all);
	llboot_mem = (unsigned int)*heap_ptr - boot->lower_addr;
}

static void format_config_info(struct service_symbs *ss, struct component_init_str *data)
{
	int i;

	for (i = 0 ; ss ; i++, ss = ss->next) {
		char *info;

		info = ss->init_str;
		if (strlen(info) >= INIT_STR_SZ) {
			printl(PRINT_HIGH, "Initialization string %s for component %s is too long (longer than %d)",
			       info, ss->obj, strlen(info));
			exit(-1);
		}

		if (is_loaded_by_llboot(ss)) {
			data[i].startup = 0;
			data[i].spdid = ss->cobj->id;
		} else {
			data[i].startup = 1;
			data[i].spdid = ((struct spd_info *)(ss->extern_info))->spd_handle;
		}

		if (ss->scheduler) {
			data[i].schedid = service_get_spdid(ss->scheduler);
		} else {
			data[i].schedid = 0;
		}

		if (0 == strcmp(" ", info)) info = "";
		strcpy(data[i].init_str, info);
	}
	data[i].spdid = 0;
}

static void make_spd_config_comp(struct service_symbs *c, struct service_symbs *all)
{
	int **heap_ptr, *heap_ptr_val;
	struct component_init_str *info;

	heap_ptr = get_heap_ptr(c);
	if (heap_ptr == NULL) {
		printl(PRINT_DEBUG, "Could not find cos_heap_ptr in %s.\n", c->obj);
		return;
	}
	heap_ptr_val = *heap_ptr;
	info = mmap((void*)heap_ptr_val, PAGE_SIZE, PROT_WRITE | PROT_READ,
			MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
	if (MAP_FAILED == info){
		perror("Couldn't map the configuration info into the address space");
		return;
	}
	printl(PRINT_DEBUG, "Found %s: remapping heap_ptr from %p to %p, writing config info.\n",
	       CONFIG_COMP, *heap_ptr, heap_ptr_val + PAGE_SIZE/sizeof(heap_ptr_val));
	*heap_ptr = heap_ptr_val + PAGE_SIZE/sizeof(heap_ptr_val);

	format_config_info(all, info);
}

static struct service_symbs *find_obj_by_name(struct service_symbs *s, const char *n)
{
	while (s) {
		if (!strncmp(&s->obj[5], n, strlen(n))) {
			return s;
		}

		s = s->next;
	}

	return NULL;
}

void set_prio(void);

#define MAX_SCHEDULERS 3

void set_curr_affinity(u32_t cpu)
{
	int ret;
	cpu_set_t s;
	CPU_ZERO(&s);
	assert(cpu <= NUM_CPU - 1);
	CPU_SET(cpu, &s);
	ret = sched_setaffinity(0, sizeof(cpu_set_t), &s);
	assert(ret == 0);

	return;
}

volatile int var;
int (*fn)(void);

static void setup_kernel(struct service_symbs *services)
{
	struct service_symbs /* *m, */ *s;
	struct service_symbs *init = NULL;
	struct spd_info *init_spd = NULL, *llboot_spd;
	struct cos_thread_info thd;

	pid_t pid;
	pid_t children[NUM_CPU];
	int cntl_fd = 0, i, cpuid, ret;
	unsigned long long start, end;

	set_curr_affinity(0);

	cntl_fd = aed_open_cntl_fd();

	s = services;
	while (s) {
		struct service_symbs *t;
		struct spd_info *t_spd;

		t = s;
		if (!is_loaded_by_llboot(s)) {
			if (strstr(s->obj, INIT_COMP) != NULL) {
				init = t;
				t_spd = init_spd = create_spd(cntl_fd, init, 0, 0);
			} else {
				t_spd = create_spd(cntl_fd, t, t->lower_addr, t->size);
			}
			if (strstr(s->obj, LLBOOT_COMP)) {
				llboot_spd = t_spd;
			}

			if (!t_spd) {
				fprintf(stderr, "\tCould not find service object.\n");
				exit(-1);
			}
		}
		s = s->next;
	}

	s = services;
	while (s) {
		if (!is_loaded_by_llboot(s)) {
			if (create_spd_capabilities(s, cntl_fd)) {
				fprintf(stderr, "\tCould not find all stubs.\n");
				exit(-1);
			}

		}

		s = s->next;
	}
	printl(PRINT_DEBUG, "\n");

	spd_assign_ids(services);

	if ((s = find_obj_by_name(services, BOOT_COMP))) {
		struct service_symbs *b = s;
		int boot_layers = 1;
		while ((s = s->next)) if (strstr(s->obj, "boot")) boot_layers++;
		make_spd_boot_recursive(b, services, boot_layers);
	}

	fflush(stdout);

	if ((s = find_obj_by_name(services, LLBOOT_COMP))) {
		make_spd_llboot(s, services);
		make_spd_scheduler(cntl_fd, s, NULL);
	}

	fflush(stdout);
	thd.sched_handle = ((struct spd_info *)s->extern_info)->spd_handle;

	if ((s = find_obj_by_name(services, INIT_COMP)) == NULL) {
		fprintf(stderr, "Could not find initial component\n");
		exit(-1);
	}
	thd.spd_handle = ((struct spd_info *)s->extern_info)->spd_handle;//spd0->spd_handle;
	var = *((int *)SERVICE_START);

	/* This will hopefully avoid hugely annoying fsck runs */
	sync();

	/* Access comp0 to make sure it is present in the page tables */
	fn = (int (*)(void))get_symb_address(&s->exported, "spd0_main");

	ret = cos_create_thd(cntl_fd, &thd);
	assert(ret == 0);

	assert(llboot_mem);
	llboot_spd->mem_size = llboot_mem;
	if (cos_init_booter(cntl_fd, llboot_spd)) {
		printf("Boot component init failed!\n");
		exit(-1);
	}
	if  (cos_create_init_thd(cntl_fd)) {
		printf("Creating init threads failed!\n");
		exit(-1);
	}

	assert(fn);
	/* We call fn to init the low level booter first! Init
	 * function will return to here and create processes for other
	 * cores. */
	fn();

	pid = getpid();
	for (i = 1; i < NUM_CPU_COS; i++) {
		printf("Parent(pid %d): forking for core %d.\n", getpid(), i);
		cpuid = i;
		pid = fork();
		children[i] = pid;
		if (pid == 0) break;
		printf("Created pid %d for core %d.\n", pid, i);
	}

	if (pid) {
                /* Parent process should give other processes a chance
		 * to run. They need to migrate to their cores. */
		sleep(1);
	} else { /* child process: set own affinity first */
		set_curr_affinity(cpuid);
#ifdef HIGHEST_PRIO
		set_prio();
#endif
		sleep(1);
		ret = cos_create_thd(cntl_fd, &thd);
		assert(ret == 0);
		ret = cos_create_init_thd(cntl_fd);
		assert(ret == 0);
	}

	printl(PRINT_HIGH, "\n Pid %d: OK, good to go, calling component 0's main\n\n", getpid());
	fflush(stdout);

	aed_disable_syscalls(cntl_fd);

#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))
	rdtscll(start);
	ret = fn();
	rdtscll(end);

	aed_enable_syscalls(cntl_fd);

	cos_restore_hw_entry(cntl_fd);

	if (pid > 0 && NUM_CPU_COS > 1) {
		int child_status;
		while (wait(&child_status) > 0) ;
	} else {
		exit(getpid());
	}

	close(cntl_fd);

	return;
}

static inline void print_usage(int argc, char **argv)
{
	char *prog_name = argv[0];
	int i;

	printl(PRINT_HIGH, "Usage: %s <comma separated string of all "
	       "objs:truster1-trustee1|trustee2|...;truster2-...> "
	       "<path to gen_client_stub>\n",
	       prog_name);

	printl(PRINT_HIGH, "\nYou gave:");
	for (i = 0 ; i < argc ; i++) {
		printl(PRINT_HIGH, " %s", argv[i]);
	}
	printl(PRINT_HIGH, "\n");

	return;
}

#define STUB_PROG_LEN 128
extern vaddr_t SS_ipc_client_marshal;
extern vaddr_t DS_ipc_client_marshal;

//#define FAULT_SIGNAL
#include <sys/ucontext.h>
#ifdef FAULT_SIGNAL
void segv_handler(int signo, siginfo_t *si, void *context) {
	ucontext_t *uc = context;
	struct sigcontext *sc = (struct sigcontext *)&uc->uc_mcontext;

	printl(PRINT_HIGH, "Segfault: Faulting address %p, ip: %lx\n", si->si_addr, sc->eip);
	exit(-1);
}
#endif
//#define ALRM_SIGNAL
#ifdef ALRM_SIGNAL
void alrm_handler(int signo, siginfo_t *si, void *context) {
	printl(PRINT_HIGH, "Alarm! Time to exit!\n");
	exit(-1);
}
#endif


#include <sched.h>
#include <sys/time.h>
#include <sys/resource.h>
static void call_getrlimit(int id, char *name)
{
	struct rlimit rl;

	if (getrlimit(id, &rl)) {
		perror("getrlimit: "); printl(PRINT_HIGH, "\n");
		exit(-1);
	}
	/* printl(PRINT_HIGH, "rlimit for %s is %d:%d (inf %d)\n",  */
	/*        name, (int)rl.rlim_cur, (int)rl.rlim_max, (int)RLIM_INFINITY); */
}

static void call_setrlimit(int id, rlim_t c, rlim_t m)
{
	struct rlimit rl;

	rl.rlim_cur = c;
	rl.rlim_max = m;
	if (setrlimit(id, &rl)) {
		perror("getrlimit: "); printl(PRINT_HIGH, "\n");
		exit(-1);
	}
}

void set_prio(void)
{
	struct sched_param sp;

	call_getrlimit(RLIMIT_CPU, "CPU");
#ifdef RLIMIT_RTTIME
	call_getrlimit(RLIMIT_RTTIME, "RTTIME");
#endif
	call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
	call_setrlimit(RLIMIT_RTPRIO, RLIM_INFINITY, RLIM_INFINITY);
	call_getrlimit(RLIMIT_RTPRIO, "RTPRIO");
	call_getrlimit(RLIMIT_NICE, "NICE");

	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
		printl(PRINT_HIGH, "\n");
	}
	sp.sched_priority = sched_get_priority_max(SCHED_RR);
	if (sched_setscheduler(0, SCHED_RR, &sp) < 0) {
		perror("setscheduler: "); printl(PRINT_HIGH, "\n");
		exit(-1);
	}
	if (sched_getparam(0, &sp) < 0) {
		perror("getparam: ");
		printl(PRINT_HIGH, "\n");
	}
	assert(sp.sched_priority == sched_get_priority_max(SCHED_RR));

	return;
}

void set_smp_affinity()
{
	char cmd[64];
	/* everything done is the python script. */
	sprintf(cmd, "python set_smp_affinity.py %d %d", NUM_CPU, getpid());
	system(cmd);
}

void setup_thread(void)
{
#ifdef FAULT_SIGNAL
	struct sigaction sa;

	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &sa, NULL);
#endif

	set_smp_affinity();

#ifdef HIGHEST_PRIO
	set_prio();
#endif
#ifdef ALRM_SIGNAL
	//printf("pid %d\n", getpid()); getchar();
	{
		struct sigaction saa;

		saa.sa_sigaction = alrm_handler;
		saa.sa_flags = SA_SIGINFO;
		sigaction(SIGALRM, &saa, NULL);
		alarm(30);
	}
	while (1) ;
#endif

}

/*
 * Format of the input string is as such:
 *
 * "s1,s2,s3,...,sn:s2-s3|...|sm;s3-si|...|sj"
 *
 * Where the pre-: comma-separated list is simply a list of all
 * involved services.  Post-: is a list of truster (before -) ->
 * trustee (all trustees separated by '|'s) relations separated by
 * ';'s.
 */
int main(int argc, char *argv[])
{
	struct service_symbs *services;
	char *delim = ":";
	char *servs, *dependencies, *ndeps, *stub_gen_prog;
	int ret = -1;

	setup_thread();

	printl(PRINT_DEBUG, "Thread scheduling parameters setup\n");

	if (argc != 3) {
		print_usage(argc, argv);
		goto exit;
	}

	stub_gen_prog = argv[2];

	/*
	 * NOTE: because strtok is used in prepare_service_symbs, we
	 * cannot use it relating to the command line args before AND
	 * after that invocation
	 */
	if (!strstr(argv[1], delim)) {
	printl(PRINT_HIGH, "No %s separating the component list from the dependencies\n", delim);
		goto exit;
	}

	/* find the last : */
	servs = strtok(argv[1], delim);
	while ((ndeps = strtok(NULL, delim))) {
		dependencies = ndeps;
		*(ndeps-1) = ':';
	}
	*(dependencies-1) = '\0';
	//printf("comps: %s\ndeps: %s\n", servs, dependencies);

	if (!servs) {
		print_usage(argc, argv);
		goto exit;
	}

	bfd_init();

	services = prepare_service_symbs(servs);

	print_objs_symbs(services);

//	printl(PRINT_DEBUG, "Loading at %x:%d.\n", BASE_SERVICE_ADDRESS, DEFAULT_SERVICE_SIZE);

	if (!dependencies) {
		printl(PRINT_HIGH, "No dependencies given, not proceeding.\n");
		goto dealloc_exit;
	}

	if (deserialize_dependencies(dependencies, services)) {
		printl(PRINT_HIGH, "Error processing dependencies.\n");
		goto dealloc_exit;
	}

	if (verify_dependency_completeness(services)) {
		printl(PRINT_HIGH, "Unresolved symbols, not linking.\n");
		goto dealloc_exit;
	}

	if (verify_dependency_soundness(services)) {
		printl(PRINT_HIGH, "Services arranged in an invalid configuration, not linking.\n");
		goto dealloc_exit;
	}

	gen_stubs_and_link(stub_gen_prog, services);
	if (load_all_services(services)) {
		printl(PRINT_HIGH, "Error loading services, aborting.\n");
		goto dealloc_exit;
	}

//	print_kern_symbs(services);

	setup_kernel(services);

	ret = 0;

 dealloc_exit:
	while (services) {
		struct service_symbs *next = services->next;
		free_service_symbs(services);
		services = next;
	}
	/* FIXME: new goto label to dealloc spds */
 exit:
	return 0;
}
