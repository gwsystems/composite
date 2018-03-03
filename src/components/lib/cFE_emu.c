#include <stdarg.h>

#include <llprint.h>
#include "../interface/resmgr/memmgr.h"

#include <cfe_error.h>
#include <cfe_evs.h>

#include <cFE_emu.h>

union shared_region *shared_region;
spdid_t spdid;

void do_emulation_setup(spdid_t id)
{
    spdid = id;

    int region_id = emu_backend_request_memory(id);

    vaddr_t client_addr = 0;
    memmgr_shared_page_map(region_id, &client_addr);
    assert(client_addr);
    shared_region = (void*)client_addr;
}


// FIXME: Query the cFE to decide whether printf is enabled
int is_printf_enabled = 1;

void OS_printf(const char *string, ...)
{
    if(is_printf_enabled) {
        char s[OS_BUFFER_SIZE];
        va_list arg_ptr;
        int ret, len = OS_BUFFER_SIZE;

        va_start(arg_ptr, string);
        ret = vsnprintf(s, len, string, arg_ptr);
        va_end(arg_ptr);
        cos_llprint(s, ret);
    }
}


int32 CFE_EVS_Register (void                 *Filters,           /* Pointer to an array of filters */
                        uint16               NumFilteredEvents,  /* How many elements in the array? */
                        uint16               FilterScheme)      /* Filtering Algorithm to be implemented */
{
    if (FilterScheme != CFE_EVS_BINARY_FILTER)
    {
       return CFE_EVS_UNKNOWN_FILTER;
    }
    CFE_EVS_BinFilter_t *bin_filters = Filters;
    int i;
    for (i = 0; i < NumFilteredEvents; i++) {
        shared_region->cfe_evs_register.filters[i] = bin_filters[i];
    }
    shared_region->cfe_evs_register.NumEventFilters = NumFilteredEvents;
    shared_region->cfe_evs_register.FilterScheme = FilterScheme;
    return emu_CFE_EVS_Register(spdid);
}

int32  CFE_SB_CreatePipe(CFE_SB_PipeId_t *PipeIdPtr,
                         uint16  Depth,
                         const char *PipeName)
{
    shared_region->cfe_sb_createPipe.Depth = Depth;
    strncpy(shared_region->cfe_sb_createPipe.PipeName, PipeName, OS_MAX_API_NAME);
    int32 ret = emu_CFE_SB_CreatePipe(spdid);
    *PipeIdPtr = shared_region->cfe_sb_createPipe.PipeId;
    return ret;
}

void CFE_SB_InitMsg(void           *MsgPtr,
                    CFE_SB_MsgId_t MsgId,
                    uint16         Length,
                    boolean        Clear )
{
    char *source = MsgPtr;
    assert(Length <= EMU_BUF_SIZE);
    memcpy(shared_region->cfe_sb_initMsg.MsgBuffer, source, Length);
    shared_region->cfe_sb_initMsg.MsgId = MsgId;
    shared_region->cfe_sb_initMsg.Length = Length;
    shared_region->cfe_sb_initMsg.Clear = Clear;
    emu_CFE_SB_InitMsg(spdid);
    memcpy(source, shared_region->cfe_sb_initMsg.MsgBuffer, Length);
}

int32 CFE_EVS_SendEvent( uint16 EventID,
                         uint16 EventType,
                         const char *Spec, ... )
{
    va_list Ptr;
    va_start(Ptr, Spec);
    vsnprintf(shared_region->cfe_evs_sendEvent.Msg, sizeof(shared_region->cfe_evs_sendEvent.Msg), Spec, Ptr);
    va_end(Ptr);

    shared_region->cfe_evs_sendEvent.EventID = EventID;
    shared_region->cfe_evs_sendEvent.EventType = EventType;

    return emu_CFE_EVS_SendEvent(spdid);
}

int32 CFE_ES_RunLoop(uint32 *RunStatus)
{
    shared_region->cfe_es_runLoop.RunStatus = *RunStatus;
    int32 result = emu_CFE_ES_RunLoop(spdid);
    *RunStatus = shared_region->cfe_es_runLoop.RunStatus;
    return result;
}

/*
 * We want the msg to live in this app, not the cFE component
 * But the message is stored in a buffer on the cFE side
 * The slution is to copy it into a buffer here
 * According to the cFE spec, this buffer only needs to last till the pipe is used again
 * Therefore one buffer per pipe is acceptable
 */
struct {
    char buf[EMU_BUF_SIZE];
} pipe_buffers[CFE_SB_MAX_PIPES];

int32  CFE_SB_RcvMsg(CFE_SB_MsgPtr_t    *BufPtr,
                     CFE_SB_PipeId_t    PipeId,
                     int32              TimeOut)
{
    shared_region->cfe_sb_rcvMsg.PipeId = PipeId;
    shared_region->cfe_sb_rcvMsg.TimeOut = TimeOut;
    int32 result = emu_CFE_SB_RcvMsg(spdid);
    if (result == CFE_SUCCESS) {
        memcpy(pipe_buffers[PipeId].buf, shared_region->cfe_sb_rcvMsg.Msg, EMU_BUF_SIZE);
        *BufPtr = (CFE_SB_MsgPtr_t)pipe_buffers[PipeId].buf;
    }
    return result;
}

uint16 CFE_SB_GetTotalMsgLength(CFE_SB_MsgPtr_t MsgPtr)
{
    shared_region->cfe_sb_getMsgLen.Msg = *MsgPtr;
    return emu_CFE_SB_GetTotalMsgLength(spdid);
}

int32 CFE_SB_SendMsg(CFE_SB_Msg_t   *MsgPtr)
{
    uint16 msg_len = CFE_SB_GetTotalMsgLength(MsgPtr);
    assert(msg_len <= EMU_BUF_SIZE);
    char *msg_ptr = (char*)MsgPtr;
    memcpy(shared_region->cfe_sb_msg.Msg, msg_ptr, (size_t)msg_len);
    return emu_CFE_SB_SendMsg(spdid);
}

uint16 CFE_SB_GetCmdCode(CFE_SB_MsgPtr_t MsgPtr)
{
    uint16 msg_len = CFE_SB_GetTotalMsgLength(MsgPtr);
    assert(msg_len <= EMU_BUF_SIZE);
    char *msg_ptr = (char*)MsgPtr;
    memcpy(shared_region->cfe_sb_msg.Msg, msg_ptr, (size_t)msg_len);
    return emu_CFE_SB_GetCmdCode(spdid);
}

CFE_SB_MsgId_t CFE_SB_GetMsgId(CFE_SB_MsgPtr_t MsgPtr)
{
    uint16 msg_len = CFE_SB_GetTotalMsgLength(MsgPtr);
    assert(msg_len <= EMU_BUF_SIZE);
    char *msg_ptr = (char*)MsgPtr;
    memcpy(shared_region->cfe_sb_msg.Msg, msg_ptr, (size_t)msg_len);
    return emu_CFE_SB_GetMsgId(spdid);
}

void CFE_SB_TimeStampMsg(CFE_SB_MsgPtr_t MsgPtr)
{
    uint16 msg_len = CFE_SB_GetTotalMsgLength(MsgPtr);
    assert(msg_len <= EMU_BUF_SIZE);
    char *msg_ptr = (char*)MsgPtr;
    memcpy(shared_region->cfe_sb_msg.Msg, msg_ptr, (size_t)msg_len);
    emu_CFE_SB_TimeStampMsg(spdid);
    memcpy(msg_ptr, shared_region->cfe_sb_msg.Msg, (size_t)msg_len);
}
