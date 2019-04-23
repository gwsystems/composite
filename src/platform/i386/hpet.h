#ifndef HPET_H
#define HPET_H

typedef enum {
	HPET_PERIODIC = 0,
	HPET_ONESHOT  = 1,
} hpet_type_t;

void  hpet_set(hpet_type_t timer_type, u64_t cycles);
void  hpet_init(void);
u64_t hpet_find(void *timer);
void  hpet_set_page(u32_t page);

#endif /* HPET_H */
