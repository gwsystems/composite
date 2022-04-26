/**
 * Redistribution of this file is permitted under the BSD two clause license.
 *
 * Copyright 2018, The George Washington University
 * Author: Phani Gadepalli, phanikishoreg@gwu.edu & Gabriel Parmer, gparmer@gwu.edu
 */

#include <cos_asm_stubs.h>

cos_asm_stub_indirect(capmgr_initthd_create)
cos_asm_stub_indirect(capmgr_thd_create_thunk)
cos_asm_stub_indirect(capmgr_thd_create_ext)

cos_asm_stub_indirect(capmgr_initaep_create)
cos_asm_stub_indirect(capmgr_aep_create_thunk)
cos_asm_stub_indirect(capmgr_aep_create_ext)

cos_asm_stub_indirect(capmgr_rcv_create)

cos_asm_stub(capmgr_set_tls)
cos_asm_stub(capmgr_asnd_create)
cos_asm_stub(capmgr_asnd_rcv_create)
cos_asm_stub(capmgr_asnd_key_create)
