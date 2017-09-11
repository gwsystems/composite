/**
 * Copyright 2012 by Gabriel Parmer, gparmer@gwu.edu.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */

#include <cos_asm_server_stub_simple_stack.h>

.text	
cos_asm_server_stub_spdid(cbuf_create)
cos_asm_server_stub_spdid(cbuf_delete)
cos_asm_server_stub_spdid(cbuf_retrieve)
cos_asm_server_stub_spdid(cbuf_register)
cos_asm_server_stub_spdid(cbuf_map_at)
cos_asm_server_stub_spdid(cbuf_unmap_at)
cos_asm_server_stub_spdid(cbuf_map_collect)
cos_asm_server_stub_spdid(cbuf_fork_spd)
cos_asm_server_stub_spdid(cbuf_collect)
cos_asm_server_stub(cbuf_mempool_resize)
cos_asm_server_stub(cbuf_memory_target_get)

/*debug functions*/
cos_asm_server_stub(cbuf_debug_cbuf_info)
cos_asm_server_stub(cbuf_debug_cbiddump)
cos_asm_server_stub(cbuf_debug_profile)
