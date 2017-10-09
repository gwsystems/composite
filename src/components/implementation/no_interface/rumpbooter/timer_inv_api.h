#ifndef TIMER_INV_API_H
#define TIMER_INV_API_H

int timer_get_counter(void);
int timer_upcounter_wait(u32_t curr_count);
int timer_app_block(tcap_time_t timeout);

#endif /* TIMER_INV_API_H */
