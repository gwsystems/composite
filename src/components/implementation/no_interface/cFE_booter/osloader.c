#include "cFE_util.h"

#include "gen/osapi.h"
#include "gen/common_types.h"

/*
** Loader API
*/
int32 OS_ModuleTableInit(void)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SymbolLookup(cpuaddr *symbol_address, const char *symbol_name)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_SymbolTableDump(const char *filename, uint32 size_limit)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ModuleLoad(uint32 *module_id, const char *module_name, const char *filename)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ModuleUnload(uint32 module_id)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}

int32 OS_ModuleInfo(uint32 module_id, OS_module_prop_t *module_info)
{
    PANIC("Unimplemented method!"); // TODO: Implement me!
    return 0;
}