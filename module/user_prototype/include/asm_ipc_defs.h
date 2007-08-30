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
#define SIZEOFUSERCAP 16
#define INVFN 0
#define ENTRYFN 4
#define INVOCATIONCNT 8
#define CAPNUM 12


