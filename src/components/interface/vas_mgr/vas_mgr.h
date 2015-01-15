/**
 * Copyright 2010 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2010
 */

#ifndef VAS_MGR_H
#define VAS_MGR_H

vaddr_t vas_mgr_expand(spdid_t spd, spdid_t dest, unsigned long amnt);
void    vas_mgr_contract(spdid_t spd, vaddr_t addr);
int vas_mgr_take(spdid_t spd, spdid_t dest, vaddr_t d_addr, unsigned long amnt);

#endif 	/* !VAS_MGR_H */
