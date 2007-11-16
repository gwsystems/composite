/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <signal.h>

#include <bfd.h>

/* composite includes */
#include <spd.h>
#include <thread.h>
#include <ipc.h>

#define NUM_KERN_SYMBS 2
const char *USER_CAP_TBL_NAME = "ST_user_caps";
const char *ST_INV_FN_NAME = "ST_direct_invocation";
const char *UPCALL_ENTRY_NAME = "cos_upcall_entry";
const char *SCHED_PAGE_NAME = "cos_sched_notifications";

#define BASE_SERVICE_ADDRESS SERVICE_START
#define DEFAULT_SERVICE_SIZE SERVICE_SIZE

#define LINKER_BIN "/usr/bin/ld"
#define GCC_BIN "gcc"

/** 
 * gabep1: the SIG_S is a section devoted to the signal handling and
 * it must be in a page on its own one page away from the rest of the
 * module 
 */
enum {TEXT_S, RODATA_S, DATA_S, BSS_S, INVSTK_S, MAXSEC_S};

struct sec_info {
	asection *s;
	int offset;
};

unsigned long ro_start;
unsigned long data_start;
unsigned long inv_stk_start;

char script[64];
char tmp_exec[128];

struct sec_info srcobj[MAXSEC_S];
struct sec_info ldobj[MAXSEC_S];

#define UNDEF_SYMB_TYPE 0x1
#define EXPORTED_SYMB_TYPE 0x2
#define MAX_SYMBOLS 64
#define MAX_TRUSTED 16

typedef int (*observer_t)(asymbol *, void *data);

struct service_symbs;

struct symb {
	char *name;
	vaddr_t addr;
	struct service_symbs *exporter;
	struct symb *exported_symb;
};

struct symb_type {
	int num_symbs;
	struct symb symbs[MAX_SYMBOLS];

	struct service_symbs *parent;
};

struct service_symbs {
	char *obj;
	unsigned long lower_addr, size;
	
	struct spd *spd;
	struct symb_type exported, undef;
	int num_dependencies;
	struct service_symbs *dependencies[MAX_TRUSTED];
	struct service_symbs *next;
	int depth;
};

static unsigned long getsym(bfd *obj, char* symbol)
{
	long storage_needed;
	asymbol **symbol_table;
	long number_of_symbols;
	int i;
	
	storage_needed = bfd_get_symtab_upper_bound (obj);
	
	if (storage_needed <= 0){
		printf("no symbols in object file\n");
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

	printf("Unable to find symbol named %s\n", symbol);
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
		printf("no symbols in object file\n");
		exit(-1);
	}
	
	symbol_table = (asymbol **) malloc (storage_needed);
	number_of_symbols = bfd_canonicalize_symtab(obj, symbol_table);

	//notes: symbol_table[i]->flags & (BSF_FUNCTION | BSF_GLOBAL)
	for (i = 0; i < number_of_symbols; i++) {
		printf("name: %s, addr: %d, flags: %s, %s%s%s, in sect %s%s%s.\n",  
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
	//printf("Unable to find symbol named %s\n", executive_entry_symbol);
	//return -1;
	return;
}
#endif

static void findsections(bfd *abfd, asection *sect, PTR obj)
{
	struct sec_info *sec_arr = obj;
	
	if(!strcmp(sect->name, ".text")){
		sec_arr[TEXT_S].s = sect;
	}
	else if(!strcmp(sect->name, ".data")){
		sec_arr[DATA_S].s = sect;
	}
	else if(!strcmp(sect->name, ".rodata")){
		sec_arr[RODATA_S].s = sect;
	}
	else if(!strcmp(sect->name, ".bss")){
		sec_arr[BSS_S].s = sect;
	}
	else if (!strcmp(sect->name, ".inv_stk")) {
		sec_arr[INVSTK_S].s = sect;
	}
}

static int calc_offset(int offset, asection *sect)
{
	int align;
	
	if(!sect){
		return offset;
	}
	align = (1 << sect->alignment_power) - 1;
	
	if(offset & align){
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
		if(srcobj[i].s == NULL)
			continue;
		offset = calc_offset(offset, srcobj[i].s);
		srcobj[i].offset = offset;
		offset += bfd_get_section_size(srcobj[i].s);
	}
	
	return offset;
}

static void emit_address(FILE *fp, unsigned long addr)
{
	fprintf(fp, ". = 0x%x;\n", (unsigned int)addr);
}

static void emit_section(FILE *fp, char *sec)
{
	fprintf(fp, ".%s : { *(.%s) }\n", sec, sec);
}

/* Look at sections and determine sizes of the text and
 * and data portions of the object file once loaded */

static int genscript(int with_addr)
{
	FILE *fp;
	
	sprintf(script, "/tmp/loader_script.%d", getpid());
	sprintf(tmp_exec, "/tmp/loader_exec.%d.%d", with_addr, getpid());

	fp = fopen(script, "w");
	if(fp == NULL){
		perror("fopen failed");
		exit(-1);
	}
	
	fprintf(fp, "SECTIONS\n{\n");

	//if (with_addr) emit_address(fp, sig_start);
	//emit_section(fp, "signal_handling");
	if (with_addr) emit_address(fp, ro_start);
	emit_section(fp, "text");
	emit_section(fp, "rodata");
	if (with_addr) emit_address(fp, data_start);
	emit_section(fp, "data");
	emit_section(fp, "bss");
	if (with_addr) emit_address(fp, inv_stk_start);
	emit_section(fp, "inv_stk");
	
	fprintf(fp, "}\n");
	fclose(fp);

	return 0;
}

static void run_linker(char *input_obj, char *output_exe)
{
	char linker_cmd[256];
	sprintf(linker_cmd, LINKER_BIN " -T %s -o %s %s", script, output_exe,
		input_obj);
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
		printf("ret %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((retaddr = (unsigned int*)getsym(obj, "blah")))
		printf("blah %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((retaddr = (unsigned int*)getsym(obj, "zero")))
		printf("zero %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((blah = (char**)getsym(obj, "str")))
		printf("str %x is %s.\n", (unsigned int)blah, *blah);
	if ((retaddr = (unsigned int*)getsym(obj, "other")))
		printf("other %x is %d.\n", (unsigned int)retaddr, *retaddr);
	if ((fn = (fn_t)getsym(obj, "foo")))
		printf("retval from foo: %d (%d).\n", fn(), (int)*str);
*/
	for (i = 0 ; i < st->num_symbs ; i++) {
		char *symb = st->symbs[i].name;
		unsigned long addr = getsym(obj, symb);
/*
		printf("Symbol %s at address 0x%x.\n", symb, 
		       (unsigned int)addr);
*/
		if (addr == 0) {
			printf("Symbol %s has invalid address.\n", symb);
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

static int load_service(struct service_symbs *ret_data, unsigned long lower_addr, 
			unsigned long size)
{
	bfd *obj, *objout;
	void *tmp_storage;

	int ro_size;
	int alldata_size;
	void *ret_addr;
	char *service_name = ret_data->obj; 

	if (!service_name) {
		printf("Invalid path to executive.\n");
		return -1;
	}

	printf("Processing object %s:\n", service_name);

	genscript(0);
	run_linker(service_name, tmp_exec);
	
	obj = bfd_openr(tmp_exec, "elf32-i386");
	if(!obj){
		bfd_perror("Object open failure\n");
		return -1;
	}
	
	if(!bfd_check_format(obj, bfd_object)){
		printf("Not an object file!\n");
		return -1;
	}
	
	bfd_map_over_sections(obj, findsections, srcobj);

	ro_start = lower_addr;
	/* Determine the size of and allocate the text and Read-Only data area */
	ro_size = calculate_mem_size(TEXT_S, DATA_S);

	printf("\tRead only section: %x:%d.\n",
	       (unsigned int)ro_start, (unsigned int)ro_size);

	ro_size = round_up_to_page(ro_size);

	/**
	 * FIXME: needing PROT_WRITE is daft, should write to file,
	 * then map in ro
	 */
	ret_addr = mmap((void*)ro_start, ro_size,
			PROT_EXEC | PROT_READ | PROT_WRITE,
			MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
			0, 0);

	if (MAP_FAILED == ret_addr){
		perror("Couldn't map text segment into address space");
		return -1;
	}
	
	data_start = ro_start + ro_size;
	/* Allocate the read-writable areas .data .bss */
	alldata_size = calculate_mem_size(DATA_S, MAXSEC_S);

	printf("\tData section: %x:%d\n",
	       (unsigned int)data_start, (unsigned int)alldata_size);

	alldata_size = round_up_to_page(alldata_size);

	if (alldata_size != 0) {
		ret_addr = mmap((void*)data_start, alldata_size,
				PROT_WRITE | PROT_READ,
				MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
				0, 0);

		if (MAP_FAILED == ret_addr){
			perror("Couldn't map data segment into address space");
			return -1;
		}
	}
	
	tmp_storage = malloc(ro_size > alldata_size ? ro_size : alldata_size);
	if(tmp_storage == NULL)
	{
		perror("Memory allocation failed\n");
		return -1;
	}
	
	unlink(tmp_exec);
	genscript(1);
	run_linker(service_name, tmp_exec);
	unlink(script);
	
	objout = bfd_openr(tmp_exec, "elf32-i386");
	if(!objout){
		bfd_perror("Object open failure\n");
		return -1;
	}
	
	if(!bfd_check_format(objout, bfd_object)){
		printf("Not an object file!\n");
		return -1;
	}
	
	bfd_map_over_sections(objout, findsections, &ldobj[0]);

	/* get the text and ro sections in a buffer */
	bfd_get_section_contents(objout, ldobj[TEXT_S].s,
				 tmp_storage + srcobj[TEXT_S].offset, 0,
				 bfd_sect_size(objout, srcobj[TEXT_S].s));
	
	if(ldobj[RODATA_S].s){
		bfd_get_section_contents(objout, ldobj[RODATA_S].s,
					 tmp_storage + srcobj[RODATA_S].offset, 0,
					 bfd_sect_size(objout, srcobj[RODATA_S].s));
	}
	
	/* 
	 * ... and copy that buffer into the actual memory location
	 * object is linked for (load it!)
	 */
	printf("\tCopying RO to %x from %x of size %x.\n", 
	       (unsigned int) ro_start, (unsigned int)tmp_storage, (unsigned int)ro_size);
	memcpy((void*)ro_start, tmp_storage, ro_size);

	printf("\tCopying data from object to %x:%x.\n", 
	       (unsigned int)tmp_storage + srcobj[DATA_S].offset, 
	       (unsigned int)bfd_sect_size(obj, srcobj[DATA_S].s));

	/* and now do the same for the data and BSS */
	bfd_get_section_contents(objout, ldobj[DATA_S].s,
				 tmp_storage + srcobj[DATA_S].offset, 0,
				 bfd_sect_size(obj, srcobj[DATA_S].s));
	
	printf("\tZeroing out from %x of size %x.\n", 
	       (unsigned int)tmp_storage + srcobj[BSS_S].offset,
	       (unsigned int)bfd_sect_size(obj, srcobj[BSS_S].s));

	/* Zero bss */
	memset(tmp_storage + srcobj[BSS_S].offset, 0,
	       bfd_sect_size(obj, srcobj[BSS_S].s));

	printf("\tCopying DATA to %x from %x of size %x.\n", 
	       (unsigned int) data_start, (unsigned int)tmp_storage, (unsigned int)alldata_size);
	
	memcpy((void*)data_start, tmp_storage, alldata_size);
	
	free(tmp_storage);

	if (set_object_addresses(objout, ret_data)) {
		printf("Could not find all object symbols.\n");
		return -1;
	}

	ret_data->lower_addr = lower_addr;
	ret_data->size = size;
	
	bfd_close(obj);
	bfd_close(objout);

	unlink(tmp_exec);

	return 0;

	/* 
	 * FIXME: unmap sections, free memory, unlink file, close bfd
	 * objects, etc... for the various error positions, i.e. all
	 * return -1;s
	 */
}

/* FIXME: should modify code to use
static struct service_symbs *get_dependency_by_index(struct service_symbs *s,
						     int index)
{
	if (index >= s->num_dependencies) {
		return NULL;
	}

	return s->dependencies[index];
}
*/
static int add_service_dependency(struct service_symbs *s, 
				  struct service_symbs *dep)
{
	if (!s || !dep || 
	    s->num_dependencies == MAX_TRUSTED) {
		return -1;
	}

	s->dependencies[s->num_dependencies] = dep;
	s->num_dependencies++;

	return 0;
}

static int initialize_service_symbs(struct service_symbs *str)
{
	str->exported.parent = str;
	str->undef.parent = str;
	str->next = NULL;
	str->exported.num_symbs = str->undef.num_symbs = 0;
	str->num_dependencies = 0;
	str->depth = -1;

	return 0;
}

static struct service_symbs *alloc_service_symbs(char *obj)
{
	struct service_symbs *str;
	char *obj_name = malloc(strlen(obj)+1);

	str = malloc(sizeof(struct service_symbs));
	if (!str || initialize_service_symbs(str)) {
		return NULL;
	}

	strcpy(obj_name, obj);
	str->obj = obj_name;

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
		printf("Have exceeded the number of allowed "
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
	
	storage_needed = bfd_get_symtab_upper_bound (obj);
	
	if (storage_needed <= 0){
		printf("no symbols in object file\n");
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
		    symbol_table[i]->flags & BSF_GLOBAL)) {
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
		bfd_perror("Object open failure\n");
		return -1;
	}
	
	if(!bfd_check_format(obj, bfd_object)){
		printf("Not an object file!\n");
		return -1;
	}
	
	if (symb_type == UNDEF_SYMB_TYPE) {
		st = &str->undef;
	} else if (symb_type == EXPORTED_SYMB_TYPE) {
		st = &str->exported;
	}
	for_each_symb_type(obj, symb_type, obs_serialize, st);

	bfd_close(obj);

	return 0;
}

static inline void print_symbs(struct symb_type *st)
{
	int i;

	for (i = 0 ; i < st->num_symbs ; i++) {
		printf("%s, ", st->symbs[i].name);
	}

	return;
}

static void print_objs_symbs(struct service_symbs *str)
{
	while (str) {
		printf("Service %s:\n\tExports: ", str->obj);
		print_symbs(&str->exported);
		printf("\n\tUndefined: ");
		print_symbs(&str->undef);
		printf("\n\n");

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

/* 
 * Assume that these are added LAST.  The last NUM_KERN_SYMBS are
 * ignored for most purposes so they must be the actual kern_syms.
 *
 * The kernel needs to know where a few symbols are, add them:
 * user_caps, cos_sched_notifications
 */
static void add_kernel_exports(struct service_symbs *service)
{
	struct symb_type *ex = &service->exported;
	const char *usr_caps = USER_CAP_TBL_NAME;
	const char *sched_page = SCHED_PAGE_NAME;

	ex->symbs[ex->num_symbs].name = malloc(strlen(usr_caps)+1);
	strcpy(ex->symbs[ex->num_symbs].name, usr_caps);
	ex->num_symbs++;

	ex->symbs[ex->num_symbs].name = malloc(strlen(sched_page)+1);
	strcpy(ex->symbs[ex->num_symbs].name, sched_page);
	ex->num_symbs++;

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
	char *delim = ",";
	char *tok;
	
	tok = strtok(services, delim);

	first = str = alloc_service_symbs(tok);

	do {
		if (obj_serialize_symbols(tok, EXPORTED_SYMB_TYPE, str) ||
		    obj_serialize_symbols(tok, UNDEF_SYMB_TYPE, str)) {
			printf("Could not operate on object %s: error.\n", tok);
			return NULL;
		}
		
		add_kernel_exports(str);

		tok = strtok(NULL, delim);
		
		if (tok) {
			str->next = alloc_service_symbs(tok);
			str = str->next;
		}
	} while (tok);
		
	return first;
}


/*
 * Find the exporter for a specific symbol from amongst a list of
 * exporters.
 */
static inline 
struct service_symbs *find_symbol_exporter(struct symb *s, 
					   struct service_symbs *exporters[],
					   int num_exporters, struct symb **exported)
{
	int i,j;

	for (i = 0 ; i < num_exporters ; i++) {
		struct symb_type *exp_symbs;

		exp_symbs = &exporters[i]->exported;

		for (j = 0 ; j < exp_symbs->num_symbs ; j++) {
			if (!strcmp(s->name, exp_symbs->symbs[j].name)) {
				*exported = &exp_symbs->symbs[j];
				return exporters[i];
			}
		}
	}

	return NULL;
}

/*
 * Verify that all symbols can be resolved by the present dependency
 * relations.  This is a programming language equivalent to
 * "completeness".
 *
 * Assumptions: All exported and undefined symbols are defined for
 * each service (prepare_service_symbs has been called), and that the
 * tree of services has been established designating the dependents of
 * each service (process_dependencies has been called).
 */
static int verify_dependency_completeness(struct service_symbs *services)
{
	int ret = 0;
	int i;

	/* for each of the services... */
	while (services) {
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
			exporter = find_symbol_exporter(symb, services->dependencies, 
							services->num_dependencies, &exp_symb);

			if (!exporter) {
				printf("Could not find exporter of symbol %s in service %s.\n",
				       symb->name, services->obj);

				ret = -1;
				goto exit;
			} else {
				symb->exporter = exporter;
				symb->exported_symb = exp_symb;
			}
		}
		
		services = services->next;
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
		struct service_symbs *d = services->dependencies[i];

		if (rec_verify_dag(d, current_depth+1, max_depth)) {
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
			printf("Cycle found in dependencies.  Not linking.\n");
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

	if (!deps) {
		return -1;
	}

	current = deps;
	next = current;

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
			printf("Could not find service %s.\n", tmp);
			return -1;
		}

		/* go through the | invoked services */
		tmp = strtok(NULL, parallel);
		while (tmp) {

			dep = get_service_struct(tmp, services);
			if (!dep) {
				printf("Could not find service %s.\n", tmp);
				return -1;
			} 
			if (dep == s) {
				printf("Reflexive relations not allowed (for %s).\n", 
				       s->obj);
				return -1;
			}

			add_service_dependency(s, dep);
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
		system(tmp_str);

		/* compile the stub */
		sprintf(tmp_str, GCC_BIN " -c -o %s_stub.o %s_stub.S", 
			tmp_name, tmp_name);
		system(tmp_str);

		/* link the stub to the service */
		sprintf(tmp_str, LINKER_BIN " -r -o %s.o %s_stub.o %s", 
			tmp_name, tmp_name, orig_name);
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
	void *ret_addr;
	unsigned long service_addr = BASE_SERVICE_ADDRESS;

	/* place the invocation stack */
	inv_stk_start = service_addr;
	service_addr += DEFAULT_SERVICE_SIZE;

	ret_addr = mmap((void*)inv_stk_start, PAGE_SIZE,
			PROT_WRITE | PROT_READ,
			MAP_FIXED | MAP_PRIVATE | MAP_ANONYMOUS,
			0, 0);
	
	if (MAP_FAILED == ret_addr){
		perror("Couldn't map the invocation stack into the address space");
		return -1;
	}

	while (services) {
		if (load_service(services, service_addr, DEFAULT_SERVICE_SIZE)) {
			return -1;
		}

		service_addr += DEFAULT_SERVICE_SIZE;

		printf("\n");
		services = services->next;
	}

	return 0;
}

static void print_kern_symbs(struct service_symbs *services)
{
	const char *u_tbl = USER_CAP_TBL_NAME;

	while (services) {
		vaddr_t addr;

		if ((addr = get_symb_address(&services->exported, u_tbl))) {
			printf("Service %s:\n\tusr_cap_tbl: %x\n",
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
/* //			printf("Requesting %d caps.\n", num_undef); */
/* 			s->spd = spd_alloc(num_undef, ucap_tbl); */
/* 		} */

/* //		printf("Service %s has spd %x.\n", s->obj, (unsigned int)s->spd); */

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
/* 				printf("Could not add capability for %s to %s.\n",  */
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
/* 		printf("Could not allocate thread.\n"); */
/* 		return; */
/* 	} */

/* 	thd_set_current(thd); */

/* 	return; */
/* } */

#include "../../aed_ioctl.h"

struct spd_info *create_spd(int cos_fd, struct service_symbs *s, 
			    int max_cap, long lowest_addr, long size) 
{
	struct spd_info *spd;
	struct usr_inv_cap *ucap_tbl;
	vaddr_t upcall_addr;
	
	ucap_tbl = (struct usr_inv_cap*)get_symb_address(&s->exported, 
							 USER_CAP_TBL_NAME);
	printf("Found ucap_tbl for component %s @ %p.\n", s->obj, ucap_tbl);

	upcall_addr = (vaddr_t)get_symb_address(&s->exported, UPCALL_ENTRY_NAME);
	if (upcall_addr == 0) {
		printf("Could not find %s in %s.\n", UPCALL_ENTRY_NAME, s->obj);
		return NULL;
	}
	printf("Found cos_upcall for component %s @ %p.\n", s->obj, (void*)upcall_addr);

	spd = (struct spd_info *)malloc(sizeof(struct spd_info));
	if (NULL == spd) {
		perror("Could not allocate memory for spd\n");
		return NULL;
	}
	
	spd->num_caps = max_cap;
	spd->ucap_tbl = (vaddr_t)ucap_tbl;
	spd->lowest_addr = lowest_addr;
	spd->size = size;
	spd->upcall_entry = upcall_addr;
	
	spd->spd_handle = cos_create_spd(cos_fd, spd);
	if (spd->spd_handle < 0) {
		printf("Could not create spd %s\n", s->obj);
		free(spd);
		return NULL;
	}
	printf("spd %s created with handle %d.\n", s->obj, (unsigned int)spd->spd_handle);

	return spd;
}

void make_spd_scheduler(int cntl_fd, struct spd_info *spd, struct service_symbs *s, struct spd_info *parent)
{
	vaddr_t sched_page;

	sched_page = (vaddr_t)get_symb_address(&s->exported, SCHED_PAGE_NAME);
	if (0 == sched_page) {
		printf("Could not find %s in %s.\n", SCHED_PAGE_NAME, s->obj);
		return;
	}
	printf("Found spd notification page @ %x.  Promoting to scheduler.\n", 
	       (unsigned int) sched_page);

	cos_promote_to_scheduler(cntl_fd, spd->spd_handle, (NULL == parent)? -1 : parent->spd_handle, sched_page);

	return;
}

/*
 * FIXME: all the exit(-1) -> return NULL, and handling in calling
 * function.
 */
struct cap_info *create_invocation_cap(struct spd_info *from_spd, struct service_symbs *from_obj, 
				       struct spd_info *to_spd, struct service_symbs *to_obj,
				       int cos_fd, char *client_stub, char *server_stub, 
				       char *server_fn, int flags)
{
	struct cap_info *cap;
	struct symb_type *st = &from_obj->undef;

	vaddr_t addr;
	int i;
	
	cap = (struct cap_info *)malloc(sizeof(struct cap_info));
	if (NULL == cap) {	
	printf("Could not allocate memory for invocation capability.\n");
		exit(-1);
	}

	/* find in what position the symbol was inserted into the
	 * user-level capability table (which position was opted for
	 * use), so that we can insert the information into the
	 * correct user-capability. */
	for (i = 0 ; i < st->num_symbs ; i++) {
		if (strcmp(server_fn, st->symbs[i].name) == 0) {
			break;
		}
	}
	if (i == st->num_symbs) {
		printf("Could not find the undefined symbol %s in %s.\n", 
		       server_fn, from_obj->obj);
		exit(-1);
	}
	
	addr = (vaddr_t)get_symb_address(&to_obj->exported, server_stub);
	if (addr == 0) {
		printf("Could not find %s in %s.\n", server_stub, to_obj->obj);
		exit(-1);
	}
	cap->SD_serv_stub = addr;
	addr = (vaddr_t)get_symb_address(&from_obj->exported, client_stub);
	if (addr == 0) {
		printf("could not find %s in %s.\n", client_stub, from_obj->obj);
		exit(-1);
	}
	cap->SD_cli_stub = addr;
	addr = (vaddr_t)get_symb_address(&to_obj->exported, server_fn);
	if (addr == 0) {
		printf("could not find %s in %s.\n", server_fn, to_obj->obj);
		exit(-1);
	}
	cap->ST_serv_entry = addr;
	
	cap->rel_offset = i+1; /* +1 for the automatically created return capability */
	cap->owner_spd_handle = from_spd->spd_handle;
	cap->dest_spd_handle = to_spd->spd_handle;
//	cap->il = 0;
	cap->il = 3;
	cap->flags = flags;

	cap->cap_handle = cos_spd_add_cap(cos_fd, cap);
 	if (cap->cap_handle == 0) {
		printf("Could not add capability # %d to %s (%d) for %s.\n", 
		       cap->rel_offset, from_obj->obj, cap->owner_spd_handle, server_fn);
		exit(-1);
	}
	
	return cap;
}

static void setup_kernel(struct service_symbs *services)
{
	struct service_symbs *s = services, *c0 = NULL, *c1 = NULL, *c2 = NULL, *pc = NULL;
	struct spd_info *spd0, *spd1, *spd2, *spdpc;
	struct cap_info *cap1, *cap1_5, *cap2, *capyield, *capnothing, *cappc, *cappcvals, *cappcsched;

	struct cos_thread_info thd;
	int cntl_fd, ret;
	int (*fn)(void);
	unsigned long long start, end;
	
	cntl_fd = aed_open_cntl_fd();

	while (s) {
		if (strstr(s->obj, "c0.o") != NULL) {
			c0 = s;
		} else if (strstr(s->obj, "c1.o") != NULL) {
			c1 = s;
		} else if (strstr(s->obj, "c2.o") != NULL) {
			c2 = s;
		} else if (strstr(s->obj, "print_comp.o") != NULL) {
			pc = s;
		}

		s = s->next;
	}
	if (c0 == NULL || c1 == NULL || c2 == NULL || pc == NULL) {
		fprintf(stderr, "Could not find service object.\n");
		exit(-1);
	}

	spd0 = create_spd(cntl_fd, c0, 2, 0, 0);
	spd1 = create_spd(cntl_fd, c1, 5, c1->lower_addr, c1->size);
	spd2 = create_spd(cntl_fd, c2, 1, c2->lower_addr, c2->size);
	spdpc = create_spd(cntl_fd, pc, 0, pc->lower_addr, pc->size);

	if (!spd0 || !spd1 || !spd2 || !spdpc) {
		printf("Could not allocate all of the spds.\n");
		exit(-1);
	}

	cap1  = create_invocation_cap(spd0, c0, spd2, c2, cntl_fd, 
				      "SS_ipc_client_marshal_args", "sched_init_inv", "sched_init", 0);
	cap1_5 = NULL; /*create_invocation_cap(spd0, c0, spdpc, pc, cntl_fd, 
			 "SS_ipc_client_marshal_args", "print_vals_inv", "print_vals", 0);*/
	cap2  = create_invocation_cap(spd1, c1, spd2, c2, cntl_fd, 
				      "SS_ipc_client_marshal_args", "spd2_inv", "spd2_fn", 0/*CAP_SAVE_REGS*/); 
	capyield  = create_invocation_cap(spd1, c1, spd2, c2, cntl_fd, 
					  "SS_ipc_client_marshal_args", "yield_inv", "yield", 0/*CAP_SAVE_REGS*/); 
	capnothing  = create_invocation_cap(spd1, c1, spd2, c2, cntl_fd, 
					    "SS_ipc_client_marshal_args", "nothing_inv", "nothing", 0/*CAP_SAVE_REGS*/); 

//	cappc = create_invocation_cap(spd1, c1, spdpc, pc, cntl_fd, 
//				      "SS_ipc_client_marshal", "print_inv", "print", 0);
	cappcvals = create_invocation_cap(spd1, c1, spdpc, pc, cntl_fd, 
					  "SS_ipc_client_marshal_args", "print_vals_inv", "print_vals", 0);
	cappcsched = create_invocation_cap(spd2, c2, spdpc, pc, cntl_fd, 
					   "SS_ipc_client_marshal_args", "print_vals_inv", "print_vals", 0);

	
	make_spd_scheduler(cntl_fd, spd2, c2, NULL);
	make_spd_scheduler(cntl_fd, spd1, c1, spd2);

	printf("Test created cap %d, %d, and %d.\n\n", 
	       (unsigned int)cap1->cap_handle, (unsigned int)cap2->cap_handle, (unsigned int)cappcvals->cap_handle);

	thd.spd_handle = spd0->spd_handle;
	thd.sched_handle = spd2->spd_handle;
	cos_create_thd(cntl_fd, &thd);

	printf("OK, good to go, calling fn\n");
	fflush(stdout);

	fn = (int (*)(void))get_symb_address(&c0->exported, "spd0_main");

#define ITER 1
#define rdtscll(val) __asm__ __volatile__("rdtsc" : "=A" (val))

	rdtscll(start);
	ret = fn();
	rdtscll(end);

	printf("Invocation takes %lld, ret %d.\n", (end-start)/ITER, ret);
	
	close(cntl_fd);

	return;
}

static inline void print_usage(char *prog_name)
{
	printf("Usage: %s <comma separated string of all "
	       "objs:truster1-trustee1|trustee2|...;truster2-...> "
	       "<path to gen_client_stub>\n",
	       prog_name);

	return;
}

#define STUB_PROG_LEN 128
extern vaddr_t SS_ipc_client_marshal;
extern vaddr_t DS_ipc_client_marshal;

#include <sys/ucontext.h>
void segv_handler(int signo, siginfo_t *si, void *context) {
	ucontext_t *uc = context;
	struct sigcontext *sc = (struct sigcontext *)&uc->uc_mcontext;

	printf("Segfault: Faulting address %p, ip: %lx\n", si->si_addr, sc->eip);
	exit(-1);
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
	char *servs, *dependencies, *stub_gen_prog;
	struct sigaction sa;
	int ret = -1;

	sa.sa_sigaction = segv_handler;
	sa.sa_flags = SA_SIGINFO;
	sigaction(SIGSEGV, &sa, NULL);
	
	if (argc != 3) {
		print_usage(argv[0]);
		goto exit;
	}

	stub_gen_prog = argv[2];

	/* 
	 * NOTE: because strtok is used in prepare_service_symbs, we
	 * cannot use it relating to the command line args before AND
	 * after that invocation
	 */
	servs = strtok(argv[1], delim);
	dependencies = strtok(NULL, delim);

	if (!servs) {
		print_usage(argv[0]);
		goto exit;
	}

	bfd_init();

	services = prepare_service_symbs(servs);

	print_objs_symbs(services);

//	printf("Loading at %x:%d.\n", BASE_SERVICE_ADDRESS, DEFAULT_SERVICE_SIZE);

	if (!dependencies) {
		printf("No dependencies given, not proceeding.\n");
		goto dealloc_exit;
	}
	
	if (deserialize_dependencies(dependencies, services)) {
		printf("Error processing dependencies.\n");
		goto dealloc_exit;
	}

	if (verify_dependency_completeness(services)) {
		printf("Unresolved symbols, not linking.\n");
		goto dealloc_exit;
	}

	if (verify_dependency_soundness(services)) {
		printf("Services arranged in an invalid configuration, not linking.\n");
		goto dealloc_exit;
	}
	
	gen_stubs_and_link(stub_gen_prog, services);
	if (load_all_services(services)) {
		printf("Error loading services, aborting.\n");
		goto dealloc_exit;
	}

	print_kern_symbs(services);

	setup_kernel(services);

/* 	start_composite(services); */

/* 	{ */
/* 		struct spd *spd; */
/* 		struct usr_inv_cap *c; */

/* 		spd = services->spd; */
/* 		c = &spd->user_cap_tbl[1]; */
/* 		c->invocation_fn = (vaddr_t)&DS_ipc_client_marshal; */
/* 		c = &spd->user_cap_tbl[2]; */
/* 		c->invocation_fn = (vaddr_t)&DS_ipc_client_marshal; */

/* 		spd = services->next->spd; */
/* 		c = &spd->user_cap_tbl[1]; */
/* 		c->invocation_fn = (vaddr_t)&DS_ipc_client_marshal; */

/* 		spd = services->next->next->spd; */
/* 		c = &spd->user_cap_tbl[1]; */
/* 		c->invocation_fn = (vaddr_t)&DS_ipc_client_marshal; */
/* 	} */

/* 	{ */
/* 		struct service_symbs *second = services;//->next;//services;//->next; */
/* 		unsigned long long start, end; */
/* 		vaddr_t entry; */
/* 		int (*fn)(); */
/* 		int ret; */
/* 		char *entry_name = "spd0_main"; */

/* #define ITER 10000 */
/* #define rdtscll(val) \ */
/*       __asm__ __volatile__("rdtsc" : "=A" (val)) */

/* 		entry = get_symb_address(&second->exported, entry_name); */
/* 		if (entry == 0) { */
/* 			printf("Could not find %s in %s.\n", entry_name, second->obj); */
/* 			goto dealloc_exit; */
/* 		} */
/* 		printf("Entry in %s for %s is @ %x.\n", second->obj, entry_name, */
/* 		       (unsigned int)entry); */
/* 		fn = (int (*)())entry; */

/* 		rdtscll(start); */
/* 		ret = fn(); */
/* 		rdtscll(end); */
/* 		printf("Invocation takes %lld, ret %d.\n", (end-start)/ITER, ret); */
/* 	} */

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

