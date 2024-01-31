#ifndef COS_TRACE_H
#define COS_TRACE_H

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <cos_types.h>
#include <ps.h>
#include <ck_ring.h>
#include <cos_component.h>


#define COS_TRACE_NEVENTS 10000

#ifndef COS_TRACE_ENABLED

void cos_trace(const char *format, cycles_t tsc, long cpu, thdid_t tid, compid_t cid, dword_t a, dword_t b, dword_t c);
void cos_trace_print_buffer(void);

/* Attaches the current (cycle, cpuid, thdid, compid) */
#define COS_TRACE(format, a, b, c)                                          \
	{                                                                       \
		cos_trace(format, ps_tsc(), cos_coreid(), cos_thdid(), cos_compid(),\
                    a, b, c);   \
    }

#else

#define COS_TRACE(format, a, b, c)  

#endif // COS_TRACE_ENABLED
#endif // COS_TRACE_H