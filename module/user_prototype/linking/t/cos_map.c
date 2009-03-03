/* 
 * Test for cos_map.h
 */

#define COS_LINUX_ENV
#define COS_MAP_DYNAMIC
#include <cos_map.h>

#define ID_NUM 300
#define OFFSET 50
#define STATIC_SZ 256
#define DYNAMIC_SZ 500

long ids[ID_NUM];

COS_MAP_STATIC_CREATE(static_map, STATIC_SZ);
cos_map_t *dynamic_map;

int main(void)
{
	int i, s_prev = 0, d_prev = 0;

	cos_map_init(&static_map, STATIC_SZ);
	dynamic_map = cos_map_alloc_map(DYNAMIC_SZ);

	for (i = 0 ; i < ID_NUM ; i++) {
		cos_map_add(&static_map, (void*)(i+1));
		cos_map_add(dynamic_map, (void*)(i+1));
	}

	for (i = 0 ; i < ID_NUM ; i++) {
		long j = (i + OFFSET) % STATIC_SZ, k = (i + OFFSET) % DYNAMIC_SZ;
		int s, d;

		s = cos_map_lookup(&static_map, j);
		d = cos_map_lookup(dynamic_map, k);

		if (s-s_prev != 1) printf("Static: prev %d, curr %d for iter %d\n", s_prev, s, i);
		if (d-d_prev != 1) printf("Dynamic: prev %d, curr %d for iter %d\n", d_prev, d, i);
		s_prev = s;
		d_prev = d;
	}
	
	for (i = 0 ; i < ID_NUM ; i++) {
		cos_map_del(&static_map, i);
		cos_map_del(dynamic_map, i);
	}

	return 0;
}
