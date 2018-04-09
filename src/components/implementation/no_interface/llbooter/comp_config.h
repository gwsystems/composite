#ifndef COMP_CONFIG_H
#define COMP_CONFIG_H

#include <cos_types.h>
#include <consts.h>
#include <string.h>

struct cos_config_info_t *comp_configs[MAX_NUM_COMPS];

enum
{
	SL_RUMPCOS = 0,
	SHMEM,
	SL_TEST_BOOT,
};

/*
 * Define your component configuration here.
 * The name of the cos_config_info struct should be that of the final object file
 * for the component so other users can clearly associate components to their
 * configurations.
 *
 * The first key-value pair should be the object name (same as the name you juse used),
 * for the struct itself. The llbooter leverages this first key-value pair to match up
 * object files to their appropriate components during runtime.
 *
 * After defining the remaining key-value pairs as you like, add your configuration struct
 * to the master list which the llbooter will parse through.
 * Add your own enum entry used to offset the config struct into the master list.
 * This enum is only used within this header file.
 *
 * Use the below configuration for the Rump Kernel as an example.
 */

static struct cos_config_info_t sl_rumpcos;
static struct cos_config_info_t shmem;
static struct cos_config_info_t sl_test_boot;

static void
__init_configs()
{
	/* Rumpkernel configuration information */
	strncpy(sl_rumpcos.kvp[COMP_KEY].key, "Component", KEY_LENGTH);
	strncpy(sl_rumpcos.kvp[COMP_KEY].value, "sl_rumpcos", VALUE_LENGTH);
	strncpy(sl_rumpcos.kvp[GREETING_KEY].key, "Greeting", KEY_LENGTH);
	strncpy(sl_rumpcos.kvp[GREETING_KEY].value, "Config for a RK", VALUE_LENGTH);
	comp_configs[SL_RUMPCOS] = &sl_rumpcos;

	/* Shared memory configuration information */
	strncpy(shmem.kvp[COMP_KEY].key, "Component", KEY_LENGTH);
	strncpy(shmem.kvp[COMP_KEY].value, "shmem", VALUE_LENGTH);
	strncpy(shmem.kvp[GREETING_KEY].key, "Greeting", KEY_LENGTH);
	strncpy(shmem.kvp[GREETING_KEY].value, "Config for shared memory component",
		VALUE_LENGTH);
	comp_configs[SHMEM] = &shmem;

	/* llboter_test configuration information */
	strncpy(sl_test_boot.kvp[COMP_KEY].key, "Component", KEY_LENGTH);
	strncpy(sl_test_boot.kvp[COMP_KEY].value, "sl_test_boot", VALUE_LENGTH);
	strncpy(sl_test_boot.kvp[GREETING_KEY].key, "Greeting", KEY_LENGTH);
	strncpy(sl_test_boot.kvp[GREETING_KEY].value, "Config for sl_test_boot",
		VALUE_LENGTH);
	comp_configs[SL_TEST_BOOT] = &sl_test_boot;
}

#endif /* COMP_CONFIG_H */
