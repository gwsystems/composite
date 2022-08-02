#ifndef COS_DEBUG_H
#define COS_DEBUG_H

#include <cos_component.h>
#include <cos_config.h>

#ifdef COMPONENT_ASSERTIONS
#define DEBUG
#endif

#ifndef PRINT_FN
#define PRINT_FN prints
#endif

#include <llprint.h>
/* Convoluted: We need to pass the __LINE__ through 2 macros to get it
 * to expand to a constant string */
#define STRX(x) #x
#define STR(x) STRX(x)
#define debug_print(str) (PRINT_FN(str __FILE__ ":" STR(__LINE__) ".\n"))

static volatile int *volatile_null_ptr = (int *) NULL;

#define BUG()                   \
	do {                        \
		debug_print("BUG @ ");  \
		*volatile_null_ptr = 0; \
	} while (0)

#ifdef DEBUG

#ifndef assert
/*
 * Tell the compiler that we will not return, thus it can make the
 * static assertion that the condition is true past the assertion.
 */
__attribute__((noreturn)) static inline void
__cos_noret(void)
{
	while (1)
		;
}

#ifndef SPIN
#define SPIN() __cos_noret()
#endif

#define assert(node)                                  \
	do {                                              \
		if (unlikely(!(node))) {                      \
			debug_print("FIXME: assert error in @ "); \
			*volatile_null_ptr = 0;                   \
			__cos_noret();                            \
		}                                             \
	} while (0)
#endif /* ifndef assert */

#ifndef BUG_ON
#define BUG_ON(c) assert(!(c))
#endif

#else /* ifndef DEBUG */
#define assert(n)
#define BUG()
#endif /* ifdef DEBUG */


#endif /* ifndef COS_DEBUG_H */
