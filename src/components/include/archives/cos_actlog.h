#ifdef ACT_LOG

#ifndef ACT_LOG_LEN
#define ACT_LOG_LEN 128 /* must be a power of 2 */
#endif
#define ACT_LOG_MASK (ACT_LOG_LEN - 1)
#ifndef NUM_ACT_ITEMS
#define NUM_ACT_ITEMS 1
#endif
#ifndef rdtscl
#define rdtscl(val) __asm__ __volatile__("rdtsc" : "=a"(val) : : "edx")
#endif

/* typedef struct {} filter_t; */
/* typedef int (*filter_fn_t)(unsigned long *items); */
/* typedef struct { */
/* 	filter_t *filt; */
/* 	filter_fn_t f; */
/* } filter_t; */
/* filter_t *filter_compose(filter_t *f1, filter_t *f2) {  */
/* 	f1->filt = f2; return f1; } */
/* int filter_invoke(filter_t *f, unsigned long *items) {  */
/* 	return f->f(items) || (f->filt ? filter_invoke(f->filt, items) : 1); } /\* tailcall *\/ */

typedef int (*filter_fn_t)(unsigned long *items);
struct action {
	action_t action;
#ifdef ACTION_TIMESTAMP
	unsigned long ts;
#endif
	unsigned long act_items[NUM_ACT_ITEMS];
};

static struct action actions[ACT_LOG_LEN];
static int           action_head = 0, action_tail = ACT_LOG_LEN - 1;

static void
action_record(action_t action, unsigned long *action_items, filter_fn_t f)
{
	struct action *a = &actions[action_head];
	int            i;

	if (f && f(action_items)) return;

	action_head = (action_head + 1) & ACT_LOG_MASK;
	if (action_head == action_tail) action_tail= (action_tail + 1) & ACT_LOG_MASK;

	a->action = action;
#ifdef ACTION_TIMESTAMP
	{
		unsigned long ts;
		rdtscl(ts);
		a->ts = ts;
	}
#endif
	for (i = 0; i < NUM_ACT_ITEMS; i++) {
		a->act_items[i] = action_items[i];
	}
}

static struct action *
action_report(void)
{
	int new_tail;

	new_tail = (action_tail + 1) & ACT_LOG_MASK;
	if (new_tail == action_head) return NULL;

	action_tail = new_tail;
	return &actions[action_tail];
}

static unsigned long
action_item(action_item_t item, struct action *a)
{
	return a->act_items[item];
}

#else

#define action_record(a, s, l, t, t)
#define action_report() NULL
#define action_item(a, b) 0

#endif /* ACT_LOG */
