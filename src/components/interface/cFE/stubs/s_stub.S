/**
 * Copyright 2009 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#define __ASM__
#include <consts.h>
#include <cos_asm_server_stub_simple_stack.h>

.text

cos_asm_server_stub(OS_IdleLoop)

cos_asm_server_stub(CFE_ES_DeleteApp)
cos_asm_server_stub(CFE_ES_ExitApp)
cos_asm_server_stub(CFE_ES_PerfLogAdd)
cos_asm_server_stub(CFE_ES_RegisterApp)
cos_asm_server_stub(CFE_ES_ResetCFE)
cos_asm_server_stub(CFE_ES_RestartApp)

cos_asm_server_stub(CFE_PSP_WatchdogDisable)
cos_asm_server_stub(CFE_PSP_WatchdogEnable)
cos_asm_server_stub(CFE_PSP_WatchdogSet)
cos_asm_server_stub(CFE_PSP_WatchdogService)

cos_asm_server_stub(CFE_SB_Subscribe)

cos_asm_server_stub(CFE_TBL_NotifyByMessage)

cos_asm_server_stub(CFE_TIME_FS2CFESeconds)

// Emulated methods
cos_asm_server_stub(emu_backend_request_memory)

cos_asm_server_stub(emu_CFE_ES_GetGenCount)
cos_asm_server_stub(emu_CFE_ES_GetGenCounterIDByName)
cos_asm_server_stub(emu_CFE_ES_GetResetType)
cos_asm_server_stub(emu_CFE_ES_GetTaskInfo)
cos_asm_server_stub(emu_CFE_ES_RunLoop)

cos_asm_server_stub(emu_CFE_EVS_Register)
cos_asm_server_stub(emu_CFE_EVS_SendEvent)

cos_asm_server_stub(emu_CFE_SB_CreatePipe)
cos_asm_server_stub(emu_CFE_SB_GetCmdCode)
cos_asm_server_stub(emu_CFE_SB_GetMsgId)
cos_asm_server_stub(emu_CFE_SB_GetMsgTime)
cos_asm_server_stub(emu_CFE_SB_GetTotalMsgLength)
cos_asm_server_stub(emu_CFE_SB_InitMsg)
cos_asm_server_stub(emu_CFE_SB_RcvMsg)
cos_asm_server_stub(emu_CFE_SB_SetCmdCode)
cos_asm_server_stub(emu_CFE_SB_SendMsg)
cos_asm_server_stub(emu_CFE_SB_TimeStampMsg)
cos_asm_server_stub(emu_CFE_SB_ValidateChecksum)

cos_asm_server_stub(emu_CFE_TIME_Add)
cos_asm_server_stub(emu_CFE_TIME_Compare)
cos_asm_server_stub(emu_CFE_TIME_GetTime)
cos_asm_server_stub(emu_CFE_TIME_Print)
