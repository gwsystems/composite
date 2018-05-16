#ifndef RK_TYPES_H
#define RK_TYPES_H

typedef enum {
	TEST_ENTRY = 0,
	TEST_FS,
	GET_BOOT_DONE,
	RK_SOCKET,
	RK_BIND,
	RK_RECVFROM,
	RK_SENDTO,
	RK_SETSOCKOPT,
	RK_MMAP,
	RK_WRITE,
	RK_READ,
	RK_LISTEN,
	RK_CLOCK_GETTIME,
	RK_SELECT,
	RK_ACCEPT,
	RK_OPEN,
	RK_UNLINK,
	RK_FTRUNCATE,
	RK_GETSOCKNAME,
	RK_GETPEERNAME,
} rk_api_t;

#define RK_API_MAX (RK_GETPEERNAME + 1)
/* FIXME: hardcoded offset.. if these keys overlap, we're screwed.. hard to find bug! */
#define RK_INV_KEY 'R'
#define RK_CLIENT(i) (RK_INV_KEY | (i << 10))

#define RK_SKEY(i, t) (RK_CLIENT(i) + (t + 1))
#define RK_RKEY(i, t) ((1 << 9) | (RK_CLIENT(i) + (t + 1)))

#define RK_CLIENT_MAX 3

#endif /* RK_TYPES_H */
