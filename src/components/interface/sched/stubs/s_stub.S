/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu
 */

#include <cos_asm_server_stub_simple_stack.h>

.text
cos_asm_server_stub(sched_thd_wakeup);
cos_asm_server_stub(sched_thd_block);
cos_asm_server_stub_rets(sched_thd_block_timeout_cserialized);
cos_asm_server_stub(sched_thd_create_cserialized);
cos_asm_server_stub_rets(sched_aep_create_cserialized);
cos_asm_server_stub(sched_thd_param_set);
cos_asm_server_stub(sched_thd_exit);
cos_asm_server_stub(sched_thd_delete);
