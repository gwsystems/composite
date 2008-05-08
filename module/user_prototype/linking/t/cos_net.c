#include <cos_component.h>
#include <cos_debug.h>
//#include <cos_alloc.h>

#define NUM_THDS 2
struct thd_map {
	unsigned short int thd, upcall;
	volatile int pending_evts, blocked;
} tmap[NUM_THDS];

static struct thd_map *get_thd_map(unsigned short int thd_id)
{
	int i;
	
	for (i = 0 ; i < NUM_THDS ; i++) {
		if (tmap[i].thd == thd_id ||
		    tmap[i].upcall == thd_id) {
			return &tmap[i];
		}
	}
	
	return NULL;
}

void synthesize_work(unsigned long long amnt)
{
	unsigned long long start, end;

	rdtscll(start);
	while (1) {
		rdtscll(end);
		if (end-start > amnt) {
			break;
		}
	}
}

#define INTERRUPT_WORK 14000
#define APP_WORK 30000

void synthesize_interrupt_work(void)
{
	synthesize_work(INTERRUPT_WORK);
}

void synthesize_app_work(void)
{
	synthesize_work(APP_WORK);
}

extern int sched_block(void);
extern int sched_wakeup(unsigned short int thd_id);

static int deposit_event(unsigned short int thd_id)
{
	struct thd_map *tm = get_thd_map(thd_id);
	int pe;

	assert(tm);
	pe = tm->pending_evts;
	tm->pending_evts++;
	if (tm->blocked && pe <= 0) {
		sched_wakeup(tm->thd);
	}

	return 0;
}

static int retrieve_event(unsigned short int thd_id)
{
	struct thd_map *tm = get_thd_map(thd_id);
	int pe;

	assert(tm);
	pe = tm->pending_evts;
	if (!pe) {
		tm->blocked = 1;
		sched_block();
		tm->blocked = 0;
		assert(tm->pending_evts);
	}
	tm->pending_evts--;

	return 0;
}

static int application(void)
{
	while (1) {
		print("application %d running... %d%d", cos_get_thd_id(), 0,0);
		retrieve_event(cos_get_thd_id());
		synthesize_app_work();
	}

	return 0;
}

static int interrupt(void)
{
	//synthesize_interrupt_work();
	print("Interrupt in thd %d. %d%d", cos_get_thd_id(), 0,0);
	deposit_event(cos_get_thd_id());

	return 0;
}

extern int sched_create_net_upcall(unsigned short int port);

static int new_thd(void)
{
	unsigned short int b_id, t_id;
	static int port = 200;
	int i;

	b_id = sched_create_net_upcall(port++);
	
	for (i = 0 ; i < NUM_THDS ; i++) {
		if (tmap[i].thd == 0) {
			tmap[i].thd = cos_get_thd_id();
			tmap[i].upcall = b_id;
		}
	}
	assert(i != NUM_THDS);

	application();

	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
	{
		interrupt();
		break;
	}
	case COS_UPCALL_BOOTSTRAP:
		new_thd();
		break;
	case COS_UPCALL_CREATE:
		assert(0);
		break;
	case COS_UPCALL_BRAND_COMPLETE:
		assert(0);
		break;
	default:
		assert(0);
		return;
	}

	return;
}
