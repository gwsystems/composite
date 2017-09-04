/*
 * Test for cos_map.h
 */

#define COS_LINUX_ENV
#include <cos_map.h>

#define ID_NUM (515)

long ids[ID_NUM], answers[ID_NUM];

COS_MAP_CREATE_STATIC(static_map);

#define PRINT(args...) // printf(args)

int
main(void)
{
	int i;

	cos_map_init_static(&static_map);

	printf("Allocated.\n");
	for (i = 0; i < ID_NUM; i++) {
		ids[i]     = cos_map_add(&static_map, (void *)i);
		answers[i] = i;
		PRINT("%d @ %ld\n", i, ids[i]);
	}
	printf("Added.\n");
	for (i = 0; i < ID_NUM; i++) {
		int ret = (int)cos_map_lookup(&static_map, ids[i]);
		if (ret != answers[i]) printf("FAIL: %d != %ld @ %ld\n", ret, answers[i], ids[i]);
		PRINT("%d @ %ld\n", ret, ids[i]);
	}
	printf("Completed Lookups.\n");
	for (i = 0; i < ID_NUM; i++) {
		if (cos_map_del(&static_map, ids[i])) printf("Delete Failure @ %d\n", i);
	}
	printf("Deleted.\n");
	for (i = 0; i < ID_NUM; i++) {
		ids[i]     = cos_map_add(&static_map, (void *)i);
		answers[i] = i;
		PRINT("%d @ %ld\n", i, ids[i]);
	}
	printf("Added.\n");
	for (i = 0; i < ID_NUM; i++) {
		int ret = (int)cos_map_lookup(&static_map, ids[i]);
		if (ret != answers[i]) printf("FAIL: %d != %ld @ %ld\n", ret, answers[i], ids[i]);
		PRINT("%d @ %ld\n", ret, ids[i]);
	}
	printf("Completed Lookups.\n");
	for (i = 0; i < ID_NUM; i++) {
		ids[i]     = cos_map_add(&static_map, (void *)i);
		answers[i] = i;
		PRINT("%d @ %ld\n", i, ids[i]);
	}
	printf("Added.\n");
	for (i = 0; i < ID_NUM; i++) {
		int ret = (int)cos_map_lookup(&static_map, ids[i]);
		if (ret != answers[i]) printf("FAIL: %d != %ld @ %ld\n", ret, answers[i], ids[i]);
		PRINT("%d @ %ld\n", ret, ids[i]);
	}
	printf("Completed Lookups.\n");

	printf("\ndone.\n");

	return 0;
}
