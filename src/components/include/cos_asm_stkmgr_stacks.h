#include <../../kernel/include/shared/cos_config.h>

// FIXME: From consts.h
#define THD_ID_SHARED_PAGE (1<<30)  // 1 gig

/* sweeney */
//#define ARGREG_ADDRESS (SHARED_REGION_START)

/*
 * TODO: this could all be done without using eax.  If the thdid was
 * pass in as ecx (thus esp as well given sysexit rules), these
 * computations would require 2 instructions only manipulating esp.
 * However, we would now need to pass in the spdid in eax, as ecx
 * (where it is now) would be overloaded.
 */
#define USE_NEW_STACKS 1
#ifdef  USE_NEW_STACKS

#if NUM_CPU_COS > 1
/* We should avoid the lock cmpxchg by using per-core freelist. */
#define COMP_INFO_CMPXCHG                       \
	lock cmpxchgl %esp, cos_comp_info
#else
#define COMP_INFO_CMPXCHG                       \
	cmpxchgl %esp, cos_comp_info
#endif

/* FIXME: Here we have the ABA problem. This simple lock-free
 * operation does not prevent ABA. A possible fix could be
 * integrating a generation number in the lower bits of the 
 * pointer. */
/* clang-format off */
#define COS_ASM_GET_STACK                       \
1:                                              \
        movl %eax, %edx;		        \
8:						\
        /* Check to see if we have a stk */     \
        movl  cos_comp_info, %eax;              \
        testl %eax, %eax;                       \
        je    2f;                               \
                                                \
        /* We have a stack */                   \
        movl (%eax), %esp;                      \
	COMP_INFO_CMPXCHG;                      \
        jnz 8b;					\
5:                                              \
						\
	/* now we have the stack */		\
        movl  %eax, %esp;                       \
        addl  $16, %esp;			\
	movl %edx, %eax;			\
	/* edx has the original eax, which is cpuid+thdid */ \
	shr $16, %eax;                          \
	pushl %eax;	/* cpu id */		\
	movl %edx, %eax;                        \
	andl $0xffff, %eax;			\
	pushl %eax;	/* thd id */		\
        pushl $0x03;    /* flags */             \
	subl $4, %esp;
        /* pushl $0xFACE;  /\* next *\/               */

	
#define COS_ASM_REQUEST_STACK                   \
2:                                              \
        movl %edx, %eax;			\
        /* get stk space */                     \
        movl $stkmgr_stack_space, %esp;         \
	andl $0xffff, %eax;			\
        shl  $7, %eax;                          \
        addl %eax, %esp;                        \
                                                \
        /* save our registers */                \
        pushl %ebp;                             \
        pushl %esi;                             \
        pushl %edi;                             \
        pushl %ebx;                             \
        pushl %ecx;                             \
	pushl %edx;				\
                                                \
        /* Call Stkmgr */                       \
        pushl %ecx;                             \
        call stkmgr_grant_stack;                \
        addl $4, %esp;                          \
                                                \
        /* restore regs */                      \
	popl %edx;				\
        popl %ecx;                              \
        popl %ebx;                              \
        popl %edi;                              \
        popl %esi;                              \
        popl %ebp;                              \
						\
	/* Check for NULL stack return */	\
        testl %eax, %eax;                       \
        je    8b;                               \
	/* Got a stack in eax */		\
	jmp   5b;                                

#define COS_ASM_RET_STACK                       \
                                                \
	movl $0x2, 4(%esp);			\
	movl $0x0, 8(%esp);			\
						\
        /* Put back on free list */             \
	movl %eax, %ebx;			\
9:						\
        movl cos_comp_info, %eax;               \
        movl %eax, (%esp);                      \
	COMP_INFO_CMPXCHG;			\
	jnz 9b;					\
	movl %ebx, %eax;			\
	/* Race? cos_comp_info could change*/	\
        movl $cos_comp_info, %edx;              \
	movl 12(%edx), %edx;			\
        test %edx, %edx;                        \
	je 4f;					\
3:                                              \
        /* stkmgr wants stack back */ 		\
        movl %esp, %edx;                        \
                                                \
        /* Since we are done with this stack    \
           We should not depend on it anymore */\
	movl $THD_ID_SHARED_PAGE, %ebx;         \
        movl (%ebx), %ebx;                      \
    	movl $stkmgr_stack_space, %esp;	        \
        shl $7, %ebx;	                        \
        addl %ebx, %esp;                        \
        /* save our registers */                \
        pushl %ebp;                             \
        pushl %esi;                             \
        pushl %edi;                             \
        pushl %ebx;                             \
        pushl %ecx;                             \
        pushl %eax;                             \
                                                \
        pushl %edx; /* address of structure in stack */      \
        movl cos_comp_info, %ecx;               \
        /*subl $8, %ecx;*/                          \
        /*movl (%ecx), %edx;*/                  \
        pushl %ecx;                             \
        call stkmgr_return_stack;               \
        addl $8, %esp;                          \
                                                \
        /*restore stack */                      \
        popl %eax;                              \
        popl %ecx;                              \
        popl %ebx;                              \
        popl %edi;                              \
        popl %esi;                              \
        popl %ebp;                              \
4:                                              \
        ;
 






/**
 * Assign a thread a stack to execute on.
 */
#define COS_ASM_GET_STACK_OLD                   \
                                                \
        /* Remove me */                         \
        movl %eax, %edx;                        \
    	movl $stkmgr_stack_space, %esp;	        \
        shl $7, %eax;	                        \
        addl %eax, %esp;                        \
        movl %edx, %eax;                        \
                                                \
                                                \
        /*movl $ARGREG_ADDRESS, %edx;           \
        mov  0x4(%edx), %esp;*/                 \
        /* ugh*/                                \
                                                \
        movl %eax, %edx;                        \
        /* do we have a stack free? */          \
        movl cos_comp_info, %eax;               \
        testl %eax, %eax;                       \
        jne 1f;                                 \
                                                \
        /* First we need a stack */             \
        movl $stkmgr_stack_space, %esp;         \
        shl $7, %edx;				\
        addl %edx, %esp;                        \
        /* save our registers */                \
        pushl %ebp;                             \
        pushl %esi;                             \
        pushl %edi;                             \
        pushl %ebx;                             \
        pushl %ecx;                             \
                                                \
        /*                                      \
        movl cos_heap_ptr, %edx;                \
        movl %edx, %eax;                        \
        addl $4096, %edx;                       \
        movl %edx, cos_heap_ptr;                \
        */                                      \
                                                \
        /* call stkmgr_get_stack */             \
        /*pushl %edx;                           \
        pushl %eax;*/                           \
        pushl %ecx;                             \
        call stkmgr_grant_stack;                \
        addl $4, %esp;                          \
                                                \
        /* popl %edx;                           \
        stk is in stk+4                         \
        movl %edx, %eax;*/                      \
        subl $0x4, %eax;                        \
        /*addl $4, %eax;*/                      \
        /*movl %edx, %eax;*/                    \
                                                \
        /*restore stack */                      \
        popl %ecx;                              \
        popl %ebx;                              \
        popl %edi;                              \
        popl %esi;                              \
        popl %ebp;                              \
        /*movl %esp, %eax;*/                    \
        jmp  2f;                                \
                                                \
1:                                              \
        /* stk_list = stk_list->next */         \
        /*                                      \
        SAFE_PUSH_ALL;                          \
        call stkmgr_got_from_list;              \
        SAFE_POP_ALL;*/                         \
        movl cos_comp_info, %eax;               \
        movl (%eax), %edx;                      \
        movl %edx, cos_comp_info;               \
        addl $4, %eax;                          \
        /*SAFE_PUSH_ALL;                          \
        pushl (%eax);                           \
        call stkmgr_return_stack;               \
        addl $4, %esp;                          \
        call stkmgr_got_from_list;              \
        SAFE_PUSH_ALL;*/                          \
        /*SAFE_PUSH_ALL;                        \
        pushl (%eax);                           \
        call stkmgr_return_stack;          \
        addl $4, %esp;                          \
        push %eax;                              \
        call stkmgr_return_stack;           \
        addl $4, %esp;                          \
        call stkmgr_got_from_list;              \
        SAFE_POP_ALL;                           \
        */                                      \
                                                \
2:                                              \
        /*movl %eax, %edx; */                   \
        /*addl $0x4,  %edx;*/                   \
        /*orl  $0x01, %edx;*/ /* mark in use */ \
        /*movl $0xFACE, 0(%edx);*/              \
        /*movl $0xFACE, %eax;*/                 \
        movl %eax, %esp;                        \
        addl  $4, %esp;                         \
        pushl $0x01;    /* flags */             \
        pushl $0xFACE;  /* next */              \
        /*SAFE_PUSH_ALL;                        \
        pushl $0xDEAD;                          \
        pushl (%esp);                           \
        call stkmgr_return_stack;               \
        addl $8, %esp;                          \
        SAFE_POP_ALL;*/


#define COS_ASM_RET_STACK_OLD                   \
        /*pushl %eax;                           \
        call stkmgr_return_stack;*/             \
        /*addl  $4, %esp;*/                     \
        /* test to see if stkmgr wants it back */\
        /*addl $4, %esp; */                     \
       /* pushl $0xDEAD;                        \
        addl $4, %esp;                          \
        movl 0(%esp), %edx;                     \
        */                                      \
        /*movl 0(%esp), %edx;*/                     \
        /*movl (%edx), %edx;*/                  \
        /*SAFE_PUSH_ALL;                          \
        pushl %edx;                             \
        call stkmgr_return_stack;               \
        addl $4, %esp;                          \
        call stkmgr_hello_world;                \
        SAFE_POP_ALL;*/                           \
                                                \
        addl $4, %esp;                          \
        movl (%esp),%edx;                       \
        andl $0x02, %edx;                       \
        test %edx, %edx;                        \
        jne  1f;                                \
        /*cmpl $0xDEAD, %edx;*/                 \
        /*jne 1f;*/                             \
        addl $4, %esp;                          \
        pushl $0x00;  /* Flag Mark Not in Use */\
        pushl $0xFACE;  /* next */              \
        /*subl $4, %esp;*/                      \
        movl cos_comp_info, %edx;               \
        /*movl (%eax), %eax;*/                  \
        /*movl %edx, 0(%esp);*/                 \
        movl %edx, (%esp);                      \
        movl %esp, cos_comp_info;               \
        jmp  2f;                                \
1:                                              \
        addl $4, %esp;                          \
        movl %esp, %edx;                        \
        push $0x00;  /* free flags */           \
                                                \
        /* Since we are done with this stack    \
           We should not depend on it anymore */\
                                                \
        movl $THD_ID_SHARED_PAGE, %ecx;          \
        movl (%ecx), %ecx;                      \
    	movl $stkmgr_stack_space, %esp;	        \
        shl $7, %ecx;	                        \
        addl %ecx, %esp;                        \
        /* save our registers */                \
        pushl %ebp;                             \
        pushl %esi;                             \
        pushl %edi;                             \
        pushl %ebx;                             \
        pushl %ecx;                             \
        pushl %eax;                             \
                                                \
        pushl %edx; /* address of stack */      \
        movl cos_comp_info, %ecx;               \
        subl $8, %ecx;                          \
        /*movl (%ecx), %edx;*/                  \
        pushl %ecx;                             \
        call stkmgr_return_stack;               \
        addl $8, %esp;				\
                                                \
        /*restore stack */                      \
        popl %eax;                              \
        popl %ecx;                              \
        popl %ebx;                              \
        popl %edi;                              \
        popl %esi;                              \
        popl %ebp;                              \
        /*                                      \
        SAFE_PUSH_ALL;                          \
        pushl %edx;                             \
        call stkmgr_return_stack;               \
        addl $4, %esp;                          \
        SAFE_POP_ALL;*/                         \
2:                                              \
        ;
  


#else

#define COS_ASM_GET_STACK                   \
	movl $cos_static_stack, %esp;	    \
	shl $12, %eax;			    \
	addl %eax, %esp;

/* clang-format on */

#define COS_ASM_RET_STACK

#endif /* USE_NEW_STACKS */
