#ifndef RK_TYPES_H
#define RK_TYPES_H

#include <stdlib.h>
#include <string.h>
#include <cos_component.h>

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

/* if t=0, points to mainkey.. so use t+1 always..*/
#define RK_SKEY(i, t) (RK_CLIENT(i) + t + 1)
#define RK_RKEY(i, t) ((1 << 9) | (RK_CLIENT(i) + t + 1))

#define RK_CLIENT_MAX (4)

#define RK_CLIENT_STARTTOK "r"
#define RK_CLIENT_ENDTOK   ","
#define RK_CLIENT_LEN      (2) //upto 2 digits..

static inline int
rk_args_instance(void)
{
	char *arg = cos_init_args();
	char *tok1, *tok2;
	char res[COMP_INFO_INIT_STR_LEN] = { '\0' }, *rs = res;
	int  len = 0, instance = 0;

	if (!arg) return -EINVAL;
	strncpy(rs, arg, COMP_INFO_INIT_STR_LEN);
	tok1 = strtok_r(rs, RK_CLIENT_STARTTOK, &rs);
	if (!strlen(arg) || !tok1 || strcmp(tok1, arg) == 0) return -EINVAL;
	tok2 = strtok_r(tok1, RK_CLIENT_ENDTOK, &tok1);
	if (!tok2 || strlen(tok2) > RK_CLIENT_LEN) return -EINVAL;

	instance = atoi(tok2);
	assert(instance > 0 && instance < RK_CLIENT_MAX);
	printc("%s:%d %s=>%d\n", __func__, __LINE__, arg, instance);

	return instance;
}

#endif /* RK_TYPES_H */
