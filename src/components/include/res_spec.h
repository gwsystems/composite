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

#endif /* RES_SPEC_H */
