/**
 * Copyright 2009 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

//#include <cos_asm_server_stub.h>
#define __ASM__
#include <consts.h>
#include <cos_asm_server_stub_simple_stack.h>

.text

cos_asm_server_stub(OS_IdleLoop)

cos_asm_server_stub(CFE_ES_ExitApp)
cos_asm_server_stub(CFE_ES_PerfLogAdd)
cos_asm_server_stub(CFE_ES_RegisterApp)
cos_asm_server_stub(CFE_ES_RunLoop)

cos_asm_server_stub(CFE_SB_GetCmdCode)
cos_asm_server_stub(CFE_SB_GetMsgId)
cos_asm_server_stub(CFE_SB_GetTotalMsgLength)
cos_asm_server_stub(CFE_SB_RcvMsg)
cos_asm_server_stub(CFE_SB_SendMsg)
cos_asm_server_stub(CFE_SB_Subscribe)
cos_asm_server_stub(CFE_SB_TimeStampMsg)
