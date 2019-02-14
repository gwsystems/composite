.text
.globl thd_dispatch_slowpath
.type thd_dispatch_slowpath, @function
thd_dispatch_slowpath:
	pushl %edi
	pushl %esi
	call sl_thd_dispatch_slowpath
	popl %esi
	popl %edi
