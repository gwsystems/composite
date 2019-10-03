#ifndef COS_ULSCHED_RCV_H
#define COS_ULSCHED_RCV_H

#include <cos_kernel_api.h>

static inline int
__cos_sched_events_present(struct cos_sched_ring *r)
{
	return !(r->tail == r->head);
}

static inline int
cos_sched_ispending(void)
{
	struct cos_scb_info *scb_cpu = cos_scb_info_get_core();
	struct cos_sched_ring *r     = &scb_cpu->sched_events;

	return r->more;
}

static inline int
__cos_sched_event_consume(struct cos_sched_ring *r, struct cos_sched_event *e)
{
	int f = 0;

	if (!r || !e || !__cos_sched_events_present(r)) return 0;
	f = ps_upfaa((unsigned long *)&r->head, 1);
	*e = r->event_buf[f];
//	memcpy((void *)e, (void *)&(r->event_buf[f]), sizeof(struct cos_sched_event));

	return 1;
}

/* if other than sched-thread calls this, races will need to be handled by the caller! */
static inline int
cos_ul_sched_rcv(arcvcap_t rcv, rcv_flags_t rfl, tcap_time_t timeout, struct cos_sched_event *evt)
{
	int ret = 0;
	struct cos_scb_info *scb_cpu = cos_scb_info_get_core();
	struct cos_sched_ring *r     = &scb_cpu->sched_events;

	assert(scb_cpu);
	/* a non-scheduler thread, should call with rcv == 0 to consume user-level events alone */
	if (unlikely(__cos_sched_event_consume(r, evt) == 0
		     && rcv && !(rfl & RCV_ULONLY))) {
		ret = cos_sched_rcv(rcv, rfl, timeout, &(evt->tid), (int *)&(evt->evt.blocked),
			            (cycles_t *)&(evt->evt.elapsed_cycs), (tcap_time_t *)&(evt->evt.next_timeout));
		if (unlikely(ret < 0)) return ret;
	}

	return (ret || __cos_sched_events_present(r) || cos_sched_ispending());
}

static inline int
cos_ul_rcv(arcvcap_t rcv, rcv_flags_t rfl, tcap_time_t sched_timeout)
{
	struct cos_sched_event ev = { .tid = 0 };
	int ret = 0;

	ret = cos_sched_rcv(rcv, rfl, sched_timeout, &(ev.tid), (int *)&(ev.evt.blocked),
			    (cycles_t *)&(ev.evt.elapsed_cycs), (tcap_time_t *)&(ev.evt.next_timeout));
	assert(ev.tid == 0);

	return ret;
}

#endif /* COS_ULSCHED_RCV_H */
