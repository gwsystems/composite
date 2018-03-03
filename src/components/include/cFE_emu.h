#include <cos_types.h>

#include <cfe_evs.h>

#define EMU_BUF_SIZE 512

union shared_region {
    struct {
        CFE_EVS_BinFilter_t filters[CFE_EVS_MAX_EVENT_FILTERS];
        uint16 NumEventFilters;
        uint16 FilterScheme;
    } cfe_evs_register;
    struct {
        CFE_SB_PipeId_t PipeId;
        uint16  Depth;
        char PipeName[OS_MAX_API_NAME];
    } cfe_sb_createPipe;
    struct {
        char MsgBuffer[EMU_BUF_SIZE];
        CFE_SB_MsgId_t MsgId;
        uint16 Length;
        boolean Clear;
    } cfe_sb_initMsg;
    struct {
        char Msg[EMU_BUF_SIZE];
        uint16 EventID;
        uint16 EventType;
    } cfe_evs_sendEvent;
    struct {
        uint32 RunStatus;
    } cfe_es_runLoop;
    struct {
        CFE_SB_PipeId_t PipeId;
        int32 TimeOut;
        char Msg[EMU_BUF_SIZE];
    } cfe_sb_rcvMsg;
    struct {
        CFE_SB_Msg_t Msg;
    } cfe_sb_getMsgLen;
    struct {
        char Msg[EMU_BUF_SIZE];
    } cfe_sb_msg;
};

int emu_backend_request_memory(spdid_t client);

int32 emu_CFE_EVS_Register(spdid_t sp);

int32 emu_CFE_SB_CreatePipe(spdid_t client);

void emu_CFE_SB_InitMsg(spdid_t client);

int32 emu_CFE_EVS_SendEvent(spdid_t client);

int32 emu_CFE_ES_RunLoop(spdid_t client);

int32 emu_CFE_SB_RcvMsg(spdid_t client);

uint16 emu_CFE_SB_GetTotalMsgLength(spdid_t client);

int32 emu_CFE_SB_SendMsg(spdid_t client);

uint16 emu_CFE_SB_GetCmdCode(spdid_t client);

CFE_SB_MsgId_t emu_CFE_SB_GetMsgId(spdid_t client);

void emu_CFE_SB_TimeStampMsg(spdid_t client);
