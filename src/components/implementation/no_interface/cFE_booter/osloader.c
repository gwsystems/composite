#include <stdint.h>

#include <cobj_format.h>
#include <cos_defkernel_api.h>

#include "cFE_util.h"

#include "gen/osapi.h"
#include "gen/common_types.h"
#include "gen/cfe_es.h"
#include "gen/cfe_evs.h"
#include "gen/cfe_tbl.h"

#define USER_CAPS_SYMB_NAME "ST_user_caps"

int cobj_count;
struct cobj_header *hs[MAX_NUM_SPDS+1];

static void
find_cobjs(struct cobj_header *h, int n)
{
	int i;
	vaddr_t start, end;

	start = (vaddr_t)h;

    printc("First cobj is '%s'.\n", h->name);

    hs[0] = h;

	for (i = 1 ; i < n ; i++) {
		int j = 0, size = 0, tot = 0;

		size = h->size;
		for (j = 0 ; j < (int)h->nsect ; j++) {
			tot += cobj_sect_size(h, j);
		}

		printc("cobj %s:%d found at %p:%x, size %x -> %x\n",
			   h->name, h->id, hs[i-1], size, tot, cobj_sect_get(hs[i-1], 0)->vaddr);

		end   = start + round_up_to_cacheline(size);
		hs[i] = h = (struct cobj_header*)end;
		start = end;
	}

	hs[n] = NULL;
	cobj_count = i;

    printc("cobj %s:%d found at %p -> %x\n",
		   hs[n-1]->name, hs[n-1]->id, hs[n-1], cobj_sect_get(hs[n-1], 0)->vaddr);

}

struct module_internal_record {
   int                 free;
   cpuaddr             entry_point;
   uint32              host_module_id;
   char                filename[OS_MAX_PATH_LEN];
   char                name[OS_MAX_API_NAME];
   struct cobj_header *header;
};

struct module_internal_record module_table[OS_MAX_MODULES];

#define INIT_STR_SZ 52
struct component_init_str {
        unsigned int spdid, schedid;
        int startup;
        char init_str[INIT_STR_SZ];
}__attribute__((packed));

struct component_init_str *init_args;

int32 OS_ModuleTableInit(void)
{
    struct cobj_header *h = (struct cobj_header *)cos_comp_info.cos_poly[0];

    int num_cobj  = (int)cos_comp_info.cos_poly[1];

    init_args = (struct component_init_str *)cos_comp_info.cos_poly[3];
    init_args++;

    find_cobjs(h, num_cobj);

    uint32 i;
    for (i = 0; i < OS_MAX_MODULES; i++) {
        module_table[i].free = TRUE;
        module_table[i].entry_point = 0;
        module_table[i].host_module_id = 0;
        strcpy(module_table[i].name, "");
        strcpy(module_table[i].filename, "");
    }

    /* TODO: Module table mutex. */

    return OS_ERR_NOT_IMPLEMENTED;
}

struct user_cap {
    void (*invfn)(void);
    int entryfn, invcount, capnum;
} __attribute__((packed));

struct symbol_of_jank {
    char name[64];
    void *fn;
};

struct symbol_of_jank soj[] = {
	{ .name = "OS_printf", .fn = OS_printf },
    { .name = "OS_close", .fn = OS_close },
    { .name = "OS_open", .fn = OS_open },
    { .name = "OS_TaskInstallDeleteHandler", .fn = OS_TaskInstallDeleteHandler },
    { .name = "OS_TaskDelay", .fn = OS_TaskDelay },
    { .name = "CFE_ES_ExitApp", .fn = CFE_ES_ExitApp },
    { .name = "CFE_ES_PerfLogAdd", .fn = CFE_ES_PerfLogAdd },
    { .name = "CFE_ES_RegisterApp", .fn = CFE_ES_RegisterApp },
    { .name = "CFE_EVS_Register", .fn = CFE_EVS_Register },
    { .name = "CFE_EVS_SendEvent", .fn = CFE_EVS_SendEvent },
    { .name = "CFE_SB_CreatePipe", .fn = CFE_SB_CreatePipe },
    { .name = "CFE_SB_GetCmdCode", .fn = CFE_SB_GetCmdCode },
    { .name = "CFE_SB_GetMsgId", .fn = CFE_SB_GetMsgId },
    { .name = "CFE_SB_GetTotalMsgLength", .fn = CFE_SB_GetTotalMsgLength },
    { .name = "CFE_SB_RcvMsg", .fn = CFE_SB_RcvMsg },
    { .name = "CFE_SB_SendMsg", .fn = CFE_SB_SendMsg },
    { .name = "CFE_SB_Subscribe", .fn = CFE_SB_Subscribe },
    { .name = "CFE_SB_TimeStampMsg", .fn = CFE_SB_TimeStampMsg },
    { .name = "CFE_ES_RunLoop", .fn = CFE_ES_RunLoop },
    { .name = "CFE_SB_InitMsg", .fn = CFE_SB_InitMsg },
    { .name = "CFE_SB_MessageStringGet", .fn = CFE_SB_MessageStringGet },
    { .name = "CFE_SB_SubscribeEx", .fn = CFE_SB_SubscribeEx },
    { .name = "CFE_SB_Unsubscribe", .fn = CFE_SB_Unsubscribe },
    { .name = "CFE_ES_RegisterChildTask", .fn = CFE_ES_RegisterChildTask },
    { .name = "CFE_ES_CalculateCRC", .fn = CFE_ES_CalculateCRC },
    { .name = "CFE_ES_CopyToCDS", .fn = CFE_ES_CopyToCDS },
    { .name = "CFE_ES_CreateChildTask", .fn = CFE_ES_CreateChildTask },
    { .name = "CFE_ES_DeleteChildTask", .fn = CFE_ES_DeleteChildTask },
    { .name = "CFE_ES_ExitChildTask", .fn = CFE_ES_ExitChildTask },
    { .name = "CFE_ES_GetAppID", .fn = CFE_ES_GetAppID },
    { .name = "CFE_ES_GetAppIDByName", .fn = CFE_ES_GetAppIDByName },
    { .name = "CFE_ES_GetAppInfo", .fn = CFE_ES_GetAppInfo },
    { .name = "CFE_ES_GetAppName", .fn = CFE_ES_GetAppName },
    { .name = "CFE_ES_RegisterCDS", .fn = CFE_ES_RegisterCDS },
    { .name = "CFE_ES_RegisterChildTask", .fn = CFE_ES_RegisterChildTask },
    { .name = "CFE_ES_RestoreFromCDS", .fn = CFE_ES_RestoreFromCDS },
    { .name = "CFE_ES_WaitForStartupSync", .fn = CFE_ES_WaitForStartupSync },
    { .name = "CFE_ES_WriteToSysLog", .fn = CFE_ES_WriteToSysLog },
    { .name = "CFE_PSP_GetCFETextSegmentInfo", .fn = CFE_PSP_GetCFETextSegmentInfo },
    { .name = "CFE_PSP_GetKernelTextSegmentInfo", .fn = CFE_PSP_GetKernelTextSegmentInfo },
    { .name = "CFE_PSP_MemCpy", .fn = CFE_PSP_MemCpy },
    { .name = "CFE_PSP_MemSet", .fn = CFE_PSP_MemSet },
    { .name = "CFE_PSP_MemValidateRange", .fn = CFE_PSP_MemValidateRange },
    { .name = "CFE_TBL_GetAddress", .fn = CFE_TBL_GetAddress },
    { .name = "CFE_TBL_GetInfo", .fn = CFE_TBL_GetInfo },
    { .name = "CFE_TBL_Load", .fn = CFE_TBL_Load },
    { .name = "CFE_TBL_Manage", .fn = CFE_TBL_Manage },
    { .name = "CFE_TBL_Modified", .fn = CFE_TBL_Modified },
    { .name = "CFE_TBL_Register", .fn = CFE_TBL_Register },
    { .name = "CFE_TBL_ReleaseAddress", .fn = CFE_TBL_ReleaseAddress },
    { .name = "CFE_TBL_Share", .fn = CFE_TBL_Share },
    { .name = "CFE_TBL_Unregister", .fn = CFE_TBL_Unregister },
    { .name = "", .fn = NULL },
};

struct symbol_of_jank* lookup_symbol_in_soj(const char *symb_name)
{
    size_t i;

    for (i = 0; soj[i].fn != NULL; i++) {
        if (!strcmp(symb_name, soj[i].name)) {
            /* We have found the matching SOJ. */

            printc("Found matching SOJ @ %p for undef '%s'.\n", soj[i].fn, symb_name);

            return &soj[i];
        }
    }

    return &soj[i];
}

/*
** Loader API
*/


struct cobj_header *get_cobj_header(const char* path)
{
	char name[OS_MAX_PATH_LEN];


    printc("cobj: Object path is %s\n", path);

	int slash_index;
	for (slash_index = strlen(path); path[slash_index] != '/' && slash_index != 0; slash_index--) {
	}

    if (slash_index == 0) {
		PANIC("Could not find slash in object name, aborting...");
	}

	/* We just want the name after the slash_index */
	strcpy(name, path + slash_index + 1);

    printc("cobj: Object name appears to be %s\n", name);

	/* But before the '.' */
	int dot_index;
	for (dot_index = 0; name[dot_index] != '.' && name[dot_index] != '\0'; dot_index++) {
	}

	if (name[dot_index] == '\0') {
		PANIC("Invalid object name, aborting...");
	}

	name[dot_index] = '\0';

    printc("cobj: Trimmed object name appears to be %s\n", name);

	int cobj_index;
	for (cobj_index = 0; hs[cobj_index] != NULL; cobj_index++) {
		if (!strcmp(hs[cobj_index]->name, name)) {
			return hs[cobj_index];
		}
	}
    return NULL;
}

struct user_cap *find_user_caps(struct cobj_header *h)
{
    size_t i;

    for (i = 0; i < h->nsymb; i++) {
        struct cobj_symb *curr = cobj_symb_get(h, i);
        if (!strcmp(curr->name, USER_CAPS_SYMB_NAME)) {

            printc("cobj: found user caps array '%s' @ %x.\n", USER_CAPS_SYMB_NAME, curr->vaddr);


            /* Set to the first user cap in the array. */
            return (struct user_cap *) (intptr_t) curr->vaddr;
        }
    }
    PANIC("Could not find user capability array!\n");
    return NULL;
}

void inspect_cobj_symbols(struct cobj_header *h, vaddr_t *comp_info)
{
	unsigned int i;


    printc("cobj: getting spd symbs for header %s, nsymbs %d.\n", h->name, h->nsymb);


    for (i = 0 ; i < h->nsymb ; i++) {
		struct cobj_symb *symb;

		symb = cobj_symb_get(h, i);
		assert(symb);

		if (symb->type == COBJ_SYMB_UNDEF) {

            printc("cobj: undefined symbol %s: nsymb %d, usercap offset %d\n", symb->name, i, symb->user_caps_offset);

            continue;
		} else if (symb->type == COBJ_SYMB_EXPORTED) {

            printc("cobj: exported symbol %s: nsymb %d, addr %x\n", symb->name, i, symb->vaddr);

            continue;
		}

		switch (symb->type) {
		case COBJ_SYMB_COMP_INFO:

            printc("cobj: comp info %s: addr %x\n", symb->name, symb->vaddr);

            *comp_info = symb->vaddr;
			break;
		case COBJ_SYMB_COMP_PLT:
			/* Otherwise known as ST_user_caps. */

            printc("cobj: capability array %s: addr %x\n", symb->name, symb->vaddr);

            break;
		default:

            printc("boot: Unknown symbol type %d\n", symb->type);

            break;
		}
	}
}

static void
expand_pgtbl(int n_pte, pgtblcap_t pt, vaddr_t vaddr, struct cobj_header *h)
{

    printc("expand_pgtbl(%d, %lu, %lu, %p)\n", n_pte, pt, vaddr, h);

    int i;
	int tot = 0;

    struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
    struct cos_compinfo    *ci    = cos_compinfo_get(defci);

	/* Expand Page table, could do this faster */
	for (i = 0 ; i < (int)h->nsect ; i++) {
		tot += cobj_sect_size(h, i);
	}

	if (tot > SERVICE_SIZE) {
		n_pte = tot / SERVICE_SIZE;
		if (tot % SERVICE_SIZE) n_pte++;
	}

	for (i = 0 ; i < n_pte ; i++) {

        printc("cos_pgtbl_intern_alloc(%p, %lu, %lu, %d)\n", ci, pt, vaddr, SERVICE_SIZE);

        if (!cos_pgtbl_intern_alloc(ci, pt, vaddr, SERVICE_SIZE)) PANIC("BUG");
	}
}

static vaddr_t
map_cobj_section(vaddr_t dest_daddr)
{
    struct cos_defcompinfo *defci = cos_defcompinfo_curr_get();
    struct cos_compinfo    *ci    = cos_compinfo_get(defci);

	vaddr_t addr = (vaddr_t) cos_page_bump_alloc(ci);
	assert(addr);

	if (cos_mem_alias_at(ci, dest_daddr, ci, addr)) PANIC("BUG");

	return addr;
}

void map_cobj_memory(struct cobj_header *h, pgtblcap_t pt) {
    int i;
    int flag;
    vaddr_t dest_daddr, prev_map = 0;
    int n_pte = 1;
    struct cobj_sect *sect = cobj_sect_get(h, 0);


	printc("cobj: Expanding pgtbl\n");


    expand_pgtbl(n_pte, pt, sect->vaddr, h);

    /* NOTE: We just hardcode this, since we also want to map into this components memory` */
	/* We'll map the component into booter's heap. */
    // new_comp_cap_info[spdid].vaddr_mapped_in_booter = (vaddr_t)cos_get_heap_ptr();

    for (i = 0 ; i < (int)h->nsect ; i++) {
        int left;

        sect = cobj_sect_get(h, i);
        flag = MAPPING_RW;
        if (sect->flags & COBJ_SECT_KMEM) {
            flag |= MAPPING_KMEM;
        }

        dest_daddr = sect->vaddr;
        left       = cobj_sect_size(h, i);

        /* previous section overlaps with this one, don't remap! */
        if (round_to_page(dest_daddr) == prev_map) {
            left 	  -= (prev_map + PAGE_SIZE - dest_daddr);
            dest_daddr = prev_map + PAGE_SIZE;
        }

        while (left > 0) {
            map_cobj_section(dest_daddr);

            prev_map = dest_daddr;
            dest_daddr += PAGE_SIZE;
            left       -= PAGE_SIZE;
        }
    }
}

static int
process_cinfo(struct cobj_header *h, spdid_t spdid, vaddr_t heap_val,
		   char *mem, vaddr_t symb_addr)
{
	int i;
	struct cos_component_information *ci;

	assert(symb_addr == round_to_page(symb_addr));
	ci = (struct cos_component_information*)(mem);

	if (!ci->cos_heap_ptr) ci->cos_heap_ptr = heap_val;

	ci->cos_this_spd_id = spdid;
	ci->init_string[0]  = '\0';

	for (i = 0 ; init_args[i].spdid ; i++) {
		char *start, *end;
		int len;

		if (init_args[i].spdid != spdid) continue;

		start = strchr(init_args[i].init_str, '\'');
		if (!start) break;
		start++;
		end   = strchr(start, '\'');
		if (!end) break;
		len   = (int)(end-start);
		memcpy(&ci->init_string[0], start, len);
		ci->init_string[len] = '\0';
	}

	return 1;
}

static vaddr_t
find_end(struct cobj_header *h)
{
	struct cobj_sect *sect;
	int max_sect;

	max_sect = h->nsect-1;
	sect     = cobj_sect_get(h, max_sect);

	return sect->vaddr + round_up_to_page(sect->bytes);
}


void populate_cobj_memory(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info, int first_time) {
    unsigned int i;
    /* Where are we in the actual component's memory in the booter? */
    char *start_addr;
    /* Where are we in the destination address space? */
    vaddr_t init_daddr;

    // TODO: Verify this transformation is correct
    // start_addr = (char *)(new_comp_cap_info[spdid].vaddr_mapped_in_booter);
    start_addr = (char *) cos_get_heap_ptr();

    init_daddr = cobj_sect_get(h, 0)->vaddr;
    for (i = 0 ; i < h->nsect ; i++) {
        struct cobj_sect *sect;
        vaddr_t dest_daddr;
        char *lsrc;
        int left;

        sect	   = cobj_sect_get(h, i);
        /* virtual address in the destination address space */
        dest_daddr = sect->vaddr;
        /* where we're copying from in the cobj */
        lsrc	   = cobj_sect_contents(h, i);
        /* how much is left to copy? */
        left	   = cobj_sect_size(h, i);

        printc("Destination is %p\n", (void*) dest_daddr);

        /* Initialize memory. */
        if (!(sect->flags & COBJ_SECT_KMEM) &&
            (first_time || !(sect->flags & COBJ_SECT_INITONCE))) {
            if (sect->flags & COBJ_SECT_ZEROS) {
                    char * to = (char *) dest_daddr;

                    printc("Zeroing some memory %p (%d bytes)!\n", to, left);


                    // HACK: Should actually resolve this
                    // memset(start_addr + (dest_daddr - init_daddr), 0, left);
                    memset(to, 0, left);
            } else {
                char * to = (char *) dest_daddr;
                char * from = lsrc;

                printc("Setting some memory to %p from %p (%d bytes)!\n", to, from, left);

                // HACK: Should actually resolve this
                // memcpy(start_addr + (dest_daddr - init_daddr), lsrc, left);
                memcpy(to, from, left);
            }
        }

        if (sect->flags & COBJ_SECT_CINFO) {
            assert(left == PAGE_SIZE);
            assert(comp_info == dest_daddr);
            process_cinfo(h, spdid, find_end(h), start_addr + (comp_info-init_daddr), comp_info);
            // NOTE: This is useless, since we never upcall
			// struct cos_component_information *ci;
            // ci = (struct cos_component_information*)(start_addr + (comp_info-init_daddr));
            // new_comp_cap_info[h->id].upcall_entry = ci->cos_upcall_entry;

        }
    }

}


void setup_cobj_memory(struct cobj_header *h, spdid_t spdid, vaddr_t comp_info, pgtblcap_t pt)
{
	/* Allocating memory and mapping it to the booter's address space */

    printc("cobj: doing map_cobj_memory\n");

    map_cobj_memory(h, pt);


    printc("cobj: doing populate_cobj_memory\n");

    populate_cobj_memory(h, spdid, comp_info, 1);
}


void load_cobj_into_memory(struct cobj_header *h)
{
    vaddr_t ci = 0;
	pgtblcap_t pt = BOOT_CAPTBL_SELF_PT;
	spdid_t spdid = h->id;

    printc("cobj: Loading cobj with id %d, name %s\n", h->id, h->name);


    // NOTE: We end up not needing this information
	// struct cobj_sect *sect;
    // sect = cobj_sect_get(h, 0);
    // new_comp_cap_info[spdid].addr_start = sect->vaddr;


    inspect_cobj_symbols(h, &ci);

    if (ci == 0) {
        PANIC("Could not find component info in cobj!");
    }


    printc("Mapping cobj '%s'.\n", h->name);

    setup_cobj_memory(h, spdid, ci, pt);
}

void link_cobj(struct cobj_header *h, struct user_cap *user_caps)
{
    unsigned int i;


    printc("link: printing symbols of jank.\n");
    for (i = 0; soj[i].fn != NULL; i++) {
        printc("\tlink: symbol of jank %d '%s' with fn @ %p\n", i, soj[i].name, soj[i].fn);
    }

    /* Iterate through each symbol in the header. If it is undefined, index into the user caps array and set the `invfn`. */
    printc("link: parsing symbols for cobj header '%s'.\n", h->name);

    for (i = 0; i < h->nsymb; i++) {
        struct cobj_symb *symb = cobj_symb_get(h, i);
        assert(symb);

        if (symb->type == COBJ_SYMB_UNDEF) {

            printc("link: found undefined symbol '%s': nsymb %u, usercap offset %d\n", symb->name, i, symb->user_caps_offset);

			// FIXME: Figure out if this is an ok way to do this
            // struct symbol_of_jank *symbol = lookup_symbol_in_soj(symb->name);
			cpuaddr addr;
			int result = OS_SymbolLookup(&addr, symb->name);
			if (result != OS_SUCCESS) {

                printc("link: ERROR: could not find matching symbol for '%s'.\n", symb->name);

                PANIC("Cannot resolve symbol!");
			}

			struct user_cap cap = (struct user_cap) {
				.invfn = (void *) addr
			};


            printc("link: setting user cap index %d invfn @ %p\n", symb->user_caps_offset, cap.invfn);

            user_caps[symb->user_caps_offset] = cap;
        }
    }


    printc("link: done parsing symbols for cobj header '%s'.\n", h->name);

}

int32 OS_ModuleLoad(uint32 *module_id, const char *module_name, const char *filename)
{

	printc("OS_ModuleLoad start\n");

    uint32 i;
    uint32 possible_id;

    /* Check parameters. */
    if (module_id == NULL || module_name == NULL || filename == NULL) {
        return OS_INVALID_POINTER;
    }

	printc("OS_ModuleLoad %s %s\n", module_name, filename);

    /* Find a free id. */
    for (possible_id = 0; possible_id < OS_MAX_MODULES; possible_id++) {
        if (module_table[possible_id].free == TRUE) break;
    }
    /* Check bounds of that id. */
    if (possible_id >= OS_MAX_MODULES || module_table[possible_id].free == FALSE) {

        printc("OS_ERR_NO_FREE_IDS\n");

        return OS_ERR_NO_FREE_IDS;
    }

    /* Check if the module was already loaded. */
    for (i = 0; i < OS_MAX_MODULES; i++) {
        if (module_table[i].free == FALSE && strcmp(module_name, module_table[i].name) == 0) {

            printc("OS_ERR_NAME_TAKEN\n");

            return OS_ERR_NAME_TAKEN;
        }
    }

	struct cobj_header *h = get_cobj_header(filename);
	if (!h) {

        printc("Could not find cobj for designated object %s!\n", filename);

        return OS_ERROR;
	}

    /* Claim the module id. */
    module_table[possible_id].free = FALSE;

	module_table[possible_id].header = h;

    struct user_cap *caps = find_user_caps(h);

	// Load + link the cobj
    load_cobj_into_memory(h);
    link_cobj(h, caps);


	printc("osloader: Loading finished successfully, returning OS_SUCCESS\n");

    return OS_SUCCESS;
}

cpuaddr search_cobj_for_symbol(struct cobj_header *h, const char *symbol_name)
{
	unsigned int i;

	for (i = 0 ; i < h->nsymb ; i++) {
		struct cobj_symb *symb;

		symb = cobj_symb_get(h, i);
		assert(symb);


		// TODO: Check if we need to do more work for certain symbol types (eg. undefined symbols)
		if (!strcmp(symb->name, symbol_name) && symb->type == COBJ_SYMB_EXPORTED) {
			return (cpuaddr) symb->vaddr;
		}
	}
	return 0;
}

int32 OS_SymbolLookup(cpuaddr *symbol_address, const char *symbol_name)
{

	printc("osloader: doing symbol lookup for %s\n", symbol_name);

    /* Check parameters. */
    if (symbol_address == NULL || symbol_name == NULL) {
        return OS_INVALID_POINTER;
    }

	struct symbol_of_jank *jank_symbol = lookup_symbol_in_soj(symbol_name);
	if (jank_symbol->fn != NULL) {
		*symbol_address = (cpuaddr) jank_symbol->fn;

        printc("osloader: found soj for %s, address %p\n", symbol_name, (void *) jank_symbol->fn);

        return OS_SUCCESS;
	}

	int i;
	for(i = 0; i < OS_MAX_MODULES; i++) {
		if(!module_table[i].free) {
			cpuaddr addr = search_cobj_for_symbol(module_table[i].header, symbol_name);
			if (addr != 0) {

                printc("osloader: found cobj symbol for %s, address %p\n", symbol_name, (void *) addr);

                *symbol_address = addr;
				return OS_SUCCESS;
			}
		}
	}

    return OS_ERROR;
}


int32 OS_ModuleUnload(uint32 module_id)
{
    /* Check the given id. */
    if (module_id >= OS_MAX_MODULES || module_table[module_id].free == TRUE) {
        return OS_ERR_INVALID_ID;
    }

    // TODO: Verify that doing nothing here makes sense

    module_table[module_id].free = TRUE;

    return OS_SUCCESS;
}

int32 OS_ModuleInfo(uint32 module_id, OS_module_prop_t *module_info)
{
    if (module_info == NULL) {
        return OS_INVALID_POINTER;
    }

    if (module_id >= OS_MAX_MODULES || module_table[module_id].free == TRUE) {
        return OS_ERR_INVALID_ID;
    }

    module_info->entry_point = module_table[module_id].entry_point;
    module_info->host_module_id = module_table[module_id].host_module_id;
    strncpy(module_info->filename, module_table[module_id].filename, OS_MAX_API_NAME);
    strncpy(module_info->name, module_info[module_id].name, OS_MAX_API_NAME);

    return OS_SUCCESS;
}

int32 OS_SymbolTableDump(const char *filename, uint32 size_limit)
{
    /* Not needed. */
    return OS_ERR_NOT_IMPLEMENTED;
}
