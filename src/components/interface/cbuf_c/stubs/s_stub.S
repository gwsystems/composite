/**
 * Copyright 2009 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_asm_server_stub_simple_stack.h>

.text	
cos_asm_server_stub_spdid(cbuf_c_create)
cos_asm_server_stub_spdid(cbuf_c_delete)
cos_asm_server_stub_spdid(cbuf_c_retrieve)
cos_asm_server_stub_spdid(cbuf_c_register)

cos_asm_server_stub(cbufmgr_buf_report)
cos_asm_server_stub(cbufmgr_set_concurrency)
cos_asm_server_stub(cbufmgr_spd_concurrency_estimate)
cos_asm_server_stub(cbufmgr_thd_blk_time)
cos_asm_server_stub(cbufmgr_thd_blk_cnt)
cos_asm_server_stub(cbufmgr_detect_suspension)
cos_asm_server_stub(cbufmgr_set_over_quota_limit)
cos_asm_server_stub(cbufmgr_set_suspension_limit)
cos_asm_server_stub(cbufmgr_get_allocated)
