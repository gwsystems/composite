
#include <cos_component.h>
#include <print.h>
#include <cos_synchronization.h>

//#include <quarantine.h>

extern void cbuf_tests();
extern void cbufp_tests();

cos_lock_t l;
#define TAKE()    do { if (unlikely(lock_take(&l) != 0)) BUG(); }   while(0)
#define RELEASE() do { if (unlikely(lock_release(&l) != 0)) BUG() } while(0)
#define LOCK_INIT()    lock_static_init(&l);

void cos_init(void)
{
	//spdid_t new_spd;
	printc("Starting in spd %d\n", cos_spd_id());
	printc("With locking\n");
	LOCK_INIT();
	printc("\nUNIT TEST (CBUF & CBUFP)\n");
	TAKE();
	cbuf_tests();
	RELEASE();
	//new_spd = quarantine_fork(cos_spd_id(), cos_spd_id());
	TAKE();
	cbufp_tests();
	printc("UNIT TEST (CBUF & CBUFP) ALL PASSED\n");
	RELEASE();
	printc("Done in spd %d\n", cos_spd_id());
	return;
}

