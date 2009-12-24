#define __NR_SYSCALL_BASE	0x900000

#define __NR_exit			(__NR_SYSCALL_BASE+  1)
#define __NR_fork			(__NR_SYSCALL_BASE+  2)
#define __NR_read			(__NR_SYSCALL_BASE+  3)
#define __NR_write			(__NR_SYSCALL_BASE+  4)
#define __NR_open			(__NR_SYSCALL_BASE+  5)
#define __NR_close			(__NR_SYSCALL_BASE+  6)
					/* 7 was sys_waitpid */
#define __NR_creat			(__NR_SYSCALL_BASE+  8)
#define __NR_link			(__NR_SYSCALL_BASE+  9)
#define __NR_unlink			(__NR_SYSCALL_BASE+ 10)
#define __NR_execve			(__NR_SYSCALL_BASE+ 11)
#define __NR_chdir			(__NR_SYSCALL_BASE+ 12)
#define __NR_time			(__NR_SYSCALL_BASE+ 13)
#define __NR_mknod			(__NR_SYSCALL_BASE+ 14)
#define __NR_chmod			(__NR_SYSCALL_BASE+ 15)
#define __NR_lchown			(__NR_SYSCALL_BASE+ 16)
					/* 17 was sys_break */
					/* 18 was sys_stat */
#define __NR_lseek			(__NR_SYSCALL_BASE+ 19)
#define __NR_getpid			(__NR_SYSCALL_BASE+ 20)
#define __NR_mount			(__NR_SYSCALL_BASE+ 21)
#define __NR_umount			(__NR_SYSCALL_BASE+ 22)
#define __NR_setuid			(__NR_SYSCALL_BASE+ 23)
#define __NR_getuid			(__NR_SYSCALL_BASE+ 24)
#define __NR_stime			(__NR_SYSCALL_BASE+ 25)
#define __NR_ptrace			(__NR_SYSCALL_BASE+ 26)
#define __NR_alarm			(__NR_SYSCALL_BASE+ 27)
					/* 28 was sys_fstat */
#define __NR_pause			(__NR_SYSCALL_BASE+ 29)
#define __NR_utime			(__NR_SYSCALL_BASE+ 30)
					/* 31 was sys_stty */
					/* 32 was sys_gtty */
#define __NR_access			(__NR_SYSCALL_BASE+ 33)
#define __NR_nice			(__NR_SYSCALL_BASE+ 34)
					/* 35 was sys_ftime */
#define __NR_sync			(__NR_SYSCALL_BASE+ 36)
#define __NR_kill			(__NR_SYSCALL_BASE+ 37)
#define __NR_rename			(__NR_SYSCALL_BASE+ 38)
#define __NR_mkdir			(__NR_SYSCALL_BASE+ 39)
#define __NR_rmdir			(__NR_SYSCALL_BASE+ 40)
#define __NR_dup			(__NR_SYSCALL_BASE+ 41)
#define __NR_pipe			(__NR_SYSCALL_BASE+ 42)
#define __NR_times			(__NR_SYSCALL_BASE+ 43)
					/* 44 was sys_prof */
#define __NR_brk			(__NR_SYSCALL_BASE+ 45)
#define __NR_setgid			(__NR_SYSCALL_BASE+ 46)
#define __NR_getgid			(__NR_SYSCALL_BASE+ 47)
					/* 48 was sys_signal */
#define __NR_geteuid			(__NR_SYSCALL_BASE+ 49)
#define __NR_getegid			(__NR_SYSCALL_BASE+ 50)
#define __NR_acct			(__NR_SYSCALL_BASE+ 51)
#define __NR_umount2			(__NR_SYSCALL_BASE+ 52)
					/* 53 was sys_lock */
#define __NR_ioctl			(__NR_SYSCALL_BASE+ 54)
#define __NR_fcntl			(__NR_SYSCALL_BASE+ 55)
					/* 56 was sys_mpx */
#define __NR_setpgid			(__NR_SYSCALL_BASE+ 57)
					/* 58 was sys_ulimit */
					/* 59 was sys_olduname */
#define __NR_umask			(__NR_SYSCALL_BASE+ 60)
#define __NR_chroot			(__NR_SYSCALL_BASE+ 61)
#define __NR_ustat			(__NR_SYSCALL_BASE+ 62)
#define __NR_dup2			(__NR_SYSCALL_BASE+ 63)
#define __NR_getppid			(__NR_SYSCALL_BASE+ 64)
#define __NR_getpgrp			(__NR_SYSCALL_BASE+ 65)
#define __NR_setsid			(__NR_SYSCALL_BASE+ 66)
#define __NR_sigaction			(__NR_SYSCALL_BASE+ 67)
					/* 68 was sys_sgetmask */
					/* 69 was sys_ssetmask */
#define __NR_setreuid			(__NR_SYSCALL_BASE+ 70)
#define __NR_setregid			(__NR_SYSCALL_BASE+ 71)
#define __NR_sigsuspend			(__NR_SYSCALL_BASE+ 72)
#define __NR_sigpending			(__NR_SYSCALL_BASE+ 73)
#define __NR_sethostname		(__NR_SYSCALL_BASE+ 74)
#define __NR_setrlimit			(__NR_SYSCALL_BASE+ 75)
#define __NR_getrlimit			(__NR_SYSCALL_BASE+ 76)	/* Back compat 2GB limited rlimit */
#define __NR_getrusage			(__NR_SYSCALL_BASE+ 77)
#define __NR_gettimeofday		(__NR_SYSCALL_BASE+ 78)
#define __NR_settimeofday		(__NR_SYSCALL_BASE+ 79)
#define __NR_getgroups			(__NR_SYSCALL_BASE+ 80)
#define __NR_setgroups			(__NR_SYSCALL_BASE+ 81)
#define __NR_select			(__NR_SYSCALL_BASE+ 82)
#define __NR_symlink			(__NR_SYSCALL_BASE+ 83)
					/* 84 was sys_lstat */
#define __NR_readlink			(__NR_SYSCALL_BASE+ 85)
#define __NR_uselib			(__NR_SYSCALL_BASE+ 86)
#define __NR_swapon			(__NR_SYSCALL_BASE+ 87)
#define __NR_reboot			(__NR_SYSCALL_BASE+ 88)
#define __NR_readdir			(__NR_SYSCALL_BASE+ 89)
#define __NR_mmap			(__NR_SYSCALL_BASE+ 90)
#define __NR_munmap			(__NR_SYSCALL_BASE+ 91)
#define __NR_truncate			(__NR_SYSCALL_BASE+ 92)
#define __NR_ftruncate			(__NR_SYSCALL_BASE+ 93)
#define __NR_fchmod			(__NR_SYSCALL_BASE+ 94)
#define __NR_fchown			(__NR_SYSCALL_BASE+ 95)
#define __NR_getpriority		(__NR_SYSCALL_BASE+ 96)
#define __NR_setpriority		(__NR_SYSCALL_BASE+ 97)
					/* 98 was sys_profil */
#define __NR_statfs			(__NR_SYSCALL_BASE+ 99)
#define __NR_fstatfs			(__NR_SYSCALL_BASE+100)
					/* 101 was sys_ioperm */
#define __NR_socketcall			(__NR_SYSCALL_BASE+102)
#define __NR_syslog			(__NR_SYSCALL_BASE+103)
#define __NR_setitimer			(__NR_SYSCALL_BASE+104)
#define __NR_getitimer			(__NR_SYSCALL_BASE+105)
#define __NR_stat			(__NR_SYSCALL_BASE+106)
#define __NR_lstat			(__NR_SYSCALL_BASE+107)
#define __NR_fstat			(__NR_SYSCALL_BASE+108)
					/* 109 was sys_uname */
					/* 110 was sys_iopl */
#define __NR_vhangup			(__NR_SYSCALL_BASE+111)
					/* 112 was sys_idle */
#define __NR_syscall			(__NR_SYSCALL_BASE+113) /* syscall to call a syscall! */
#define __NR_wait4			(__NR_SYSCALL_BASE+114)
#define __NR_swapoff			(__NR_SYSCALL_BASE+115)
#define __NR_sysinfo			(__NR_SYSCALL_BASE+116)
#define __NR_ipc			(__NR_SYSCALL_BASE+117)
#define __NR_fsync			(__NR_SYSCALL_BASE+118)
#define __NR_sigreturn			(__NR_SYSCALL_BASE+119)
#define __NR_clone			(__NR_SYSCALL_BASE+120)
#define __NR_setdomainname		(__NR_SYSCALL_BASE+121)
#define __NR_uname			(__NR_SYSCALL_BASE+122)
					/* 123 was sys_modify_ldt */
#define __NR_adjtimex			(__NR_SYSCALL_BASE+124)
#define __NR_mprotect			(__NR_SYSCALL_BASE+125)
#define __NR_sigprocmask		(__NR_SYSCALL_BASE+126)
#define __NR_create_module		(__NR_SYSCALL_BASE+127)
#define __NR_init_module		(__NR_SYSCALL_BASE+128)
#define __NR_delete_module		(__NR_SYSCALL_BASE+129)
#define __NR_get_kernel_syms		(__NR_SYSCALL_BASE+130)
#define __NR_quotactl			(__NR_SYSCALL_BASE+131)
#define __NR_getpgid			(__NR_SYSCALL_BASE+132)
#define __NR_fchdir			(__NR_SYSCALL_BASE+133)
#define __NR_bdflush			(__NR_SYSCALL_BASE+134)
#define __NR_sysfs			(__NR_SYSCALL_BASE+135)
#define __NR_personality		(__NR_SYSCALL_BASE+136)
					/* 137 was sys_afs_syscall */
#define __NR_setfsuid			(__NR_SYSCALL_BASE+138)
#define __NR_setfsgid			(__NR_SYSCALL_BASE+139)
#define __NR__llseek			(__NR_SYSCALL_BASE+140)
#define __NR_getdents			(__NR_SYSCALL_BASE+141)
#define __NR__newselect			(__NR_SYSCALL_BASE+142)
#define __NR_flock			(__NR_SYSCALL_BASE+143)
#define __NR_msync			(__NR_SYSCALL_BASE+144)
#define __NR_readv			(__NR_SYSCALL_BASE+145)
#define __NR_writev			(__NR_SYSCALL_BASE+146)
#define __NR_getsid			(__NR_SYSCALL_BASE+147)
#define __NR_fdatasync			(__NR_SYSCALL_BASE+148)
#define __NR__sysctl			(__NR_SYSCALL_BASE+149)
#define __NR_mlock			(__NR_SYSCALL_BASE+150)
#define __NR_munlock			(__NR_SYSCALL_BASE+151)
#define __NR_mlockall			(__NR_SYSCALL_BASE+152)
#define __NR_munlockall			(__NR_SYSCALL_BASE+153)
#define __NR_sched_setparam		(__NR_SYSCALL_BASE+154)
#define __NR_sched_getparam		(__NR_SYSCALL_BASE+155)
#define __NR_sched_setscheduler		(__NR_SYSCALL_BASE+156)
#define __NR_sched_getscheduler		(__NR_SYSCALL_BASE+157)
#define __NR_sched_yield		(__NR_SYSCALL_BASE+158)
#define __NR_sched_get_priority_max	(__NR_SYSCALL_BASE+159)
#define __NR_sched_get_priority_min	(__NR_SYSCALL_BASE+160)
#define __NR_sched_rr_get_interval	(__NR_SYSCALL_BASE+161)
#define __NR_nanosleep			(__NR_SYSCALL_BASE+162)
#define __NR_mremap			(__NR_SYSCALL_BASE+163)
#define __NR_setresuid			(__NR_SYSCALL_BASE+164)
#define __NR_getresuid			(__NR_SYSCALL_BASE+165)
					/* 166 was sys_vm86 */
#define __NR_query_module		(__NR_SYSCALL_BASE+167)
#define __NR_poll			(__NR_SYSCALL_BASE+168)
#define __NR_nfsservctl			(__NR_SYSCALL_BASE+169)
#define __NR_setresgid			(__NR_SYSCALL_BASE+170)
#define __NR_getresgid			(__NR_SYSCALL_BASE+171)
#define __NR_prctl			(__NR_SYSCALL_BASE+172)
#define __NR_rt_sigreturn		(__NR_SYSCALL_BASE+173)
#define __NR_rt_sigaction		(__NR_SYSCALL_BASE+174)
#define __NR_rt_sigprocmask		(__NR_SYSCALL_BASE+175)
#define __NR_rt_sigpending		(__NR_SYSCALL_BASE+176)
#define __NR_rt_sigtimedwait		(__NR_SYSCALL_BASE+177)
#define __NR_rt_sigqueueinfo		(__NR_SYSCALL_BASE+178)
#define __NR_rt_sigsuspend		(__NR_SYSCALL_BASE+179)
#define __NR_pread			(__NR_SYSCALL_BASE+180)
#define __NR_pwrite			(__NR_SYSCALL_BASE+181)
#define __NR_chown			(__NR_SYSCALL_BASE+182)
#define __NR_getcwd			(__NR_SYSCALL_BASE+183)
#define __NR_capget			(__NR_SYSCALL_BASE+184)
#define __NR_capset			(__NR_SYSCALL_BASE+185)
#define __NR_sigaltstack		(__NR_SYSCALL_BASE+186)
#define __NR_sendfile			(__NR_SYSCALL_BASE+187)
					/* 188 reserved */
					/* 189 reserved */
#define __NR_vfork			(__NR_SYSCALL_BASE+190)
#define __NR_ugetrlimit			(__NR_SYSCALL_BASE+191)	/* SuS compliant getrlimit */
#define __NR_mmap2			(__NR_SYSCALL_BASE+192)
#define __NR_truncate64			(__NR_SYSCALL_BASE+193)
#define __NR_ftruncate64		(__NR_SYSCALL_BASE+194)
#define __NR_stat64			(__NR_SYSCALL_BASE+195)
#define __NR_lstat64			(__NR_SYSCALL_BASE+196)
#define __NR_fstat64			(__NR_SYSCALL_BASE+197)
#define __NR_lchown32			(__NR_SYSCALL_BASE+198)
#define __NR_getuid32			(__NR_SYSCALL_BASE+199)
#define __NR_getgid32			(__NR_SYSCALL_BASE+200)
#define __NR_geteuid32			(__NR_SYSCALL_BASE+201)
#define __NR_getegid32			(__NR_SYSCALL_BASE+202)
#define __NR_setreuid32			(__NR_SYSCALL_BASE+203)
#define __NR_setregid32			(__NR_SYSCALL_BASE+204)
#define __NR_getgroups32		(__NR_SYSCALL_BASE+205)
#define __NR_setgroups32		(__NR_SYSCALL_BASE+206)
#define __NR_fchown32			(__NR_SYSCALL_BASE+207)
#define __NR_setresuid32		(__NR_SYSCALL_BASE+208)
#define __NR_getresuid32		(__NR_SYSCALL_BASE+209)
#define __NR_setresgid32		(__NR_SYSCALL_BASE+210)
#define __NR_getresgid32		(__NR_SYSCALL_BASE+211)
#define __NR_chown32			(__NR_SYSCALL_BASE+212)
#define __NR_setuid32			(__NR_SYSCALL_BASE+213)
#define __NR_setgid32			(__NR_SYSCALL_BASE+214)
#define __NR_setfsuid32			(__NR_SYSCALL_BASE+215)
#define __NR_setfsgid32			(__NR_SYSCALL_BASE+216)
#define __NR_getdents64			(__NR_SYSCALL_BASE+217)
#define __NR_pivot_root			(__NR_SYSCALL_BASE+218)
#define __NR_mincore			(__NR_SYSCALL_BASE+219)
#define __NR_madvise			(__NR_SYSCALL_BASE+220)
#define __NR_fcntl64			(__NR_SYSCALL_BASE+221)
					/* 222 for tux */
					/* 223 is unused */
#define __NR_gettid			(__NR_SYSCALL_BASE+224)
#define __NR_readahead			(__NR_SYSCALL_BASE+225)
#define __NR_setxattr			(__NR_SYSCALL_BASE+226)
#define __NR_lsetxattr			(__NR_SYSCALL_BASE+227)
#define __NR_fsetxattr			(__NR_SYSCALL_BASE+228)
#define __NR_getxattr			(__NR_SYSCALL_BASE+229)
#define __NR_lgetxattr			(__NR_SYSCALL_BASE+230)
#define __NR_fgetxattr			(__NR_SYSCALL_BASE+231)
#define __NR_listxattr			(__NR_SYSCALL_BASE+232)
#define __NR_llistxattr			(__NR_SYSCALL_BASE+233)
#define __NR_flistxattr			(__NR_SYSCALL_BASE+234)
#define __NR_removexattr		(__NR_SYSCALL_BASE+235)
#define __NR_lremovexattr		(__NR_SYSCALL_BASE+236)
#define __NR_fremovexattr		(__NR_SYSCALL_BASE+237)
#define __NR_tkill			(__NR_SYSCALL_BASE+238)
#define __NR_sendfile64			(__NR_SYSCALL_BASE+239)
#define __NR_futex			(__NR_SYSCALL_BASE+240)
#define __NR_sched_setaffinity		(__NR_SYSCALL_BASE+241)
#define __NR_sched_getaffinity		(__NR_SYSCALL_BASE+242)
#define __NR_io_setup			(__NR_SYSCALL_BASE+243)
#define __NR_io_destroy			(__NR_SYSCALL_BASE+244)
#define __NR_io_getevents		(__NR_SYSCALL_BASE+245)
#define __NR_io_submit			(__NR_SYSCALL_BASE+246)
#define __NR_io_cancel			(__NR_SYSCALL_BASE+247)
#define __NR_exit_group			(__NR_SYSCALL_BASE+248)
#define __NR_lookup_dcookie		(__NR_SYSCALL_BASE+249)
#define __NR_epoll_create		(__NR_SYSCALL_BASE+250)
#define __NR_epoll_ctl			(__NR_SYSCALL_BASE+251)
#define __NR_epoll_wait			(__NR_SYSCALL_BASE+252)
#define __NR_remap_file_pages		(__NR_SYSCALL_BASE+253)
					/* 254 for set_thread_area */
					/* 255 for get_thread_area */
#define __NR_set_tid_address		(__NR_SYSCALL_BASE+256)
#define __NR_timer_create		(__NR_SYSCALL_BASE+257)
#define __NR_timer_settime		(__NR_SYSCALL_BASE+258)
#define __NR_timer_gettime		(__NR_SYSCALL_BASE+259)
#define __NR_timer_getoverrun		(__NR_SYSCALL_BASE+260)
#define __NR_timer_delete		(__NR_SYSCALL_BASE+261)
#define __NR_clock_settime		(__NR_SYSCALL_BASE+262)
#define __NR_clock_gettime		(__NR_SYSCALL_BASE+263)
#define __NR_clock_getres		(__NR_SYSCALL_BASE+264)
#define __NR_clock_nanosleep		(__NR_SYSCALL_BASE+265)
#define __NR_statfs64			(__NR_SYSCALL_BASE+266)
#define __NR_fstatfs64			(__NR_SYSCALL_BASE+267)
#define __NR_tgkill			(__NR_SYSCALL_BASE+268)
#define __NR_utimes			(__NR_SYSCALL_BASE+269)
#define __NR_fadvise64			(__NR_SYSCALL_BASE+270)
#define __NR_pciconfig_iobase		(__NR_SYSCALL_BASE+271)
#define __NR_pciconfig_read		(__NR_SYSCALL_BASE+272)
#define __NR_pciconfig_write		(__NR_SYSCALL_BASE+273)
#define __NR_mq_open			(__NR_SYSCALL_BASE+274)
#define __NR_mq_unlink			(__NR_SYSCALL_BASE+275)
#define __NR_mq_timedsend		(__NR_SYSCALL_BASE+276)
#define __NR_mq_timedreceive		(__NR_SYSCALL_BASE+277)
#define __NR_mq_notify			(__NR_SYSCALL_BASE+278)
#define __NR_mq_getsetattr		(__NR_SYSCALL_BASE+279)
#define __NR_waitid			(__NR_SYSCALL_BASE+280)


/* ok the next few values are for the optimization of the unified syscalls
 * on arm.
 * If the syscall has #arguments
 *	<=4 set to 0
 *	 >4 set to 1
 *
 * Since the majority of the syscalls need <=4 arguments this saves a lot
 * of byte (12 per syscall) and cycles (~16)
 */
#define __ARGS_exit			0
#define __ARGS_fork			0
#define __ARGS_read			0
#define __ARGS_write			0
#define __ARGS_open			0
#define __ARGS_close			0
#define __ARGS_waitpid			0
#define __ARGS_creat			0
#define __ARGS_link			0
#define __ARGS_unlink			0
#define __ARGS_execve			0
#define __ARGS_chdir			0
#define __ARGS_time			0
#define __ARGS_mknod			0
#define __ARGS_chmod			0
#define __ARGS_lchown			0
#define __ARGS_break			0

#define __ARGS_lseek			0
#define __ARGS_getpid			0
#define __ARGS_mount			1
#define __ARGS_umount			0
#define __ARGS_setuid			0
#define __ARGS_getuid			0
#define __ARGS_stime			0
#define __ARGS_ptrace			0
#define __ARGS_alarm			0

#define __ARGS_pause			0
#define __ARGS_utime			0
#define __ARGS_stty			0
#define __ARGS_gtty			0
#define __ARGS_access			0
#define __ARGS_nice			0
#define __ARGS_ftime			0
#define __ARGS_sync			0
#define __ARGS_kill			0
#define __ARGS_rename			0
#define __ARGS_mkdir			0
#define __ARGS_rmdir			0
#define __ARGS_dup			0
#define __ARGS_pipe			0
#define __ARGS_times			0
#define __ARGS_prof			0
#define __ARGS_brk			0
#define __ARGS_setgid			0
#define __ARGS_getgid			0
#define __ARGS_signal			0
#define __ARGS_geteuid			0
#define __ARGS_getegid			0
#define __ARGS_acct			0
#define __ARGS_umount2			0
#define __ARGS_lock			0
#define __ARGS_ioctl			0
#define __ARGS_fcntl			0
#define __ARGS_mpx			0
#define __ARGS_setpgid			0
#define __ARGS_ulimit			0

#define __ARGS_umask			0
#define __ARGS_chroot			0
#define __ARGS_ustat			0
#define __ARGS_dup2			0
#define __ARGS_getppid			0
#define __ARGS_getpgrp			0
#define __ARGS_setsid			0
#define __ARGS_sigaction		0
#define __ARGS_sgetmask			0
#define __ARGS_ssetmask			0
#define __ARGS_setreuid			0
#define __ARGS_setregid			0
#define __ARGS_sigsuspend		0
#define __ARGS_sigpending		0
#define __ARGS_sethostname		0
#define __ARGS_setrlimit		0
#define __ARGS_getrlimit		0
#define __ARGS_getrusage		0
#define __ARGS_gettimeofday		0
#define __ARGS_settimeofday		0
#define __ARGS_getgroups		0
#define __ARGS_setgroups		0
#define __ARGS_select			0
#define __ARGS_symlink			0

#define __ARGS_readlink			0
#define __ARGS_uselib			0
#define __ARGS_swapon			0
#define __ARGS_reboot			0
#define __ARGS_readdir			0
#define __ARGS_mmap			0	/* this is NOT 1 !!! (special case) */
#define __ARGS_munmap			0
#define __ARGS_truncate			0
#define __ARGS_ftruncate		0
#define __ARGS_fchmod			0
#define __ARGS_fchown			0
#define __ARGS_getpriority		0
#define __ARGS_setpriority		0
#define __ARGS_profil			0
#define __ARGS_statfs			0
#define __ARGS_fstatfs			0
#define __ARGS_ioperm			0
#define __ARGS_socketcall		0
#define __ARGS_syslog			0
#define __ARGS_setitimer		0
#define __ARGS_getitimer		0
#define __ARGS_stat			0
#define __ARGS_lstat			0
#define __ARGS_fstat			0


#define __ARGS_vhangup			0
#define __ARGS_idle			0
#define __ARGS_syscall			0
#define __ARGS_wait4			0
#define __ARGS_swapoff			0
#define __ARGS_sysinfo			0
#define __ARGS_ipc			1
#define __ARGS_fsync			0
#define __ARGS_sigreturn		0
#define __ARGS_clone			0
#define __ARGS_setdomainname		0
#define __ARGS_uname			0
#define __ARGS_modify_ldt		0
#define __ARGS_adjtimex			0
#define __ARGS_mprotect			0
#define __ARGS_sigprocmask		0
#define __ARGS_create_module		0
#define __ARGS_init_module		0
#define __ARGS_delete_module		0
#define __ARGS_get_kernel_syms		0
#define __ARGS_quotactl			0
#define __ARGS_getpgid			0
#define __ARGS_fchdir			0
#define __ARGS_bdflush			0
#define __ARGS_sysfs			0
#define __ARGS_personality		0
#define __ARGS_afs_syscall		0
#define __ARGS_setfsuid			0
#define __ARGS_setfsgid			0
#define __ARGS__llseek			1
#define __ARGS_getdents			0
#define __ARGS__newselect		1
#define __ARGS_flock			0
#define __ARGS_msync			0
#define __ARGS_readv			0
#define __ARGS_writev			0
#define __ARGS_getsid			0
#define __ARGS_fdatasync		0
#define __ARGS__sysctl			0
#define __ARGS_mlock			0
#define __ARGS_munlock			0
#define __ARGS_mlockall			0
#define __ARGS_munlockall		0
#define __ARGS_sched_setparam		0
#define __ARGS_sched_getparam		0
#define __ARGS_sched_setscheduler	0
#define __ARGS_sched_getscheduler	0
#define __ARGS_sched_yield		0
#define __ARGS_sched_get_priority_max	0
#define __ARGS_sched_get_priority_min	0
#define __ARGS_sched_rr_get_interval	0
#define __ARGS_nanosleep		0
#define __ARGS_mremap			0
#define __ARGS_setresuid		0
#define __ARGS_getresuid		0
#define __ARGS_vm86			0
#define __ARGS_query_module		1
#define __ARGS_poll			0
#define __ARGS_nfsservctl		0
#define __ARGS_setresgid		0
#define __ARGS_getresgid		0
#define __ARGS_prctl			1
#define __ARGS_rt_sigreturn		0
#define __ARGS_rt_sigaction		0
#define __ARGS_rt_sigprocmask		0
#define __ARGS_rt_sigpending		0
#define __ARGS_rt_sigtimedwait		0
#define __ARGS_rt_sigqueueinfo		0
#define __ARGS_rt_sigsuspend		0
#define __ARGS_pread			0
#define __ARGS_pwrite			0
#define __ARGS_chown			0
#define __ARGS_getcwd			0
#define __ARGS_capget			0
#define __ARGS_capset			0
#define __ARGS_sigaltstack		0
#define __ARGS_sendfile			0

#define __ARGS_vfork			0
#define __ARGS_ugetrlimit		0
#define __ARGS_mmap2			1
#define __ARGS_truncate64		0
#define __ARGS_ftruncate64		0
#define __ARGS_stat64			0
#define __ARGS_lstat64			0
#define __ARGS_fstat64			0
#define __ARGS_lchown32			0
#define __ARGS_getuid32			0
#define __ARGS_getgid32			0
#define __ARGS_geteuid32		0
#define __ARGS_getegid32		0
#define __ARGS_setreuid32		0
#define __ARGS_setregid32		0
#define __ARGS_getgroups32		0
#define __ARGS_setgroups32		0
#define __ARGS_fchown32			0
#define __ARGS_setresuid32		0
#define __ARGS_getresuid32		0
#define __ARGS_setresgid32		0
#define __ARGS_getresgid32		0
#define __ARGS_chown32			0
#define __ARGS_setuid32			0
#define __ARGS_setgid32			0
#define __ARGS_setfsuid32		0
#define __ARGS_setfsgid32		0
#define __ARGS_getdents64		0
#define __ARGS_pivot_root		0
#define __ARGS_mincore			0
#define __ARGS_madvise			0
#define __ARGS_fcntl64			0

#define __ARGS_security			0
#define __ARGS_gettid			0
#define __ARGS_readahead		0
#define __ARGS_setxattr			1
#define __ARGS_lsetxattr		1
#define __ARGS_fsetxattr		1
#define __ARGS_getxattr			0
#define __ARGS_lgetxattr		0
#define __ARGS_fgetxattr		0
#define __ARGS_listxattr		0
#define __ARGS_llistxattr		0
#define __ARGS_flistxattr		0
#define __ARGS_removexattr		0
#define __ARGS_lremovexattr		0
#define __ARGS_fremovexattr		0
#define __ARGS_tkill			0

#define __ARGS_sendfile64		0
#define __ARGS_futex			0
#define __ARGS_sched_setaffinity	0
#define __ARGS_sched_getaffinity	0
#define __ARGS_io_setup			0
#define __ARGS_io_destroy		0
#define __ARGS_io_getevents		0
#define __ARGS_io_submit		0
#define __ARGS_io_cancel		0
#define __ARGS_exit_group		0
#define __ARGS_lookup_dcookie		0
#define __ARGS_epoll_create		0
#define __ARGS_epoll_ctl		0
#define __ARGS_epoll_wait		0
#define __ARGS_remap_file_pages		0
#define __ARGS_set_thread_area		0
#define __ARGS_get_thread_area		0
#define __ARGS_set_tid_address		0

#define __ARGS_timer_create		0
#define __ARGS_timer_settime		0
#define __ARGS_timer_gettime		0
#define __ARGS_timer_getoverrun		0
#define __ARGS_timer_delete		0
#define __ARGS_clock_settime		0
#define __ARGS_clock_gettime		0
#define __ARGS_clock_getres		0
#define __ARGS_clock_nanosleep		0
#define __ARGS_statfs64			0
#define __ARGS_fstatfs64		0
#define __ARGS_tgkill			0
#define __ARGS_utimes			0

#define __ARGS_fadvise64_64		0
#define __ARGS_pciconfig_iobase		0
#define __ARGS_pciconfig_read		1
#define __ARGS_pciconfig_write		1
#define __ARGS_mq_open			0
#define __ARGS_mq_unlink		0
#define __ARGS_mq_timedsend		0
#define __ARGS_mq_timedreceive		1
#define __ARGS_mq_notify		0
#define __ARGS_mq_getsetattr		0
#define __ARGS_waitid			0

#ifdef __ASSEMBLER__
#define syscall_weak(name,wsym,sym) __syscall_weak $__NR_##name, wsym, sym, __ARGS_##name
.macro __syscall_weak name wsym sym typ
.text
.type \wsym,function
.weak \wsym
\wsym:
.type \sym,function
.global \sym
\sym:
.ifgt \typ
	mov	ip, sp
	stmfd	sp!,{r4, r5, r6}
	ldmia	ip, {r4, r5, r6}
.endif
	swi	\name
.ifgt \typ
	b	__unified_syscall4
.else
	b	__unified_syscall
.endif
.endm

#define syscall(name,sym) __syscall $__NR_##name, sym, __ARGS_##name
.macro __syscall name sym typ
.text
.type \sym,function
.global \sym
\sym:
.ifgt \typ
	mov	ip, sp
	stmfd	sp!,{r4, r5, r6}
	ldmia	ip, {r4, r5, r6}
.endif
	swi	\name
.ifgt \typ
	b	__unified_syscall4
.else
	b	__unified_syscall
.endif
.endm

#endif
