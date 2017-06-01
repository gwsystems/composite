#ifndef REGS_H
#define REGS_H

#include <cos_types.h>
#include <print.h>

struct cos_regs {
	spdid_t spdid;
	int tid;
	void *fault_addr;
	struct pt_regs regs;
};

static void cos_regs_save(int tid, spdid_t spdid, void *fault, struct cos_regs *r)
{
	r->fault_addr = fault;
	r->regs.ip = cos_thd_cntl(COS_THD_GET_IP, tid, 1, 0);
	r->regs.sp = cos_thd_cntl(COS_THD_GET_SP, tid, 1, 0);
	r->regs.bp = cos_thd_cntl(COS_THD_GET_FP, tid, 1, 0);
	r->regs.ax = cos_thd_cntl(COS_THD_GET_1,  tid, 1, 0);
	r->regs.bx = cos_thd_cntl(COS_THD_GET_2,  tid, 1, 0);
	r->regs.cx = cos_thd_cntl(COS_THD_GET_3,  tid, 1, 0);	
	r->regs.dx = cos_thd_cntl(COS_THD_GET_4,  tid, 1, 0);
	r->regs.di = cos_thd_cntl(COS_THD_GET_5,  tid, 1, 0);
	r->regs.si = cos_thd_cntl(COS_THD_GET_6,  tid, 1, 0);
	r->tid     = tid;
	r->spdid   = spdid;
}

static void cos_regs_restore(struct cos_regs *r)
{
	cos_thd_cntl(COS_THD_SET_IP, r->tid, r->regs.ip, 1);
	cos_thd_cntl(COS_THD_SET_SP, r->tid, r->regs.sp, 1);
	cos_thd_cntl(COS_THD_SET_FP, r->tid, r->regs.bp, 1);
	cos_thd_cntl(COS_THD_SET_1,  r->tid, r->regs.ax, 1);
	cos_thd_cntl(COS_THD_SET_2,  r->tid, r->regs.bx, 1);
	cos_thd_cntl(COS_THD_SET_3,  r->tid, r->regs.cx, 1);	
	cos_thd_cntl(COS_THD_SET_4,  r->tid, r->regs.dx, 1);
	cos_thd_cntl(COS_THD_SET_5,  r->tid, r->regs.di, 1);
	cos_thd_cntl(COS_THD_SET_6,  r->tid, r->regs.si, 1);
}

static void cos_regs_print(struct cos_regs *r)
{
	printc("EIP:%10x\tESP:%10x\tEBP:%10x\n"
	       "EAX:%10x\tEBX:%10x\tECX:%10x\n"
	       "EDX:%10x\tEDI:%10x\tESI:%10x\n",
	       (unsigned int)r->regs.ip, (unsigned int)r->regs.sp, (unsigned int)r->regs.bp,
	       (unsigned int)r->regs.ax, (unsigned int)r->regs.bx, (unsigned int)r->regs.cx,
	       (unsigned int)r->regs.dx, (unsigned int)r->regs.di, (unsigned int)r->regs.si);

	return;
}

#endif	/* REGS_H */
