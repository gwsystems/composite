#ifndef COS_SCHED_PARAMS_H
#define COS_SCHED_PARAMS_H

#include <res_spec.h>
#include <sconf/sconf.h>

static int 
parse_sched_str(char *str, union sched_param *sp, int nsp)
{
	struct sconf_kvs kvs[] = {{.key = "p", .val = NULL},
				  {.key = "w", .val = NULL},
				  {.key = "b", .val = NULL}};
	int types[] = {SCHEDP_PRIO, SCHEDP_WINDOW, SCHEDP_BUDGET};
	int r, n = 0, i;

	if (nsp < 3) return -1;
	for (i = 0 ; i < 3 ; i++) sp[i].c.type = SCHEDP_NOOP;
	if ((r = sconf_kv_populate(str, kvs, 3))) return r;
	for (i = 0 ; i < 3 ; i++) {
		if (!kvs[i].val) continue;
		sp[n].c.type    = types[i];
		sp[n++].c.value = atoi(kvs[i].val);
	}
	return n;
}

#endif	/* COS_SCHED_PARAMS_H */
