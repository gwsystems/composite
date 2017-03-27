/**
 * Copyright 2009 by Gabriel Parmer, gabep1@cs.bu.edu
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 */


#include <cos_asm_server_stub_simple_stack.h>

.text	

<<<<<<< HEAD
cos_asm_server_stub_spdid(__mman_fork_spd)
cos_asm_server_stub_spdid(mman_get_page)
=======
//FIXME: the kernel ensured _spdid is not passed

//cos_asm_server_stub_spdid(mman_get_page)
//cos_asm_server_stub_spdid(mman_revoke_page)
//cos_asm_server_stub_spdid(__mman_alias_page)

cos_asm_server_stub(mman_get_page)
cos_asm_server_stub(mman_valloc)
>>>>>>> 30617db6d411a37cacea71d2cc806cfb300d9c27
cos_asm_server_stub(mman_release_page)
cos_asm_server_stub(mman_revoke_page)
cos_asm_server_stub(__mman_alias_page)
cos_asm_server_stub(mman_print_stats)
