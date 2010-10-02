#include <cos_component.h>
#include <cos_scheduler.h>
#define COS_FMT_PRINT
#include <print.h>
#include <errno.h>
#include <sched.h>

int sched_child_get_evt(spdid_t spdid, struct sched_child_evt *e, int idle, unsigned long wake_diff)
{
	BUG();
	return 0;
}

int sched_child_cntl_thd(spdid_t spdid)
{
	BUG();
	return 0;
}

int sched_child_thd_crt(spdid_t spdid, spdid_t dest_spd)
{
	BUG();
	return 0;
}


int sched_root_init(void) { BUG(); return 0; }

int sched_wakeup(spdid_t spdid, unsigned short int thd_id)
{
	BUG();
	return -ENOTSUP;
}

int sched_block(spdid_t spdid, unsigned short int dependency_thd)
{
	BUG();
	return -ENOTSUP;
}

void sched_timeout(spdid_t spdid, unsigned long amnt) { BUG(); return; }

int sched_priority(unsigned short int tid) { BUG(); return 0; }

int sched_timeout_thd(spdid_t spdid)
{
	BUG();
	return -ENOTSUP;
}

unsigned int sched_tick_freq(void)
{
	BUG();
	return 0;
}

unsigned long sched_timestamp(void)
{
	BUG();
	return 0;
}


int sched_create_thread(spdid_t spdid, struct cos_array *data){
	BUG();
	return -ENOTSUP;
}

int sched_create_thread_default(spdid_t spdid, spdid_t target)
{
	BUG();
	return -ENOTSUP;
}

int sched_thread_params(spdid_t spdid, u16_t thd_id, res_spec_t rs)
{
	BUG();
	return -ENOTSUP;
}

int sched_create_net_brand(spdid_t spdid, unsigned short int port)
{
	BUG();
	return -ENOTSUP;
}

int sched_add_thd_to_brand(spdid_t spdid, unsigned short int bid, unsigned short int tid)
{
	BUG();
	return -ENOTSUP;
}

void sched_exit(void) { BUG(); return; }


int sched_component_take(spdid_t spdid) 
{ 
	BUG(); 
	return -ENOTSUP;
}

int sched_component_release(spdid_t spdid)
{
	BUG();
	return -ENOTSUP;
}

