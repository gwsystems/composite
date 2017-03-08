#ifndef PERFDATA_H
#define PERFDATA_H

/* For now, not re-entrant */

#define PERF_VAL_RANGE 1000

enum ptile_id {
	PTILE_90 = 0,
	PTILE_95,
	PTILE_99,
};

struct perfdata {
	double values[PERF_VAL_RANGE];
	double min, max, avg;
	double sd, var;
	double ptiles[3]; /* 90, 95, 99 */
} pd;

static void
perfdata_init(void)
{
	memset(&pd, 0, sizeof(pd));
}

static struct perfdata *
perfdata_pd(void)
{ return &pd; }

static void
__print_values(void)
{
	int i;

	for (i = 0; i < PERF_VAL_RANGE; i ++) printc("%f\n", pd.values[i]);
}

static int
perfdata_add(double val)
{
	static int i = 0;

	if (i >= PERF_VAL_RANGE) return -ENOSPC;

	pd.values[i ++] = val;

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
__bubble_sort(void)
{
	int i;

	for (i = 0 ; i < PERF_VAL_RANGE ; i ++) {
		int j;

		for (j = 0 ; j < PERF_VAL_RANGE - i - 1 ; j ++) {
			if (pd.values[j] > pd.values[j + 1]) {
				double tmp = pd.values[j];

				pd.values[j]     = pd.values[j + 1];
				pd.values[j + 1] = tmp;
			}
		}
	}
}

static void
perfdata_calc(void)
{
	int i, j;

//	__print_values();
	__bubble_sort();
	__print_values();

	pd.min = pd.values[0];
	pd.max = pd.values[PERF_VAL_RANGE - 1];

	for (i = 0 ; i < PERF_VAL_RANGE ; i ++) pd.avg += pd.values[i];	
	pd.avg /= PERF_VAL_RANGE;

	for (i = 0 ; i < PERF_VAL_RANGE ; i ++) pd.var += (pd.values[i] - pd.avg) * (pd.values[i] - pd.avg);
	pd.var /= PERF_VAL_RANGE;

	pd.sd = __sqroot(pd.var);

	pd.ptiles[PTILE_90] = pd.values[(int)((PERF_VAL_RANGE * 90) / 100) - 1];
	pd.ptiles[PTILE_95] = pd.values[(int)((PERF_VAL_RANGE * 95) / 100) - 1];
	pd.ptiles[PTILE_99] = pd.values[(int)((PERF_VAL_RANGE * 99) / 100) - 1];
}

static double
perfdata_min(void)
{ return pd.min; }

static double
perfdata_max(void)
{ return pd.max; }

static double
perfdata_avg(void)
{ return pd.avg; }

static double
perfdata_sd(void)
{ return pd.sd; }

static double
perfdata_90ptile(void)
{ return pd.ptiles[PTILE_90]; }

static double
perfdata_95ptile(void)
{ return pd.ptiles[PTILE_95]; }

static double
perfdata_99ptile(void)
{ return pd.ptiles[PTILE_99]; }

static void
perfdata_print(void)
{
	printc("Size-%d\nS.D-%f\nAvg-%f\nmin-%f\nmax-%f\n99%%tile-%f\n95%%tile-%f\n90%%tile-%f\n",
		PERF_VAL_RANGE, pd.sd, pd.avg, pd.min, pd.max, pd.ptiles[PTILE_99], pd.ptiles[PTILE_95], pd.ptiles[PTILE_90]);
}

#endif /* PERFDATA_H */
