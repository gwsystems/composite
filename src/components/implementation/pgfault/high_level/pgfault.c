/**
 * Copyright 2011 by The George Washington University.  All rights reserved.
 *
 * Redistribution of this file is permitted under the GNU General
 * Public License v2.
 *
 * Author: Gabriel Parmer, gparmer@gwu.edu, 2011
 */

#include <cos_component.h>
#include <pgfault.h>
#include <sched.h>
#include <print.h>

/* FIXME: should have a set of saved fault regs per thread. */
int regs_active = 0; 
void *fault_addr;
struct pt_regs regs;

static void save_fault_regs(int tid, void *fault)
{
	if (regs_active) BUG();
	regs_active = 1;
	fault_addr = fault;
	regs.ip = cos_thd_cntl(COS_THD_GET_IP, tid, 1, 0);
	regs.sp = cos_thd_cntl(COS_THD_GET_SP, tid, 1, 0);
	regs.bp = cos_thd_cntl(COS_THD_GET_FP, tid, 1, 0);
	regs.ax = cos_thd_cntl(COS_THD_GET_1, tid, 1, 0);
	regs.bx = cos_thd_cntl(COS_THD_GET_2, tid, 1, 0);
	regs.cx = cos_thd_cntl(COS_THD_GET_3, tid, 1, 0);	
	regs.dx = cos_thd_cntl(COS_THD_GET_4, tid, 1, 0);
	regs.di = cos_thd_cntl(COS_THD_GET_5, tid, 1, 0);
	regs.si = cos_thd_cntl(COS_THD_GET_6, tid, 1, 0);
}

static void print_regs(void)
{
	printc("EIP:%10x\tESP:%10x\tEBP:%10x\n"
	       "EAX:%10x\tEBX:%10x\tECX:%10x\n"
	       "EDX:%10x\tEDI:%10x\tESI:%10x\n",
	       (unsigned int)regs.ip, (unsigned int)regs.sp, (unsigned int)regs.bp,
	       (unsigned int)regs.ax, (unsigned int)regs.bx, (unsigned int)regs.cx,
	       (unsigned int)regs.dx, (unsigned int)regs.di, (unsigned int)regs.si);

	return;
}

void fault_page_fault_handler(spdid_t spdid, void *fault_addr, int flags, void *ip)
{
	save_fault_regs(cos_get_thd_id(), fault_addr);
	printc("Fault in spd %d @ %p\n", spdid, fault_addr);
	print_regs();
	BUG(); 			/* no fault is a good fault currently */
//	sched_block(spdid, 0);
	return;
}
