#include <cos_component.h>
#include <llprint.h>
#include <cos_types.h>

/* 
 * USE THE chkpt.toml RUNSCRIPT FOR THIS TEST TO PASS
 * UNCOMMENT THE #DEFINE ENABLE_CHKPT IN llbooter.c FOR THIS TEST TO PASS
 * Component IDs are hardcoded: 
 * we must be creating a checkpoint of component with ID 2
 * and the component created from the checkpoint must have ID 3
 */
static compid_t test_compid = 2;
static compid_t chkpt_compid = 3;

static u32_t test_var = 2;

void
cos_init(void)
{
	assert(cos_compid() == test_compid);
	assert(test_var == 2);
	test_var = 5;
}

void
cos_parallel_init(coreid_t cid, int init_core, int ncores)
{
	if (init_core) {
		assert(cos_compid() == test_compid);
	}
}

void
parallel_main(coreid_t cid)
{
	assert(cos_compid() == chkpt_compid);
	assert(test_var == 5);

	printc("Success: Checkpoint created and executed\n");
}
