#ifndef CSTUB_H
#define CSTUB_H

/* CSTUB macros marshal component invocations.
 * See CSTUB_FN() and CSTUB_INVOKE() */

/* The IPC api determines the register ABI.
 * input registers:
 * 	cap#    -> eax
 *	sp      -> ebp
 *	1st arg -> ebx
 *	2nd arg -> esi
 *	3rd arg -> edi
 *	4th arg -> edx
 * For a single return value, the output registers are:
 * 	ret	-> eax
 * 	fault	-> ecx
 * Some IPC can return multiple values, and they use additional registers:
 * 	ret2	-> esi
 * 	ret3	-> edi
 */

#define CSTUB_ASM_OUT(_ret, _fault) "=a"(_ret), "=c"(_fault)
#define CSTUB_ASM_OUT_3RETS(_ret0, _fault, _ret1, _ret2) "=a"(_ret0), "=c"(_fault), "=S"(_ret1), "=D"(_ret2)

/* input registers */
#define CSTUB_ASM_IN_0(_uc) "a"(_uc->cap_no)
#define CSTUB_ASM_IN_1(_uc, first) CSTUB_ASM_IN_0(_uc), "b"(first)
#define CSTUB_ASM_IN_2(_uc, first, second) CSTUB_ASM_IN_1(_uc, first), "S"(second)
#define CSTUB_ASM_IN_3(_uc, first, second, third) CSTUB_ASM_IN_2(_uc, first, second), "D"(third)
#define CSTUB_ASM_IN_4(_uc, first, second, third, fourth) CSTUB_ASM_IN_3(_uc, first, second, third), "d"(fourth)

/* Push the input registers onto stack before asm body */
#define CSTUB_ASM_BODY()               \
	"pushl %%ebx\n\t"              \
	"pushl %%esi\n\t"              \
	"pushl %%edi\n\t"              \
	"pushl %%edx\n\t"              \
	"pushl %%ebp\n\t"              \
	"movl %%esp, %%ebp\n\t"        \
	"movl $1f, %%ecx\n\t"          \
	"sysenter\n\t"                 \
	".align 8\n\t"                 \
	"jmp 2f\n\t"                   \
	".align 8\n\t"                 \
	"1:\n\t"                       \
	"movl $1, %%ecx\n\t"           \
	"jmp 3f\n\t"                   \
	"2:\n\t"                       \
	"call SS_ipc_client_fault\n\t" \
	"movl %%eax, %%ecx\n\t"        \
	"3:\n\t"

/* Pop the inputs back, note reverse order from push */
#define CSTUB_ASM_POST() \
	"popl %%ebp\n\t" \
	"popl %%edx\n\t" \
	"popl %%edi\n\t" \
	"popl %%esi\n\t" \
	"popl %%ebx\n\t"

#define CSTUB_ASM_POST_3RETS() \
	"popl %%ebp\n\t"       \
	"popl %%edx\n\t"       \
	"popl %%ebx\n\t"       \
	"popl %%ebx\n\t"       \
	"popl %%ebx\n\t"

#define CSTUB_ASM(_narg, _ret, _fault, ...)                      \
	__asm__ __volatile__(CSTUB_ASM_BODY() CSTUB_ASM_POST()   \
	                     : CSTUB_ASM_OUT(_ret, _fault)       \
	                     : CSTUB_ASM_IN_##_narg(__VA_ARGS__) \
	                     : "memory", "cc")

#define CSTUB_ASM_3RETS(_narg, _ret0, _fault, _ret1, _ret2, ...)                \
	__asm__ __volatile__(CSTUB_ASM_BODY() CSTUB_ASM_POST_3RETS()            \
	                     : CSTUB_ASM_OUT_3RETS(_ret0, _fault, _ret1, _ret2) \
	                     : CSTUB_ASM_IN_##_narg(__VA_ARGS__)                \
	                     : "memory", "cc")


/* Use CSTUB_INVOKE() to make a capability invocation with _uc.
 *	_ret: output return variable
 *	_fault: output fault variable
 *	_uc: a usr_inv_cap
 *	_narg: the number of args in ...
 *		* NOTE: _narg must be a literal constant integer
 */
#define CSTUB_INVOKE(_ret, _fault, _uc, _narg, ...) CSTUB_ASM(_narg, _ret, _fault, _uc, __VA_ARGS__)

#define CSTUB_INVOKE_3RETS(_ret0, _fault, _ret1, _ret2, _uc, _narg, ...) \
	CSTUB_ASM_3RETS(_narg, _ret0, _fault, _ret1, _ret2, _uc, __VA_ARGS__)


/* Use CSTUB_FN() to declare a function that is going to use CSTUB_INVOKE.
 *	type: return type of function
 *	name: name of function corresponding to the invocation
 */
#define CSTUB_FN(type, name) __attribute__((regparm(1))) type name##_call

#endif /* CSTUB_H */
