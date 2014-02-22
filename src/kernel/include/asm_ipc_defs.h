/* Definitions */
	
/* the offset on the stack to the fn return address for trust cases */
//#define IPRETURN 12 // saving ecx & edx??
#define IPRETURN 4

/* offsets into the thd_invocation_frame structure */
#define SFRAMEUSR 4
#define SFRAMESP 8
#define SFRAMEIP 12

/* user capability structure offsets */
/* really 16, see below for use (mult index reg by 2) */
#define UCAP_SZ 4 		/* # of longs */
#define UCAP_SZ_STR "4"
#define SIZEOFUSERCAP (UCAP_SZ*4)
#define INVFN 0
#define ENTRYFN 4
#define INVOCATIONCNT 8
#define CAPNUM 12

/* offsets into syscall integer */
#define COS_ASYNC_CAP_FLAG_BIT 32 /* async cap flag -> 32 */
#define COS_ASYNC_CAP_FLAG (1<<(COS_ASYNC_CAP_FLAG_BIT-1))
#define COS_CAPABILITY_OFFSET 20 /* bits 20->31 */
#define COS_SYSCALL_OFFSET 15	 /* bits 15->20 */

//#define RET_CAP (INV_CAP_OFFSET-1)

