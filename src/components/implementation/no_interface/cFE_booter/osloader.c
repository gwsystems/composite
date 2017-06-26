#include "cFE_util.h"

#include "gen/osapi.h"
#include "gen/common_types.h"

struct module_internal_record {
   int                 free;
   cpuaddr             entry_point;
   uint32              host_module_id;
   char                filename[OS_MAX_PATH_LEN];
   char                name[OS_MAX_API_NAME];
};

struct module_internal_record module_table[OS_MAX_MODULES];

/*
** Loader API
*/
int32 OS_ModuleTableInit(void)
{
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

int32 OS_SymbolLookup(cpuaddr *symbol_address, const char *symbol_name)
{
    /* Check parameters. */
    if (symbol_address == NULL || symbol_name == NULL) {
        return OS_INVALID_POINTER;
    }

    /* TODO: Look up entrypoint. */

    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_SymbolTableDump(const char *filename, uint32 size_limit)
{
    /* Not needed. */
    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_ModuleLoad(uint32 *module_id, const char *module_name, const char *filename)
{
    uint32 i;
    uint32 possible_id;
    char translated_path[OS_MAX_PATH_LEN];

    /* Check parameters. */
    if (module_id == NULL || module_name == NULL || filename == NULL) {
        return OS_INVALID_POINTER;
    }

    /* Find a free id. */
    for (possible_id = 0; possible_id < OS_MAX_MODULES; possible_id++) {
        if (module_table[possible_id].free == TRUE) break;
    }

    /* Check bounds of that id. */
    if (possible_id >= OS_MAX_MODULES || module_table[possible_id].free == FALSE) {
        return OS_ERR_NO_FREE_IDS;
    }

    /* Check if the module was already loaded. */
    for (i = 0; i < OS_MAX_MODULES; i++) {
        if (module_table[i].free == FALSE && strcmp(module_name, module_table[i].name) == 0) {
            return OS_ERR_NAME_TAKEN;
        }
    }

    /* Claim the module id. */
    module_table[possible_id].free = FALSE;

    /* Translate the filename. */
    int32 return_code = OS_TranslatePath(filename, (char *)translated_path);
    if (return_code != OS_SUCCESS) {
        module_table[possible_id].free = TRUE;
        return return_code;
    }

    /* TODO: Load the module. */

    return OS_ERR_NOT_IMPLEMENTED;
}

int32 OS_ModuleUnload(uint32 module_id)
{
    /* Check the given id. */
    if (module_id >= OS_MAX_MODULES || module_table[module_id].free == TRUE) {
        return OS_ERR_INVALID_ID;
    }

    /* TODO: Unload module. */

    module_table[module_id].free = TRUE;

    return OS_ERR_NOT_IMPLEMENTED;
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

    /* TODO: Address info? */

    return OS_ERR_NOT_IMPLEMENTED;
}
