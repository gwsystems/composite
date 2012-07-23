/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_asm_server_stub_simple_stack.h>
.text	

cos_asm_server_stub(sched_init)
cos_asm_server_stub(sched_exit)
cos_asm_server_stub(sched_isroot)
cos_asm_server_stub_spdid(sched_child_thd_crt)
cos_asm_server_stub_spdid(sched_child_get_evt)
cos_asm_server_stub_spdid(sched_child_cntl_thd)
