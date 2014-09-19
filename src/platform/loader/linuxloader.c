
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
#include "cl_globals.h"
#include "cl_inline.h"

#include <cos_config.h>
#include <cobj_format.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>
#include <signal.h>
#include <libgen.h>
#include <unistd.h>

#include <bfd.h>

int service_get_spdid(struct service_symbs *ss);

int is_hl_booter_loaded(struct service_symbs *s)
{
	return s->is_composite_loaded;
}

#ifdef DEBUG
void print_syms(bfd *obj)
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

int make_cobj_caps(struct service_symbs *s, struct cobj_header *h);

/*
 * Has this service already been processed?
 */
inline int service_processed(char *obj_name, struct service_symbs *services)
{
	while (services) {
		if (!strcmp(services->obj, obj_name)) {
			return 1;
		}
		services = services->next;
	}

	return 0;
}

int
symb_already_undef(struct service_symbs *ss, const char *name)
{
	int i;
	struct symb_type *undef = &ss->undef;

	for (i = 0 ; i < undef->num_symbs ; i++) {
		if (!strcmp(undef->symbs[i].name, name)) return 1;
	}
	return 0;
}


void print_kern_symbs(struct service_symbs *services)
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

#include "../linux/module/aed_ioctl.h"

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

struct symb *spd_contains_symb(struct service_symbs *s, char *name) 
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

int cap_get_info(struct service_symbs *service, struct cap_ret_info *cri, struct symb *symb)
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

int create_spd_capabilities(struct service_symbs *service/*, struct spd_info *si*/, int cntl_fd)
{
	int i;
	struct symb_type *undef_symbs = &service->undef;
	struct spd_info *spd = (struct spd_info*)service->extern_info;
	
	assert(!is_booter_loaded(service));
	for (i = 0 ; i < undef_symbs->num_symbs ; i++) {
		struct symb *symb = &undef_symbs->symbs[i];
		struct cap_ret_info cri;

		if (cap_get_info(service, &cri, symb)) return -1;
		assert(!is_booter_loaded(cri.serv));
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

	assert(!is_booter_loaded(s));
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

int service_get_spdid(struct service_symbs *ss)
{
	if (is_booter_loaded(ss)) { 
		return (int)ss->cobj->id;
	} else {
		assert(ss->extern_info);
		return ((struct spd_info*)ss->extern_info)->spd_handle;
	}
}

int serialize_spd_graph(struct comp_graph *g, int sz, struct service_symbs *ss)
{
	struct comp_graph *edge;
	int g_frontier = 0;

	while (ss) {
		int i, cid, sid;

		if (is_booter_loaded(ss)) {
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
void make_spd_mpd_mgr(struct service_symbs *mm, struct service_symbs *all)
{
	int **heap_ptr, *heap_ptr_val;
	struct comp_graph *g;

	if (is_booter_loaded(mm)) {
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

void make_spd_init_file(struct service_symbs *ic, const char *fname)
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

	if (is_booter_loaded(ic)) {
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

int make_cobj_caps(struct service_symbs *s, struct cobj_header *h)
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

struct service_symbs *find_obj_by_name(struct service_symbs *s, const char *n);

void make_spd_config_comp(struct service_symbs *c, struct service_symbs *all);

int 
spd_already_loaded(struct service_symbs *c)
{
	return c->already_loaded || !is_hl_booter_loaded(c);
}

void 
make_spd_boot_schedule(struct service_symbs *comp, struct service_symbs **sched, 
		       unsigned int *off)
{
	int i;

	if (spd_already_loaded(comp)) return;

	for (i = 0 ; i < comp->num_dependencies ; i++) {
		struct dependency *d = &comp->dependencies[i];

		if (!spd_already_loaded(d->dep)) {
			make_spd_boot_schedule(d->dep, sched, off);
		}
		assert(spd_already_loaded(d->dep));
	}
	sched[*off] = comp;
	comp->already_loaded = 1;
	(*off)++;
	printl(PRINT_HIGH, "\t%d: %s\n", *off, comp->obj);
}

void format_config_info(struct service_symbs *ss, struct component_init_str *data);

void 
make_spd_boot(struct service_symbs *boot, struct service_symbs *all)
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

	if (service_get_spdid(boot) != LLBOOT_BOOT) {
		printf("Booter component must be component number %d, is %d.\n"
		       "\tSuggested fix: Your first four components should be e.g. "
		       "c0.o, ;llboot.o, ;*fprr.o, ;mm.o, ;print.o, ;boot.o, ;\n", LLBOOT_BOOT, service_get_spdid(boot));
		exit(-1);
	}

	/* should be loaded by llboot */
	assert(is_booter_loaded(boot) && !is_hl_booter_loaded(boot)); 
	assert(boot->cobj->nsect == MAXSEC_S); /* extra section for other components */
	/* Assign ids to the booter-loaded components. */
	for (all = first ; NULL != all ; all = all->next) {
		if (!is_hl_booter_loaded(all)) continue;

		h = all->cobj;
		assert(h);
		cnt++;
		tot_sz += h->size;
	}

	schedule = malloc(sizeof(struct service_symbs *) * cnt);
	assert(schedule);
	printl(PRINT_HIGH, "Loaded component's initialization scheduled in the following order:\n");
	for (all = first ; NULL != all ; all = all->next) {
		make_spd_boot_schedule(all, schedule, &off);
	}

	/* Setup the capabilities for each of the booter-loaded
	 * components */
	all = first;
	for (all = first ; NULL != all ; all = all->next) {
		if (!is_hl_booter_loaded(all)) continue;

		if (make_cobj_caps(all, all->cobj)) {
			printl(PRINT_HIGH, "Could not create capabilities in cobj for %s\n", all->obj);
			exit(-1);
		}
	}

	all_obj_sz = 0;
	/* Find the cobj's size */
	for (all = first ; NULL != all ; all = all->next) {
		struct cobj_header *h;

		if (!is_hl_booter_loaded(all)) continue;
		printl(PRINT_HIGH, "booter found %s:%d with len %d\n", 
		       all->obj, service_get_spdid(all), all->cobj->size)
		n++;

		assert(is_hl_booter_loaded(all));
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

	/* Initialize the new section */
	{
		struct cobj_sect *s_prev;

		new_h->size += all_obj_sz;

		s_prev = cobj_sect_get(new_h, INITFILE_S);

		cobj_sect_init(new_h, INITFILE_S, cos_sect_get(INITFILE_S)->cobj_flags, 
			       round_up_to_page(s_prev->vaddr + s_prev->bytes), 
			       all_obj_sz);
	}
	new_sect_start  = new_end = cobj_sect_contents(new_h, INITFILE_S);
	new_vaddr_start = cobj_sect_get(new_h, INITFILE_S)->vaddr;
#define ADDR2VADDR(a) ((a-new_sect_start)+new_vaddr_start)

	ci = (void *)cobj_vaddr_get(new_h, (u32_t)get_symb_address(&boot->exported, COMP_INFO));
	assert(ci);
	ci->cos_poly[0] = ADDR2VADDR(new_sect_start);

	/* copy the cobjs */
	for (all = first ; NULL != all ; all = all->next) {
		struct cobj_header *h;

		if (!is_hl_booter_loaded(all)) continue;
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

void
spd_assign_ids(struct service_symbs *all)
{
	struct cobj_header *h;

	/* Assign ids to the booter-loaded components. */
	for (; NULL != all ; all = all->next) {
		if (!is_booter_loaded(all)) continue;

		h = all->cobj;
		assert(h);
		spdid_inc++;
		h->id = spdid_inc;
	}
}

void 
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
		if (!is_booter_loaded(all)) continue;

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

		if (!is_booter_loaded(all) || is_hl_booter_loaded(all)) continue;
		n++;

		heap_ptr_val = (int*)*heap_ptr;
		assert(is_booter_loaded(all));
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

void format_config_info(struct service_symbs *ss, struct component_init_str *data)
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

		if (is_booter_loaded(ss)) {
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

void make_spd_config_comp(struct service_symbs *c, struct service_symbs *all)
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

struct service_symbs *find_obj_by_name(struct service_symbs *s, const char *n)
{
	while (s) {
		if (!strncmp(&s->obj[5], n, strlen(n))) {
			return s;
		}

		s = s->next;
	}

	return NULL;
}

int (*fn)(void);

void setup_kernel(struct service_symbs *services)
{
	struct service_symbs /* *m, */ *s;
	struct service_symbs *init = NULL;
	struct spd_info *init_spd = NULL, *llboot_spd;
	struct cos_thread_info thd;

	pid_t pid;
	/* pid_t children[NUM_CPU]; */
	int cntl_fd = 0, i, /* cpuid, */ ret;
	unsigned long long start, end;
	
#ifdef __LINUX_COS
	set_curr_affinity(0);
#endif

	cntl_fd = aed_open_cntl_fd();

	s = services;
	while (s) {
		struct service_symbs *t;
		struct spd_info *t_spd;

		t = s;
		if (!is_booter_loaded(s)) {
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
		if (!is_booter_loaded(s)) {
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
		make_spd_boot(s, services);
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
		/* cpuid = i; */
		pid = fork();
		/* children[i] = pid; */
		if (pid == 0) break;
		printf("Created pid %d for core %d.\n", pid, i);
	}

	if (pid) {
                /* Parent process should give other processes a chance
		 * to run. They need to migrate to their cores. */
		sleep(1);
	} else { /* child process: set own affinity first */ 
#ifdef __LINUX_COS
		set_curr_affinity(cpuid);
#ifdef HIGHEST_PRIO
		set_prio();
#endif
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

	rdtscll(start);
	ret = fn();
	rdtscll(end);

	aed_enable_syscalls(cntl_fd);

	cos_restore_hw_entry(cntl_fd);

	if (pid > 0) {
		int child_status;
		while (wait(&child_status) > 0) ;
	} else {
		exit(getpid());
	}

	close(cntl_fd);

	return;
}








#include <assert.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/ucontext.h>
#include <unistd.h>

#include "cl_types.h"
#include "cl_macros.h"

#include <cos_config.h>
#ifdef LINUX_HIGHEST_PRIORITY
#define HIGHEST_PRIO 1
#endif


enum {PRINT_NONE = 0, PRINT_HIGH, PRINT_NORMAL, PRINT_DEBUG} print_lvl = PRINT_DEBUG;

#ifdef FAULT_SIGNAL
void segv_handler(int signo, siginfo_t *si, void *context) {
        ucontext_t *uc = context;
        struct sigcontext *sc = (struct sigcontext *)&uc->uc_mcontext;

        printl(PRINT_HIGH, "Segfault: Faulting address %p, ip: %lx\n", si->si_addr, sc->eip);
        exit(-1);
}
#endif

#ifdef ALRM_SIGNAL
void alrm_handler(int signo, siginfo_t *si, void *context) {
        printl(PRINT_HIGH, "Alarm! Time to exit!\n");
        exit(-1);
}
#endif


void
call_getrlimit(int id, char *name)
{
        struct rlimit rl;

        if (getrlimit(id, &rl)) {
                perror("getrlimit: "); printl(PRINT_HIGH, "\n");
                exit(-1);
        }
        /* printl(PRINT_HIGH, "rlimit for %s is %d:%d (inf %d)\n",  */
        /*        name, (int)rl.rlim_cur, (int)rl.rlim_max, (int)RLIM_INFINITY); */
}

void
call_setrlimit(int id, rlim_t c, rlim_t m)
{
        struct rlimit rl;

        rl.rlim_cur = c;
        rl.rlim_max = m;
        if (setrlimit(id, &rl)) {
                perror("getrlimit: "); printl(PRINT_HIGH, "\n");
                exit(-1);
        }
}

void
set_curr_affinity(u32_t cpu)
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

void
set_prio(void)
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

void
set_smp_affinity()
{
        char cmd[64];
        /* everything done is the python script. */
        sprintf(cmd, "python set_smp_affinity.py %d %d", NUM_CPU, getpid());
        system(cmd);
}


void
setup_thread(void)
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
