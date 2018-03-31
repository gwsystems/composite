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
#include "cl_globals.h"
#include "cl_inline.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>

char *service_name = NULL;

struct private_symtab {
	char name[MAX_SYMB_LEN];
	unsigned long vma_val;
};

unsigned long
getsym(bfd *obj, char* symbol)
{
	long storage_needed;
	asymbol **symbol_table = NULL;
	static struct private_symtab *private_symtabs = NULL;
	static char prev_name[MAX_FILE_NAME_LEN] = "";
	long number_of_symbols;
	int i;

	assert(service_name);
	if (strcmp(prev_name, service_name)) {
		strcpy(prev_name, service_name);

		storage_needed = bfd_get_symtab_upper_bound (obj);
		printl(PRINT_DEBUG, "\tAllocating new symbol table...\n");

		if (storage_needed <= 0){
			printl(PRINT_HIGH, "no symbols in object file\n");
			exit(-1);
		}

		symbol_table = (asymbol **) malloc(storage_needed);
		assert(symbol_table);
		number_of_symbols = bfd_canonicalize_symtab(obj, symbol_table);

		/* Free previous cahced symtabs */
		if (private_symtabs == NULL) {
			free(private_symtabs);
		}
		private_symtabs = malloc(number_of_symbols * sizeof(struct private_symtab));
		assert(private_symtabs);
		for (i = 0 ; i < number_of_symbols ; i++) {
			assert(symbol_table[i]->name);
			strcpy(private_symtabs[i].name, symbol_table[i]->name);
			private_symtabs[i].vma_val = symbol_table[i]->section->vma + symbol_table[i]->value;
		}
	}

	assert(private_symtabs != NULL);

	/* notes: symbol_table[i]->flags & (BSF_FUNCTION | BSF_GLOBAL) */
	for (i = 0; i < number_of_symbols; i++) {
		if(!strcmp(symbol,  private_symtabs[i].name)){
			return private_symtabs[i].vma_val;
		}
	}

	printl(PRINT_DEBUG, "Unable to find symbol named %s\n", symbol);
	return 0;
}


int
set_object_addresses(bfd *obj, struct service_symbs *obj_data)
{
	struct symb_type *st = &obj_data->exported;
	int i;

	for (i = 0 ; i < st->num_symbs ; i++) {
		char *symb = st->symbs[i].name;
		unsigned long addr = getsym(obj, symb);
		if (addr == 0) {
			printl(PRINT_DEBUG, "Symbol %s has invalid address.\n", symb);
			return -1;
		}

		st->symbs[i].addr = addr;
	}

	return 0;
}


int
make_cobj_symbols(struct service_symbs *s, struct cobj_header *h)
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
		{.name = COMP_PLT, .type = COBJ_SYMB_COMP_PLT},
		{.name = NULL, .type = 0}
	};

	/* Create the symbols */
	printl(PRINT_DEBUG, "%s loaded by Composite\n", s->obj);
	printl(PRINT_DEBUG, "\tMap symbols:\n");
	for (i = 0 ; map[i].name != NULL ; i++) {
		addr = (u32_t)get_symb_address(&s->exported, map[i].name);
		printl(PRINT_DEBUG, "\tsymb %s, addr %x, nsymb %d\n", map[i].name, addr, i);
		printf("\tsymb %s, addr %x, nsymb %d\n", map[i].name, addr, i);
		/* ST_user_caps offset is 0 when not relevant. */
		if (addr && cobj_symb_init(h, symb_offset++, map[i].name, map[i].type, addr, 0)) {
			printl(PRINT_HIGH, "boot component: couldn't create map cobj symb for %s (%d).\n", map[i].name, i);
			return -1;
		}
	}

	printl(PRINT_DEBUG, "\tExported symbols:\n");
	for (i = 0 ; i < s->exported.num_symbs ; i++) {
		printl(PRINT_DEBUG, "\tsymb %s, nsymb %d\n", s->exported.symbs[i].name, i);

		/* ST_user_caps offset is 0 when not relevant. */
		if (cobj_symb_init(h, symb_offset++, s->exported.symbs[i].name, COBJ_SYMB_EXPORTED, s->exported.symbs[i].addr, 0)) {
			printl(PRINT_HIGH, "boot component: couldn't create exported cobj symb for %s (%d).\n", s->exported.symbs[i].name, i);
			return -1;
		}
	}

	return 0;
}


int
calc_offset(int offset, asection *sect)
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


int
calculate_mem_size(int first, int last)
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


void
emit_address(FILE *fp, unsigned long addr)
{
	fprintf(fp, ". = 0x%x;\n", (unsigned int)addr);
}

void
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

void
findsections_srcobj(bfd *abfd, asection *sect, PTR obj)
{
	findsections(sect, obj, 0);
}

void
findsections_ldobj(bfd *abfd, asection *sect, PTR obj)
{
	findsections(sect, obj, 1);
}



void
emit_section(FILE *fp, char *sec)
{
	/*
	 * The kleene star after the section will collapse
	 * .rodata.str1.x for all x into the only case we deal with
	 * which is .rodata
	 */
	//      fprintf(fp, ".%s : { *(.%s*) }\n", sec, sec);
	fprintf(fp, "%s : { *(%s*) }\n", sec, sec);
}

void
run_linker(char *input_obj, char *output_exe, char *script)
{
	char linker_cmd[256];
	sprintf(linker_cmd, LINKER_BIN " -m elf_i386 -T %s -o %s %s", script, output_exe,
			input_obj);
	printl(PRINT_DEBUG, "%s\n", linker_cmd);
	fflush(stdout);
	system(linker_cmd);
}



/*
 * Look at sections and determine sizes of the text and
 * and data portions of the object file once loaded
 */

int
genscript(int with_addr, char *tmp_exec, char *script)
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


void
section_info_init(struct cos_sections *cs)
{
	int i;

	for (i = 0 ; cs[i].secid != MAXSEC_S ; i++) {
		cs[i].srcobj.s = cs[i].ldobj.s = NULL;
		cs[i].srcobj.offset = cs[i].ldobj.offset = 0;
	}
}

int
load_service(struct service_symbs *ret_data, unsigned long lower_addr, unsigned long size)
{
	bfd *obj, *objout;
	int sect_sz, offset, tot_static_mem = 0;
	void *ret_addr;
	service_name = ret_data->obj;
	struct cobj_header *h;
	int i;
	char script[64];
	char tmp_exec[128];

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
	genscript(0, tmp_exec, script);
	run_linker(service_name, tmp_exec, script);

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
	if (!is_booter_loaded(ret_data)) {
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

		nsymbs = ret_data->exported.num_symbs + 2; /* +2 is for COMP_INFO and COMP_PLT */
		ncaps  = ret_data->undef.num_symbs;
		nsymbs += ncaps; /* We must track undefined symbols in addition to exports */
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
				ret_data->is_scheduler ? COBJ_INIT_THD : 0);
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
	genscript(1, tmp_exec, script);
	run_linker(service_name, tmp_exec, script);
	unlink(script);
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
		if (!is_booter_loaded(ret_data)) {
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

	if (is_booter_loaded(ret_data)) {
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

unsigned long
load_all_services(struct service_symbs *services)
{
	unsigned long service_addr = BASE_SERVICE_ADDRESS + DEFAULT_SERVICE_SIZE;
	long sz;

	while (services) {
		sz = services->mem_size = load_service(services, service_addr, DEFAULT_SERVICE_SIZE);
		if (!sz) return -1;

		if (strstr(services->obj, LLBOOT_COMP)) llboot_mem = sz;

		service_addr += DEFAULT_SERVICE_SIZE;
		/* note this works for the llbooter and root memory manager too */
		if (strstr(services->obj, BOOT_COMP) || strstr(services->obj, LLBOOT_COMP)) { // Booter needs larger VAS
			service_addr += 15*DEFAULT_SERVICE_SIZE;
		} else if (strstr(services->obj, INITMM) || sz > DEFAULT_SERVICE_SIZE) {
			service_addr += 3*DEFAULT_SERVICE_SIZE;
		}

		printl(PRINT_DEBUG, "\n");
		services = services->next;
	}

	return service_addr;
}
