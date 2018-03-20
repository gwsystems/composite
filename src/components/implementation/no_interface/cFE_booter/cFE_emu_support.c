#include <stdio.h>

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_types.h>

#include <memmgr.h>

#include <cfe_error.h>

#include <cFE_emu.h>

union shared_region *shared_regions[16];

int
emu_backend_request_memory(spdid_t client)
{
	vaddr_t our_addr = 0;
	int     id       = memmgr_shared_page_alloc(&our_addr);
	assert(our_addr);
	shared_regions[client] = (void *)our_addr;
	return id;
}

int32
emu_CFE_EVS_Register(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_EVS_Register(s->cfe_evs_register.filters, s->cfe_evs_register.NumEventFilters,
	                        s->cfe_evs_register.FilterScheme);
}

int32
emu_CFE_SB_CreatePipe(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_SB_CreatePipe(&s->cfe_sb_createPipe.PipeId, s->cfe_sb_createPipe.Depth,
	                         s->cfe_sb_createPipe.PipeName);
}

void
emu_CFE_SB_InitMsg(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	CFE_SB_InitMsg(s->cfe_sb_initMsg.MsgBuffer, s->cfe_sb_initMsg.MsgId, s->cfe_sb_initMsg.Length,
	               s->cfe_sb_initMsg.Clear);
}

int32
emu_CFE_EVS_SendEvent(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_EVS_SendEvent(s->cfe_evs_sendEvent.EventID, s->cfe_evs_sendEvent.EventType, "%s",
	                         s->cfe_evs_sendEvent.Msg);
}

int32
emu_CFE_ES_RunLoop(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_ES_RunLoop(&s->cfe_es_runLoop.RunStatus);
}

int32
emu_CFE_SB_RcvMsg(spdid_t client)
{
	union shared_region *s = shared_regions[client];

	CFE_SB_MsgPtr_t BufPtr;
	int32           result = CFE_SB_RcvMsg(&BufPtr, s->cfe_sb_rcvMsg.PipeId, s->cfe_sb_rcvMsg.TimeOut);

	/* We want to save the message contents to the shared region
	 * But we need to be sure there is something to copy, so we check the call was successful
	 */
	if (result == CFE_SUCCESS) {
		int len = CFE_SB_GetTotalMsgLength(BufPtr);
		assert(len <= EMU_BUF_SIZE);
		memcpy(s->cfe_sb_rcvMsg.Msg, (char *)BufPtr, len);
	}
	return result;
}

uint16
emu_CFE_SB_GetTotalMsgLength(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_SB_GetTotalMsgLength(&s->cfe_sb_getMsgLen.Msg);
}

int32
emu_CFE_SB_SendMsg(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_SB_SendMsg((CFE_SB_MsgPtr_t)s->cfe_sb_msg.Msg);
}

uint16
emu_CFE_SB_GetCmdCode(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_SB_GetCmdCode((CFE_SB_MsgPtr_t)s->cfe_sb_msg.Msg);
}

CFE_SB_MsgId_t
emu_CFE_SB_GetMsgId(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_SB_GetMsgId((CFE_SB_MsgPtr_t)s->cfe_sb_msg.Msg);
}


void
emu_CFE_SB_TimeStampMsg(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	CFE_SB_TimeStampMsg((CFE_SB_MsgPtr_t)s->cfe_sb_msg.Msg);
}
