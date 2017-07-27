#include <cos_component.h>
#include <initf.h>
#include <sched.h>

__attribute__((weak)) int _binary_init_tar_start = 0;
__attribute__((weak)) int _binary_init_tar_size  = 0;

struct init_fs_info {
	int size;
	char *start;
} info;

int
__initf_read(int offset, int cbid, int sz)
{
	int max = sz, amnt, left;
	char *buf;

	if (offset > info.size) return 0;

	left = info.size - offset;
	amnt = (left > max) ? max : left;

	buf = cbuf2buf(cbid, amnt);
	if (!buf) assert(0);
	memcpy(buf, info.start+offset, amnt);
	cbuf_free(cbid);

	return amnt;
}

int
initf_size(void)
{
	return info.size;
}

void
cos_init(void)
{
	info.start = (char*)&_binary_init_tar_start;
	info.size  = (int)  &_binary_init_tar_size;
}
