#include <stdlib.h>

#include <cos_kernel_api.h>
#include <cos_defkernel_api.h>
#include <cos_types.h>
#include <llprint.h>

#include <elf_loader.h>
#include <initargs.h>

#ifndef BOOTER_MAX_SINV
#define BOOTER_MAX_SINV 256
#endif
#ifndef INITARGS_MAX_PATHNAME
#define INITARGS_MAX_PATHNAME 512
#endif

typedef enum {
	CRT_COMP_SCHED       = 1, 	/* is this a scheduler? */
	CRT_COMP_DELEG       = 1<<1,	/* does this component require delegating management capabilities to it? */
	CRT_COMP_DERIVED     = 1<<2, 	/* derived from another component */
	CRT_COMP_SCHED_DELEG = 1<<4,	/* is the system thread initialization delegated to this component? */
} crt_comp_flags_t;

struct crt_comp {
	crt_comp_flags_t flags;
	char *name;
	compid_t id;
	vaddr_t entry_addr, ro_addr, rw_addr;

	char *mem;		/* image memory */
	struct elf_hdr *elf_hdr;
	struct cos_defcompinfo comp_res;
};

void
crt_comp_init(struct crt_comp *comp, char *name, compid_t id)
{
	*comp = (struct crt_comp) {
		.name = name,
		.id   = id,
	};
}

/*
 * Create the component from the elf object including all the resource
 * tables, and memory.
 *
 * Notes:
 * - The capability tables are empty.
 * - `name` is *not* copied, so it is borrowed from within `c`. Copy
 *   it manually if you can't guarantee it will stay alive.
 *
 * Return 0 on success, -errno on failure -- either of elf parsing, or
 * of memory allocation.
 */
int
crt_comp_construct(struct crt_comp *c, char *name, compid_t id, struct elf_hdr *elf_hdr)
{
	size_t  ro_sz,   rw_sz, data_sz, bss_sz, tot_sz;
	char   *ro_src, *data_src, *mem;
	int     ret;
	struct cos_compinfo *ci, *root_ci;

	c->name       = name;
	c->id         = id;
	c->elf_hdr    = elf_hdr;
	c->entry_addr = elf_entry_addr(elf_hdr);

	ci            = cos_compinfo_get(&c->comp_res);
	root_ci       = cos_compinfo_get(cos_defcompinfo_curr_get());

	if (elf_load_info(c->elf_hdr, &c->ro_addr, &ro_sz, &ro_src, &c->rw_addr, &data_sz, &data_src, &bss_sz)) return -EINVAL;

	printc("\t\tElf object: ro [0x%lx, 0x%lx), data [0x%lx, 0x%lx), bss [0x%lx, 0x%lx).\n",
	       c->ro_addr, c->ro_addr + ro_sz, c->rw_addr, c->rw_addr + data_sz, c->rw_addr + data_sz, c->rw_addr + data_sz + bss_sz);

	ret = cos_compinfo_alloc(ci, c->ro_addr, BOOT_CAPTBL_FREE, c->entry_addr, root_ci);
	assert(!ret);

	tot_sz = round_up_to_page(round_up_to_page(ro_sz) + data_sz + bss_sz);
	mem    = cos_page_bump_allocn(root_ci, tot_sz);
	if (!mem) return -ENOMEM;
	c->mem = mem;

	memcpy(mem, ro_src, ro_sz);
	memcpy(mem + round_up_to_page(ro_sz), data_src, data_sz);
	memset(mem + round_up_to_page(ro_sz) + data_sz, 0, bss_sz);

	/* FIXME: separate map of RO and RW */
	if (c->ro_addr != cos_mem_aliasn(ci, root_ci, (vaddr_t)mem, tot_sz)) return -ENOMEM;

	return 0;
}

struct crt_sinv {
	char *name;
	struct crt_comp *server, *client;
	vaddr_t c_fn_addr, c_ucap_addr;
	vaddr_t s_fn_addr;
	sinvcap_t sinv_cap;
};

void
crt_sinv_init(struct crt_sinv *sinv, char *name, struct crt_comp *server, struct crt_comp *client,
	      vaddr_t c_fn_addr, vaddr_t c_ucap_addr, vaddr_t s_fn_addr)
{
	*sinv = (struct crt_sinv) {
		.name        = name,
		.server      = server,
		.client      = client,
		.c_fn_addr   = c_fn_addr,
		.c_ucap_addr = c_ucap_addr,
		.s_fn_addr   = s_fn_addr
	};

	return;
}

int
crt_sinv_construct(struct crt_sinv *sinv)
{
	struct cos_compinfo *cli = cos_compinfo_get(&sinv->client->comp_res);
	struct cos_compinfo *srv = cos_compinfo_get(&sinv->server->comp_res);
	unsigned int ucap_off;
	struct usr_inv_cap *ucap;

	assert(cli && srv && srv->comp_cap);

	sinv->sinv_cap = cos_sinv_alloc(cli, srv->comp_cap, sinv->s_fn_addr, sinv->client->id);
	assert(sinv->sinv_cap);

	/* poor-mans virtual address translation from client VAS -> our ptrs */
	ucap_off = sinv->c_ucap_addr - sinv->client->ro_addr;
	ucap = (struct usr_inv_cap *)(sinv->client->mem + ucap_off);
	*ucap = (struct usr_inv_cap) {
		.invocation_fn = sinv->c_fn_addr,
		.cap_no        = sinv->sinv_cap,
		.data          = NULL
	};

	return 0;
}


/* Booter's additive information about the component */
struct boot_comp {
	struct crt_comp comp;
};
static struct boot_comp boot_comps[MAX_NUM_COMPS];
/* All synchronous invocations */
static struct crt_sinv  boot_sinvs[BOOTER_MAX_SINV];

static void
comps_init(void)
{
	struct initargs comps, curr;
	struct initargs_iter i;
	int cont, ret, j;
	int comp_idx = 0, sinv_idx = 0;

	ret = args_get_entry("components", &comps);
	assert(!ret);

	printc("Components (and initialization schedule):\n");
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct crt_comp *comp;
		struct elf_hdr  *elf;
		int   keylen;
		int   idx  = atoi(args_key(&curr, &keylen));
		char *name = args_value(&curr);
		char *root = "binaries/";
		int   len  = strlen(root);
		char  path[INITARGS_MAX_PATHNAME];

		assert(idx < MAX_NUM_COMPS && idx > 0 && name);

		strcpy(path, root);
		strncat(path, name, INITARGS_MAX_PATHNAME - len);

		comp = &boot_comps[idx-1].comp; /* there is no component id 0 */
		if (crt_comp_construct(comp, name, idx, (struct elf_hdr *)args_get(path))) {
			printc("Error constructing the resource tables and image of component %s.\n", comp->name);
			BUG();
		}
		assert(cos_compinfo_get(&comp->comp_res)->comp_cap);
	}

	ret = args_get_entry("sinvs", &comps);
	assert(!ret);

	printc("Synchronous invocations (%d):\n", args_len(&comps));
	for (cont = args_iter(&comps, &i, &curr) ; cont ; cont = args_iter_next(&i, &curr)) {
		struct crt_sinv *sinv;
		int serv_idx = atoi(args_get_from("server", &curr));
		int cli_idx  = atoi(args_get_from("client", &curr));

		assert(sinv_idx < BOOTER_MAX_SINV);
		sinv = &boot_sinvs[sinv_idx];
		sinv_idx++;	/* bump pointer allocation */

		crt_sinv_init(sinv, args_get_from("name", &curr), &boot_comps[serv_idx].comp, &boot_comps[cli_idx].comp,
			      strtoul(args_get_from("c_fn_addr", &curr), NULL, 10), strtoul(args_get_from("c_ucap_addr", &curr), NULL, 10),
			      strtoul(args_get_from("s_fn_addr", &curr), NULL, 10));

		printc("\t%s (%d->%d):\tclient_fn @ 0x%lx, client_ucap @ 0x%lx, server_fn @ 0x%lx\n",
		       sinv->name, sinv->client->id, sinv->server->id, sinv->c_fn_addr, sinv->c_ucap_addr, sinv->s_fn_addr);

		crt_sinv_construct(sinv);
	}

	printc("DONE!\n");

	return;
}

static void
booter_init(void)
{
	struct cos_compinfo *boot_info = cos_compinfo_get(cos_defcompinfo_curr_get());

	printc("Heap pointer @ %p\n", cos_get_heap_ptr());
	cos_meminfo_init(&(boot_info->mi), BOOT_MEM_KM_BASE, COS_MEM_KERN_PA_SZ, BOOT_CAPTBL_SELF_UNTYPED_PT);
	cos_defcompinfo_init();
}

void
cos_init(void)
{
	booter_init();
	comps_init();

	while (1) ;
}
