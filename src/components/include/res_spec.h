#ifndef RES_SPEC_H
#define RES_SPEC_H

#include <cos_types.h>

typedef int res_t;
typedef enum {RESRES_SOFT, RESRES_FIRM, RESRES_HARD} res_hardness_t;
typedef enum {RESRES_MEM, RESRES_CPU, RESRES_IO} res_type_t;
typedef struct {
	/* allocation, and window of that allocation */
	s16_t a, w;
} __attribute__((packed)) res_spec_t;
#define NULL_RSPEC ((res_spec_t){.a = 0, .w = 0})

static inline res_spec_t 
resres_spec(s16_t alloc)
{ return (res_spec_t){.a = alloc, .w = 0}; }

static inline res_spec_t 
resres_spec_w(s16_t alloc, s16_t window)
{ return (res_spec_t){.a = alloc, .w = window}; }

typedef enum {
	SCHEDP_NOOP,
	SCHEDP_PRIO,		/* fixed priority */
	SCHEDP_RPRIO,		/* priority relatively higher than current thread */
	/* priority relatively lower (not numerically) than current thread */
	SCHEDP_RLPRIO,
	SCHEDP_DEADLINE,	/* if != window */
	SCHEDP_BUDGET,		/* exec time */
	SCHEDP_WINDOW,     	/* period */
	SCHEDP_PROPORTION,	/* percent required */
	SCHEDP_WEIGHT,		/* proportion compared to other */
	SCHEDP_IDLE, 		/* idle thread: internal use only */
	SCHEDP_TIMER, 		/* timer thread: internal use only */
	SCHEDP_MAX		/* maximum value */
} sched_param_type_t;

struct sched_param_s {
	sched_param_type_t type:8;
	unsigned int value:24;
} __attribute__((packed));

union sched_param {
	struct sched_param_s c;      /* composite */
	u32_t v;		     /* value     */
};

#endif /* RES_SPEC_H */
