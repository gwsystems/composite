.text
.globl strchr

strchr:
	.PROC
	.CALLINFO
	.ENTRY
	depw,z %r25, 23, 24, %r23
	or %r23, %r25, %r23
	depw,z %r23, 15, 16, %r21
	or %r23, %r21, %r23
	ldi 3, %r19
	and,<> %r26, %r19, %r20
	b,n .Lmdoit
.Lsingle:
	ldb,ma 1(%r26), %r20
	cmpb,=,n %r20, %r25, .Lrfound
	and,= %r26, %r19, %r0
	b,n .Lsingle
.Lmdoit:
	ldw,ma 4(%r26), %r20
	uxor,nbz %r20, %r23, %r0
	b,n .Lfoobar
	uxor,sbz %r0, %r20, %r0
	b,n .Lmdoit
	bv %r0(%rp)
	or %r0, %r0, %r28
.Lfoobar:
	addi -4, %r26, %r26
.Lfound:
	extrw,u %r20, 7, 8, %r19
	depw %r20, 23, 24, %r20
	cmpb,<> %r25, %r19, .Lfound
	addi 1, %r26, %r26
.Lrfound:
	bv %r0(%rp)
	addi -1, %r26, %r28
	.EXIT
	.PROCEND
	.EXPORT strchr
