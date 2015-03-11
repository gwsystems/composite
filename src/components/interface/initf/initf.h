#ifndef INITF_H
#define INITF_H

#include <cos_component.h>
#include <cos_debug.h>
#include <cbuf.h>

#define MAX_ARGSZ ((int)(2<<10))

int __initf_read(int offset, int cbid, int sz);
/* Get the size of the init file */
int initf_size(void);

/* Copies up to req_size bytes starting at offset into the init file
 * into buf.  Returns how many bytes are copied into buf.  The return
 * value of 0 specifies that there is no more to be read. */
static int
initf_read(int offset, char *buf, int req_sz)
{
        cbuf_t cb;
        int ret, sz = (req_sz > MAX_ARGSZ) ? MAX_ARGSZ : req_sz;
        char *d;

        d = cbuf_alloc_ext(sz, &cb, CBUF_TMEM);
        if (!d) assert(0);
        ret = __initf_read(offset, cb, sz);
        memcpy(buf, d, ret);
        cbuf_free(cb);

        return ret;
}

#endif /* !INITF_H */
