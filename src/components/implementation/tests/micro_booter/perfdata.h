/*
 * Copyright 2017, Phani Gadepalli
 *
 * This uses a two clause BSD License.
 */
#ifndef PERFDATA_H
#define PERFDATA_H

/* Unoptimized by far!!!! */

#define PERF_VAL_RANGE     100000
#define PERF_DATA_NAME     32
#define PERF_DATA_PTILE_SZ 3

#define PERF_DATA_DEBUG

enum ptile_id {
	PTILE_90 = 0,
	PTILE_95,
	PTILE_99,
};

struct perfdata {
	char   name[PERF_DATA_NAME];
	double values[PERF_VAL_RANGE];
	double min, max, avg;
	double sd, var;
	double ptiles[PERF_DATA_PTILE_SZ]; /* 90, 95, 99 */
};

static void
perfdata_init(struct perfdata *pd, const char *nm)
{
	memset(pd, 0, sizeof(struct perfdata));
	strncpy(pd->name, nm, PERF_DATA_NAME-1);
}

static void
__print_values(struct perfdata *pd)
{
#ifdef PERF_DATA_DEBUG
	int i;

	for (i = 0; i < PERF_VAL_RANGE; i ++) printc("%.2f\n", pd->values[i]);
#endif
}

static int
perfdata_add(struct perfdata *pd, double val)
{
	static int i = 0;

	if (unlikely(i >= PERF_VAL_RANGE)) return -ENOSPC;

	pd->values[i ++] = val;

	return 0;
}

static double
__sqroot(double n)
{
	double lo = 0, hi = n, mid;
	int i;

	for(i = 0 ; i < 1000 ; i++) {
		mid = (lo + hi) / 2;

		if(mid * mid == n) return mid;

		if(mid * mid > n) hi = mid;
		else              lo = mid;
	}

	return mid;
}

static void
__pd_bubble_sort(struct perfdata *pd)
{
	int i;

	for (i = 0 ; i < PERF_VAL_RANGE ; i ++) {
		int j;

		for (j = 0 ; j < PERF_VAL_RANGE - i - 1 ; j ++) {
			if (pd->values[j] > pd->values[j + 1]) {
				double tmp = pd->values[j];

				pd->values[j]     = pd->values[j + 1];
				pd->values[j + 1] = tmp;
			}
		}
	}
}

static void
perfdata_calc(struct perfdata *pd)
{
	int i, j;

	__pd_bubble_sort(pd);
//	__print_values(pd);

	pd->min = pd->values[0];
	pd->max = pd->values[PERF_VAL_RANGE - 1];

	for (i = 0 ; i < PERF_VAL_RANGE ; i ++) pd->avg += pd->values[i];	
	pd->avg /= PERF_VAL_RANGE;

	for (i = 0 ; i < PERF_VAL_RANGE ; i ++) pd->var += (pd->values[i] - pd->avg) * (pd->values[i] - pd->avg);
	pd->var /= PERF_VAL_RANGE;

	pd->sd = __sqroot(pd->var);

	pd->ptiles[PTILE_90] = pd->values[(int)((PERF_VAL_RANGE * 90) / 100) - 1];
	pd->ptiles[PTILE_95] = pd->values[(int)((PERF_VAL_RANGE * 95) / 100) - 1];
	pd->ptiles[PTILE_99] = pd->values[(int)((PERF_VAL_RANGE * 99) / 100) - 1];
}

static double
perfdata_min(struct perfdata *pd)
{ return pd->min; }

static double
perfdata_max(struct perfdata *pd)
{ return pd->max; }

static double
perfdata_avg(struct perfdata *pd)
{ return pd->avg; }

static double
perfdata_sd(struct perfdata *pd)
{ return pd->sd; }

static double
perfdata_90ptile(struct perfdata *pd)
{ return pd->ptiles[PTILE_90]; }

static double
perfdata_95ptile(struct perfdata *pd)
{ return pd->ptiles[PTILE_95]; }

static double
perfdata_99ptile(struct perfdata *pd)
{ return pd->ptiles[PTILE_99]; }

static void
perfdata_print(struct perfdata *pd)
{
	printc("PD:%s-sz:%d,SD:%.2f,Mean:%.2f,99%%:%.2f\n", 
		pd->name, PERF_VAL_RANGE, pd->sd, pd->avg, pd->ptiles[PTILE_99]);
}

#endif /* PERFDATA_H */
