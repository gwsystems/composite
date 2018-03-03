#include <cos_component.h>
#include <cos_types.h>

#include <memmgr.h>

#include <cFE_emu.h>

union shared_region *shared_regions[16];

int emu_backend_request_memory(spdid_t client)
{
    vaddr_t our_addr = 0;
    int id = memmgr_shared_page_alloc(cos_comp_info.cos_this_spd_id, &our_addr);
    assert(our_addr);
    shared_regions[client] = (void*)our_addr;
    return id;
}

int32 emu_CFE_EVS_Register(spdid_t client)
{
    union shared_region* s = shared_regions[client];
    return CFE_EVS_Register(s->cfe_evs_register.filters, s->cfe_evs_register.NumEventFilters, s->cfe_evs_register.FilterScheme);
}

int32 emu_CFE_SB_CreatePipe(spdid_t client)
{
    union shared_region* s = shared_regions[client];
    return CFE_SB_CreatePipe(&s->cfe_sb_createPipe.PipeId, s->cfe_sb_createPipe.Depth, s->cfe_sb_createPipe.PipeName);
}

void emu_CFE_SB_InitMsg(spdid_t client)
{
    union shared_region* s = shared_regions[client];
    CFE_SB_InitMsg(s->cfe_sb_initMsg.MsgBuffer, s->cfe_sb_initMsg.MsgId, s->cfe_sb_initMsg.Length, s->cfe_sb_initMsg.Clear);
}

int32 emu_CFE_EVS_SendEvent(spdid_t client)
{
    union shared_region* s = shared_regions[client];
    return CFE_EVS_SendEvent(s->cfe_evs_sendEvent.EventID, s->cfe_evs_sendEvent.EventType, "%s", s->cfe_evs_sendEvent.Msg);
}
