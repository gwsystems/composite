#include <sl.h>
#include <stddef.h>

#define NUM_FIELDS 14

long __CTOR_LIST__;
long __INIT_ARRAY_LIST__;
long __DTOR_LIST__;
long __FINI_ARRAY_LIST__;
long __CRECOV_LIST__;

static char * field_names[NUM_FIELDS] = {
	"state",
	"properties",
	"thdid",
	"aepinfo",
	"sndcap",
	"prio",
	"dependency",
	"budget",
	"last_replenish",
	"period",
	"periodic_cycs",
	"timeout_cycs",
	"wakeup_cycs",
	"timeout_idx"
};
static size_t expected_offsets[NUM_FIELDS] = {0,4,8,12,32,36,44,48,52,60,68,76,84,92};
static size_t expected_size = 96;

int
main(void) {
	int i;
	size_t actual_offsets[NUM_FIELDS] = {
		offsetof(struct sl_thd,state),
		offsetof(struct sl_thd,properties),
		offsetof(struct sl_thd,thdid),
		offsetof(struct sl_thd,aepinfo),
		offsetof(struct sl_thd,sndcap),
		offsetof(struct sl_thd,prio),
		offsetof(struct sl_thd,dependency),
		offsetof(struct sl_thd,budget),
		offsetof(struct sl_thd,last_replenish),
		offsetof(struct sl_thd,period),
		offsetof(struct sl_thd,periodic_cycs),
		offsetof(struct sl_thd,timeout_cycs),
		offsetof(struct sl_thd,wakeup_cycs),
		offsetof(struct sl_thd,timeout_idx)
	};

	printf("***** Checking expected sl_thd offsets *****\n");

	if(sizeof(struct sl_thd) != expected_size) {
		printf("ERROR: sl_thd struct size doesn't match expected.\n\n");
		return -1;
	}

	for (i = 0; i < NUM_FIELDS; i++) {
		if (expected_offsets[i] != actual_offsets[i]) {
			printf("ERROR: sl_thd struct offset of %s doesn't match expected.\n\n", field_names[i]);
			return -1;
		}
	}

	return 0;
}
