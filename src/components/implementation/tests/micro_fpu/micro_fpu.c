#include <cos_component.h>
#include <print.h>
#include <sched.h>
#include <res_spec.h>

#define ITER 20000

double compute_float(int);

volatile double tmp;

void
cos_init(void)
{
        static int flag = 0;
        union sched_param sp;
        int i;
        static int main_thd, high_prio_thd, low_prio_thd;
        static unsigned long long block_start, block_end, wakeup_start, wakeup_end;
        static unsigned long long block_max, block_min, block_avg, block_tmp, wakeup_max, wakeup_min, wakeup_avg, wakeup_tmp;

        if (flag == 0) {
                printc("<<< FPU MICRO BENCHMARK TEST  >>>\n");
                flag = 1;
                main_thd = cos_get_thd_id();

                /* Create thread with higher priority */
                sp.c.type = SCHEDP_PRIO;
                sp.c.value = 10;
                high_prio_thd = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

                /* Create thread with lower priority */
                sp.c.type = SCHEDP_PRIO;
                sp.c.value = 11;
                low_prio_thd = sched_create_thd(cos_spd_id(), sp.v, 0, 0);

        } else {
                for (i = 0; i < ITER; i++) {
                        if (cos_get_thd_id() == high_prio_thd) {
                                tmp = compute_float(10);
                                tmp = compute_float(9);

                                /* BEGIN: BLOCK + CONTEXT SWITCH */
                                rdtscll(block_start);
                                sched_block(cos_spd_id(), low_prio_thd);

                                /* switched back, continue to execute */
                                rdtscll(wakeup_end);
                                wakeup_tmp = wakeup_end - wakeup_start;
                                if (wakeup_tmp > wakeup_max) wakeup_max = wakeup_tmp;
                                else if ( (wakeup_tmp < wakeup_min) || (wakeup_min == 0)) wakeup_min = wakeup_tmp;
                                wakeup_avg += wakeup_tmp;
                                /* END: WAKEUP + CONTEXT SWITCH */

                        } else if (cos_get_thd_id() == low_prio_thd) {
                                rdtscll(block_end);
                                block_tmp = block_end - block_start;
                                if (block_tmp > block_max) block_max = block_tmp;
                                else if ((block_tmp < block_min) || (block_min == 0)) block_min = block_tmp;
                                block_avg += block_tmp;
                                /* END: BLOCK + CONTEXT SWITCH */

                                tmp = compute_float(8);
                                tmp = compute_float(7);

                                /* BEGIN: WAKEUP + CONTEXT SWITCH */
                                rdtscll(wakeup_start);
                                sched_wakeup(cos_spd_id(), high_prio_thd);
                        }
                }
        }

        if (cos_get_thd_id() == low_prio_thd) {
                printc("block_max: %llu, block_min: %llu, block_avg: %llu\n", block_max, block_min, block_avg / ITER);
                printc("wakeup_max: %llu, wakeup_min: %llu, wakeup_avg: %llu\n", wakeup_max, wakeup_min, wakeup_avg / ITER);
                printc("<<< FPU MICRO BENCHMARK TEST DONE >>>\n");
        }

        return;
}

double
compute_float(int counter)
{
        double a = 0.1;
        int i;

        for (i = 0; i < counter; i++) {
                a += 0.1;
        }

        return a;
}
