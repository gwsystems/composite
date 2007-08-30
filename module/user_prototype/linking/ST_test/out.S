.text
.globl blah
.align 16
blah:
	movl $ST_user_caps, %eax
	/* eax now holds the usr_inv_cap */
	addl $16, %eax
	jmp *0(%eax)
	

.globl ST_direct_invocation
.align 16
ST_direct_invocation:
	/* increment the invocation count */
	incl 8(%eax)
	/* offset into the user-capability structure and call entry fn */
	jmp *4(%eax)

.data
.align 16
.globl ST_user_caps
ST_user_caps:
.rep 8
.long 4
.endr
ST_user_caps_end:
.section .inv_stk
.globl ST_invocation_stack
ST_invocation_stack:
.long
.globl ST_inv_stk_size
ST_inv_stk_size:
.long
