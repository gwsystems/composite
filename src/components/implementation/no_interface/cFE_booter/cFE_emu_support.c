#include <stdio.h>

#include <cos_component.h>
#include <cos_debug.h>
#include <cos_types.h>

#include <memmgr.h>

#include <cFE_emu.h>

#include <cfe_error.h>

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
emu_CFE_ES_GetGenCount(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_ES_GetGenCount(s->cfe_es_getGenCount.CounterId, &s->cfe_es_getGenCount.Count);
}

int32
emu_CFE_ES_GetGenCounterIDByName(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_ES_GetGenCounterIDByName(&s->cfe_es_getGenCounterIDByName.CounterId, s->cfe_es_getGenCounterIDByName.CounterName);
}

int32
emu_CFE_ES_GetResetType(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_ES_GetResetType(&s->cfe_es_getResetType.ResetSubtype);
}

int32
emu_CFE_ES_GetTaskInfo(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_ES_GetTaskInfo(&s->cfe_es_getTaskInfo.TaskInfo, s->cfe_es_getTaskInfo.TaskId);
}

int32
emu_CFE_ES_RunLoop(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_ES_RunLoop(&s->cfe_es_runLoop.RunStatus);
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

int32
emu_CFE_EVS_SendEvent(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_EVS_SendEvent(s->cfe_evs_sendEvent.EventID, s->cfe_evs_sendEvent.EventType, "%s",
	                         s->cfe_evs_sendEvent.Msg);
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
emu_CFE_SB_GetMsgTime(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	CFE_TIME_SysTime_t time = CFE_SB_GetMsgTime((CFE_SB_MsgPtr_t)&s->cfe_sb_msg.Msg);
	s->time = time;
}

uint16
emu_CFE_SB_GetTotalMsgLength(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_SB_GetTotalMsgLength(&s->cfe_sb_getMsgLen.Msg);
}

void
emu_CFE_SB_InitMsg(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	CFE_SB_InitMsg(s->cfe_sb_initMsg.MsgBuffer, s->cfe_sb_initMsg.MsgId, s->cfe_sb_initMsg.Length,
	               s->cfe_sb_initMsg.Clear);
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

int32
emu_CFE_SB_SetCmdCode(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_SB_SetCmdCode((CFE_SB_MsgPtr_t)s->cfe_sb_setCmdCode.Msg, s->cfe_sb_setCmdCode.CmdCode);
}

int32
emu_CFE_SB_SendMsg(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_SB_SendMsg((CFE_SB_MsgPtr_t)s->cfe_sb_msg.Msg);
}

void
emu_CFE_SB_TimeStampMsg(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	CFE_SB_TimeStampMsg((CFE_SB_MsgPtr_t)s->cfe_sb_msg.Msg);
}

boolean
emu_CFE_SB_ValidateChecksum(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	return CFE_SB_ValidateChecksum((CFE_SB_MsgPtr_t)s->cfe_sb_msg.Msg);
}

void
emu_CFE_TIME_Add(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	s->cfe_time_add.Result = CFE_TIME_Add(s->cfe_time_add.Time1, s->cfe_time_add.Time2);
}

void
emu_CFE_TIME_Compare(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	s->cfe_time_compare.Result = CFE_TIME_Compare(s->cfe_time_compare.Time1, s->cfe_time_compare.Time2);
}

void
emu_CFE_TIME_GetTime(spdid_t client) {
	union shared_region *s = shared_regions[client];
	s->time = CFE_TIME_GetTime();
}

void
emu_CFE_TIME_Print(spdid_t client)
{
	union shared_region *s = shared_regions[client];
	CFE_TIME_Print(s->cfe_time_print.PrintBuffer, s->cfe_time_print.TimeToPrint);
}
