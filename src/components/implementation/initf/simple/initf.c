#include <cos_component.h>
#include <initf.h>
#include <sched.h>

struct init_fs_info {
	int size;
	char *start;
} info;

int __initf_read(int offset, struct cos_array *da)
{
	int max = da->sz, amnt, left;
	
	if (offset > info.size) return 0;
	left = info.size - offset;
	amnt = (left > max) ? max : left;

	memcpy(da->mem, info.start+offset, amnt);
	return amnt;
}

int initf_size(void)
{
	return info.size;
}

void cos_init(void)
{
	info.start = (char*)cos_comp_info.cos_poly[0];
	info.size = (int)cos_comp_info.cos_poly[1];
}

void ignore_me(void) { sched_block(cos_spd_id(), 0); }
