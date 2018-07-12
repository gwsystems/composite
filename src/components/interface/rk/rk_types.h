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
	RK_CONNECT,
	RK_BIND,
	RK_RECVFROM,
	RK_SENDTO,
	RK_SETSOCKOPT,
	RK_GETSOCKOPT,
	RK_MMAP,
	RK_WRITE,
	RK_WRITEV,
	RK_READ,
	RK_FCNTL,
	RK_LISTEN,
	RK_CLOCK_GETTIME,
	RK_SELECT,
	RK_ACCEPT,
	RK_OPEN,
	RK_CLOSE,
	RK_UNLINK,
	RK_FTRUNCATE,
	RK_GETSOCKNAME,
	RK_GETPEERNAME,
} rk_api_t;

#define RK_API_MAX (RK_GETPEERNAME + 1)
/* FIXME: hardcoded offset.. if these keys overlap, we're screwed.. hard to find bug! */
#define RK_INV_KEY 0
#define RK_CLIENT(i) (RK_INV_KEY | (i << 10))

/* if t=0, points to mainkey.. so use t+1 always..*/
#define RK_SKEY(i, t) (RK_CLIENT(i) + t + 1)
#define RK_RKEY(i, t) ((1 << 9) | (RK_CLIENT(i) + t + 1))

#define RK_CLIENT_MAX (4)

#define RK_CLIENT_STARTTOK 'r'
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

	while ((tok1 = strtok_r(rs, RK_CLIENT_ENDTOK, &rs)) != NULL) {
		if (tok1[0] == RK_CLIENT_STARTTOK) break;
	}
	if (!tok1 || strlen(tok1) == 1 || strlen(tok1) > (RK_CLIENT_LEN + 1)) return -EINVAL;

	instance = atoi(&tok1[1]);
	assert(instance > 0 && instance < RK_CLIENT_MAX);

	return instance;
}

#define RK_RUNTYPE_STARTTOK 'r'
#define RK_RUNTYPE_ENDTOK   ","
#define RK_RUNTYPE_LEN      (1) //1 character Q for QEMU or H for HW

/* return int (typecast to char: 'Q' for QEMU and 'H' for HW on success) */
static inline int
rk_args_runtype(void)
{
	char *arg = cos_init_args();
	char *tok1, *tok2;
	char res[COMP_INFO_INIT_STR_LEN] = { '\0' }, *rs = res;
	int  len = 0;

	if (!arg) return -EINVAL;
	strncpy(rs, arg, COMP_INFO_INIT_STR_LEN);

	while ((tok1 = strtok_r(rs, RK_RUNTYPE_ENDTOK, &rs)) != NULL) {
		if (tok1[0] == RK_RUNTYPE_STARTTOK) break;
	}
	if (!tok1 || strlen(tok1) != (RK_RUNTYPE_LEN + 1)) return -EINVAL;
	if (tok1[1] != 'Q' && tok1[1] != 'H') return -EINVAL;

	return (int)tok1[1];
}

#endif /* RK_TYPES_H */
