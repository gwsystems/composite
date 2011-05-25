/**
 * Copyright 2009 by Andrew Sweeney, ajs86@gwu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */


#include <cos_asm_server_stub_simple_stack.h>

.text

cos_asm_server_stub_spdid(stkmgr_grant_stack)
cos_asm_server_stub_spdid(stkmgr_return_stack)

cos_asm_server_stub(stkmgr_stack_report)
cos_asm_server_stub(stkmgr_set_concurrency)
cos_asm_server_stub(stkmgr_spd_concurrency_estimate)
cos_asm_server_stub(stkmgr_thd_blk_time)
cos_asm_server_stub(stkmgr_thd_blk_cnt)
cos_asm_server_stub(stkmgr_detect_suspension)
cos_asm_server_stub(stkmgr_set_over_quota_limit)
cos_asm_server_stub(stkmgr_set_suspension_limit)
cos_asm_server_stub(stkmgr_get_allocated)
	
cos_asm_server_stub_spdid(stkmgr_stack_introspect)
cos_asm_server_stub_spdid(stkmgr_stack_close)
