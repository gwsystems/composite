#ifndef INITF_H
#define INITF_H

#include <cos_component.h>
#include <cos_debug.h>

#define MAX_ARGSZ ((int)((2<<10)-sizeof(struct cos_array)))

int __initf_read(int offset, struct cos_array *da);
/* Get the size of the init file */
int initf_size(void);

/* Copies up to req_size bytes starting at offset into the init file
 * into buf.  Returns how many bytes are copied into buf.  The return
 * value of 0 specifies that there is no more to be read. */
static int initf_read(int offset, char *buf, int req_sz)
{
	struct cos_array *da;
	int ret, sz = (req_sz > MAX_ARGSZ) ? MAX_ARGSZ : req_sz;
	
	da = cos_argreg_alloc(sz + sizeof(struct cos_array));
	if (!da) BUG();
	da->sz = sz;
	ret = __initf_read(offset, da);
	memcpy(buf, da->mem, ret);
	cos_argreg_free(da);

	return ret;
}

#endif /* !INITF_H */
