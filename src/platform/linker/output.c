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
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

int
cos_spd_add_cap(struct cap_info *capi)
{
	static int ncap = 1;
	ncap++;
	return ncap;
}

int is_hl_booter_loaded(struct service_symbs *s)
{
	return s->is_composite_loaded;
}

int
service_get_spdid(struct service_symbs *ss)
{
	if (is_booter_loaded(ss)) {
		return (int)ss->cobj->id;
	} else {
		assert(ss->extern_info);
		return ((struct spd_info*)ss->extern_info)->spd_handle;
	}
}

/*
 * FIXME: all the exit(-1) -> return NULL, and handling in calling
 * function.
 */
/*struct cap_info **/
int
create_invocation_cap(struct spd_info *from_spd, struct service_symbs *from_obj,
			  struct spd_info *to_spd, struct service_symbs *to_obj,
			  char *client_fn, char *client_stub,
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

	cap.cap_handle = cos_spd_add_cap(&cap);

 	if (cap.cap_handle == 0) {
		printl(PRINT_DEBUG, "Could not add capability # %d to %s (%d) for %s.\n",
		       cap.rel_offset, from_obj->obj, cap.owner_spd_handle, server_fn);
		exit(-1);
	}

	return 0;
}

struct symb *
spd_contains_symb(struct service_symbs *s, char *name)
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

int
cap_get_info(struct service_symbs *service, struct cap_ret_info *cri, struct symb *symb)
{
	struct symb *exp_symb = symb->exported_symb;
	struct service_symbs *exporter = symb->exporter;
	struct symb *c_stub, *s_stub, *c_stub_rets;
	int is_rets = 0;
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
	c_stub_rets = c_stub;
	if (NULL == c_stub) {
		c_stub = spd_contains_symb(service, CAP_CLIENT_STUB_DEFAULT);
		if (NULL == c_stub) {
			printl(PRINT_HIGH, "Could not find a client stub for function %s in service %s.\n",
			       symb->name, service->obj);
			return -1;
		}
		c_stub_rets = spd_contains_symb(service, CAP_CLIENT_STUB_RETS);
		if (NULL == c_stub_rets) {
			printl(PRINT_HIGH, "Could not find a client stub for function %s in service %s.\n",
			       symb->name, service->obj);
			return -1;
		}
	}

	if (MAX_SYMB_LEN-1 == snprintf(tmp, MAX_SYMB_LEN-1, "%s%s", exp_symb->name, CAP_SERVER_STUB_POSTPEND_RETS)) {
		printl(PRINT_HIGH, "symbol name %s too long to become server capability\n", exp_symb->name);
		return -1;
	}

	s_stub = spd_contains_symb(exporter, tmp);
	if (NULL == s_stub) {
		printl(PRINT_HIGH, "Could not find server stub (%s) for function %s in service %s to satisfy %s.\n",
		       tmp, exp_symb->name, exporter->obj, service->obj);
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
	} else {
		is_rets = 1;
	}

	cri->csymb = symb;
	cri->ssymbfn = exp_symb;
	cri->cstub = is_rets ? c_stub_rets : c_stub;
	cri->sstub = s_stub;
	cri->serv = exporter;
	if (exp_symb->modifier_offset) {
		printf("%d: %s\n", service_get_spdid(exporter),
		       exp_symb->name + exp_symb->modifier_offset);
	}
	cri->fault_handler = (u32_t)fault_handler_num(exp_symb->name + exp_symb->modifier_offset);

	return 0;
}

int
create_spd_capabilities(struct service_symbs *service)
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
		if (create_invocation_cap(spd, service, cri.serv->extern_info, cri.serv,
					  cri.csymb->name, cri.cstub->name, cri.sstub->name,
					  cri.ssymbfn->name, 0)) {
			return -1;
		}
	}

	return 0;
}

struct spd_info *
create_spd(struct service_symbs *s, long lowest_addr, long size)
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
	spd->spd_handle = spdid_inc;
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

void
make_spd_scheduler(struct service_symbs *s, struct service_symbs *p)
{
	vaddr_t sched_page;
	struct cos_sched_data_area *sched_page_ptr;

	sched_page_ptr = (struct cos_sched_data_area*)get_symb_address(&s->exported, SCHED_NOTIF);
	sched_page = (vaddr_t)sched_page_ptr;

	printl(PRINT_DEBUG, "Found spd notification page @ %x.  Promoting to scheduler.\n",
	       (unsigned int) sched_page);

	return;
}


int
serialize_spd_graph(struct comp_graph *g, int sz, struct service_symbs *ss)
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

int **
get_heap_ptr(struct service_symbs *ss)
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
void
make_spd_mpd_mgr(struct service_symbs *mm, struct service_symbs *all)
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

int
make_cobj_caps(struct service_symbs *s, struct cobj_header *h)
{
	int i;
	struct symb_type *undef_symbs = &s->undef;

	printl(PRINT_DEBUG, "%s loaded by Composite -- Capabilities:\n", s->obj);
	for (i = 0 ; i < undef_symbs->num_symbs ; i++) {
		u32_t cap_off, dest_id, sfn, cstub, sstub, fault;
		struct symb *symb = &undef_symbs->symbs[i];
		struct cap_ret_info cri;

		/*
		 * Before creating the cobj, we add ncap to nsymb because we want to track both undefined and exports.
		 * In order to get the actual symbol index of the current undefined symbol, we need to undo that operation.
		 */
		int symbol_idx = h->nsymb - h->ncap + i;

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

		printl(PRINT_DEBUG, "\tInserting undefined symbol %s at offset %d into user caps array.\n", cri.csymb->name, cap_off + 1);
		if (cobj_symb_init(h, symbol_idx, cri.csymb->name, COBJ_SYMB_UNDEF, sfn, cap_off + 1)) {
			printl(PRINT_HIGH, "Couldn't create undefined symbol %s.\n", cri.csymb->name);
			return -1;
		}
	}

	return 0;
}

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

void
format_config_info(struct service_symbs *ss, struct component_init_str *data)
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
make_spd_config_comp(struct service_symbs *c, struct service_symbs *all)
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

		/* Output cobject file. */
		char filename[MAX_FILE_NAME_LEN] = { '\0' };
		sprintf(filename, "%s.co", &all->obj[5]);
		printl(PRINT_HIGH, "Outputting co to file %s\n", filename);
		int co_fd = open(filename, O_WRONLY | O_CREAT);
		if (write(co_fd, h, h->size) == -1) {
			printl(PRINT_HIGH, "Error outputting co.\n");
		}
		close(co_fd);
	}

	all = first;
	*heap_ptr = (int*)(round_up_to_page((int)*heap_ptr));
	ci->cos_poly[1] = (vaddr_t)n;

	ci->cos_poly[2] = ((unsigned int)*heap_ptr);
	make_spd_mpd_mgr(boot, all);

	ci->cos_poly[3] = ((unsigned int)*heap_ptr);
	make_spd_config_comp(boot, all);
	llboot_mem = (unsigned int)*heap_ptr - boot->lower_addr;
	boot->heap_top = (unsigned int)*heap_ptr; /* ensure that we copy all of the meta-data as well */
}

struct service_symbs *
find_obj_by_name(struct service_symbs *s, const char *n)
{
	while (s) {
		if (!strncmp(&s->obj[5], n, strlen(n))) {
			return s;
		}

		s = s->next;
	}

	return NULL;
}

void
output_image(struct service_symbs *services)
{
	struct service_symbs *s;
        u32_t entry_point = 0;
        char image_filename[18];
        int image;
        u32_t image_base;

	s = services;
	while (s) {
		struct service_symbs *t;
		struct spd_info *t_spd;

		t = s;
		if (!is_booter_loaded(s)) {
			if (strstr(s->obj, INIT_COMP) != NULL) {
				t_spd = create_spd(t, 0, 0);
			} else {
				t_spd = create_spd(t, t->lower_addr, t->size);
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
			if (create_spd_capabilities(s)) {
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

	if ((s = find_obj_by_name(services, LLBOOT_COMP))) {
		make_spd_llboot(s, services);
		make_spd_scheduler(s, NULL);
	}

	assert(s);
	image_base = s->lower_addr;
	printl(PRINT_DEBUG, "Image base is 0x%08x\n", image_base);

	entry_point = get_symb_address(&s->exported, "cos_upcall_entry");
	printl(PRINT_DEBUG, "Entry point is at 0x%08x\n", entry_point);

        sprintf(image_filename, "%08x-%08x", image_base, entry_point);
        printl(PRINT_HIGH, "Writing image %s (%u bytes)\n", image_filename, s->heap_top - image_base);
        image = open(image_filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
        if (write(image, (void*)image_base, s->heap_top - image_base) < 0) {
                printl(PRINT_DEBUG, "Error number %d\n", errno);
                perror("Couldn't write image");
                exit(-1);
        }
        close(image);

	return;
}
