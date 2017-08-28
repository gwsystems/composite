#ifndef TIMER_H
#define TIMER_H

#ifdef TIMER_ACTIVATE

#ifndef rdtscl
#define rdtscl(val) __asm__ __volatile__("rdtsc" : "=a"(val) : : "edx")
#endif

#define STATIC_TIMER_RECORDS(name, N) struct timer_record name[N]
#define TIMER_INIT(name, rs, idx) struct timer name = {.start = 0, .r = &rs[idx]}

struct timer_record {
	unsigned long tot, max, min, cnt;
};

struct timer {
	unsigned long        start;
	struct timer_record *r;
};

static inline void
timer_start(struct timer *t)
{
	rdtscl(t->start);
}

static inline void
timer_end(struct timer *t)
{
	unsigned long        end, diff;
	struct timer_record *r;

	rdtscl(end);
	diff = end - t->start;
	/* If you're measuring for that long, use another mechanism */
	if (diff > ((unsigned long)~0) / 2) return;
	r = t->r;

	if (r->max < diff) r->max = diff;
	if (r->min > diff || r->min == 0) r->min = diff;
	r->tot += diff;
	r->cnt++;
}

static inline void
timer_report(struct timer_record *rs, int type, unsigned long *avg, unsigned long *max, unsigned long *min)
{
	struct timer_record *r = &rs[type];
	*avg                   = (unsigned long)(r->cnt ? (r->tot / r->cnt) : 0);
	*max                   = rs[type].max;
	*min                   = rs[type].min;

	return;
}

#else

struct timer {
	int nothing;
};
struct timer_record {
	int nothing;
};
#define STATIC_TIMER_RECORDS(name, N) struct timer_record *name
#define TIMER_INIT(name, rs, idx) struct timer name
static inline void
timer_start(struct timer *t)
{
	return;
}
static inline void
timer_end(struct timer *t)
{
	return;
}
static inline void
timer_report(struct timer_record *rs, int type, unsigned long *a, unsigned long *min, unsigned long *max)
{
	return;
}

#endif

#endif /* !TIMER_H */
