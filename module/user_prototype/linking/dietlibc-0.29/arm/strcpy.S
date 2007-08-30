.text
	.align	2
	.global	strcpy
	.type	 strcpy,function
strcpy:
	mov	ip, r0
#ifndef WANT_SMALL_STRING_ROUTINES
	ands	r2, r1, #3
	ldr	r3, [r1, -r2]!
	movne	r2, r2, lsl #3
	movne	r3, r3, lsr r2
	addne	pc, pc, r2, lsl #1
	mov	r0, r0			@ nop
.Lloop:
	tst	r3, #255
	strb	r3, [ip], #1
	moveqs	pc, lr
	mov	r3, r3, lsr #8
	tst	r3, #255
	strb	r3, [ip], #1
	moveqs	pc, lr
	mov	r3, r3, lsr #8
	tst	r3, #255
	strb	r3, [ip], #1
	moveqs	pc, lr
	mov	r3, r3, lsr #8
	tst	r3, #255
	strb	r3, [ip], #1
	ldrne	r3, [r1, #4]!
#else
.Lloop:
	ldrb	r3, [ip], #1
	strb	r3, [r1], #1
	tst	r3, #255
#endif
	bne	.Lloop
	movs	pc, lr
.Lfe1:
	.size	 strcpy,.Lfe1-strcpy
