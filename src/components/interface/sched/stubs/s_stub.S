/**
 * Copyright 2007 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_asm_server_stub_simple_stack.h>
.text	

cos_asm_server_stub(sched_init)
cos_asm_server_stub(sched_isroot)
cos_asm_server_stub_spdid(sched_wakeup)
cos_asm_server_stub_spdid(sched_block)
cos_asm_server_stub_spdid(sched_timeout)
cos_asm_server_stub_spdid(sched_timeout_thd)
cos_asm_server_stub_spdid(sched_create_net_brand)
cos_asm_server_stub_spdid(sched_add_thd_to_brand)
cos_asm_server_stub(sched_exit)
cos_asm_server_stub(sched_create_thread)
cos_asm_server_stub(sched_create_thd)
cos_asm_server_stub(sched_create_thread_default)
cos_asm_server_stub_spdid(sched_thread_params)
cos_asm_server_stub(sched_tick_freq)
cos_asm_server_stub(sched_cyc_per_tick)
cos_asm_server_stub(sched_timestamp)
cos_asm_server_stub(sched_timer_stopclock)
cos_asm_server_stub(sched_priority)
	
cos_asm_server_stub_spdid(sched_component_take)
cos_asm_server_stub_spdid(sched_component_release)

cos_asm_server_stub_spdid(sched_child_thd_crt)
cos_asm_server_stub_spdid(sched_child_get_evt)
cos_asm_server_stub_spdid(sched_child_cntl_thd)
