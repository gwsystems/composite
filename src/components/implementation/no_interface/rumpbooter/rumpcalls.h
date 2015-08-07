#ifndef RUMPCALLS_H
#define RUMPCALLS_H

typedef __builtin_va_list va_list;
#define va_start(v,l)      __builtin_va_start((v),l)
#define va_arg             __builtin_va_arg
#define va_end(va_arg)     __builtin_va_end(va_arg)

struct cos_rumpcalls
{
	unsigned short int (*rump_cos_get_thd_id)(void);
	int    (*rump_vsnprintf)(char* str, size_t size, const char *format, va_list arg_ptr);
	void   (*rump_cos_print)(char s[], int ret);
	int    (*rump_strcmp)(const char *a, const char *b);
	char*  (*rump_strncpy)(char *d, const char *s, unsigned long n);
	void*  (*rump_memcalloc)(unsigned long n, unsigned long size);
	void*  (*rump_memalloc)(unsigned long nbytes, unsigned long align);
};

/* Mapping the functions from rumpkernel to composite */
void cos2rump_setup(void);

void *cos_memcalloc(size_t n, size_t size);
void *cos_memalloc(size_t nbytes, size_t align);

#endif /* RUMPCALLS_H */
