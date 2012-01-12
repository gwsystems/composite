/**
 * Copyright 2008 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_asm_server_stub_simple_stack.h>

.text	
cos_asm_server_stub_spdid(periodic_wake_create)
cos_asm_server_stub_spdid(periodic_wake_remove)
cos_asm_server_stub_spdid(periodic_wake_wait)
cos_asm_server_stub(periodic_wake_get_misses)
cos_asm_server_stub(periodic_wake_get_deadlines)
cos_asm_server_stub(periodic_wake_get_lateness)
cos_asm_server_stub(periodic_wake_get_miss_lateness)
cos_asm_server_stub(periodic_wake_get_period)
