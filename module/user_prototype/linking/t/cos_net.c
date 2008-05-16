#include <cos_component.h>
#include <cos_debug.h>
//#include <cos_alloc.h>

#define NUM_THDS 2
#define BLOCKED 0x8000000
#define GET_CNT(x) (x & (~BLOCKED))
#define IS_BLOCKED(x) (x & BLOCKED)

struct thd_map {
	unsigned short int thd, upcall, port;
	volatile int pending_evts, blocked;
} tmap[NUM_THDS];

static struct thd_map *get_thd_map_port(unsigned short port) 
{
	int i;
	
	for (i = 0 ; i < NUM_THDS ; i++) {
		if (tmap[i].port == port) {
			return &tmap[i];
		}
	}
	
	return NULL;
}

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

#define REPORT_PACKETS_WINDOW 100
extern void sched_report_processing(unsigned int);

void synthesize_work(unsigned long long amnt)
{
	static int packets_processed[MAX_NUM_THREADS] = {0, };
	int id = cos_get_thd_id();
	unsigned long long start, end;

	rdtscll(start);
	while (1) {
		rdtscll(end);
		if (end-start > amnt) {
			break;
		}
	}

	packets_processed[id]++;
	if (packets_processed[id] % REPORT_PACKETS_WINDOW == 0) {
		sched_report_processing(REPORT_PACKETS_WINDOW);
		packets_processed[id] = 0;
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

static int deposit_event(struct thd_map *tm)
{
	int pe;
	
	if (!tm) return 0;

//	sched_wakeup(tm->thd);

//	return 0;

	do {
		pe = tm->pending_evts;
	} while(cos_cmpxchg(&tm->pending_evts, pe, pe+1) != pe+1);

	if (pe == BLOCKED /* implies cnt == 0 */) {
		sched_wakeup(tm->thd);
	}

	return 0;
}

static int retrieve_event(unsigned short int thd_id)
{
	struct thd_map *tm = get_thd_map(thd_id);
	int pe;

//	sched_block();

//	return 0;

	assert(tm);
//again:
	pe = tm->pending_evts;
	assert(!IS_BLOCKED(pe));
	if (0 == pe) {
		int ret;
		/* if this fails, then there's a pending evt, mission accomp */
		ret = cos_cmpxchg(&tm->pending_evts, 0, BLOCKED);
		if (ret == BLOCKED) {
			//tm->blocked = 1;
			//if (pe != 0) {
			//tm->blocked = 0;
			//goto again;
			//}

			assert(!sched_block());
			//tm->blocked = 0;
			//assert(tm->pending_evts);
			do {
				pe = tm->pending_evts;
				assert(IS_BLOCKED(pe) && GET_CNT(pe) > 0);
			} while (cos_cmpxchg(&tm->pending_evts, pe, GET_CNT(pe)-1) != GET_CNT(pe)-1);
		} else {
			do {
				pe = tm->pending_evts;
			} while(cos_cmpxchg(&tm->pending_evts, pe, pe-1) != pe-1);
		}
	} else {
		do {
			pe = tm->pending_evts;
		} while(cos_cmpxchg(&tm->pending_evts, pe, pe-1) != pe-1);
	}

	return 0;
}

static int application(void)
{
	while (1) {
//		print("application %d running... %d%d", cos_get_thd_id(), 0,0);
		retrieve_event(cos_get_thd_id());
		synthesize_app_work();
	}

	return 0;
}

static int interrupt(int id)
{
	struct thd_map *tm = get_thd_map_port(id);
//	struct thd_map *tm = get_thd_map(cos_get_thd_id());

//	print("port is %d. %d%d", id, 0,0);
	synthesize_interrupt_work();
//	print("Interrupt in thd %d. %d%d", cos_get_thd_id(), 0,0);
	//print("port %d, thread_map %p, dest_thd %d", id, (unsigned int)tm, tm->thd);
	deposit_event(tm);

	return 0;
}

extern int sched_ds_create_net_upcall(unsigned short int port);

static int new_thd(void)
{
	unsigned short int b_id, t_id;
	static int port = 200;
	int i;

	b_id = sched_ds_create_net_upcall(port);
	
	for (i = 0 ; i < NUM_THDS ; i++) {
		if (tmap[i].thd == 0) {
			tmap[i].thd = cos_get_thd_id();
			tmap[i].upcall = b_id;
			tmap[i].port = port;
			break;
		}
	}
	assert(i != NUM_THDS);

	port++;
	application();

	return 0;
}

void cos_upcall_fn(upcall_type_t t, void *arg1, void *arg2, void *arg3)
{
	switch (t) {
	case COS_UPCALL_BRAND_EXEC:
	{
		//print("port %d. %d%d", (unsigned int)arg1, 0,0);
		interrupt((unsigned int)arg1);
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
		print("Unknown type of upcall %d made to net. %d%d", t, 0,0);
		assert(0);
		return;
	}

	return;
}
