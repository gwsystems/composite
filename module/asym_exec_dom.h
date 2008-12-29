//#ifndef ASYM_EXEC_DOM_H
//#define ASYM_EXEC_DOM_H

/* This is all deadweight now */

#define HIJACK_SWITCH_TO_GUEST_ID 6666
#define TIF_VIRTUAL_SYSCALL     10      /* gabep1@cs.bu.edu: virtual process making a syscall */
#define _TIF_VIRTUAL_SYSCALL	(1<<TIF_VIRTUAL_SYSCALL)
#define TIF_HIJACK_ENV     11           /* gabep1@cs.bu.edu: hijack environment */
#define _TIF_HIJACK_ENV 	(1<<TIF_HIJACK_ENV)

//#endif /* ASYM_EXEC_DOM_H */
