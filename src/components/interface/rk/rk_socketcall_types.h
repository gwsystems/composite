#ifndef RK_SOCKETCALL_TYPES_H
#define RK_SOCKETCALL_TYPES_H

/* found this from linux kernel source code: linux/include/uapi/linux/net.h */
#define SOCKETCALL_SOCKET       1               /* sys_socket(2)                */
#define SOCKETCALL_BIND         2               /* sys_bind(2)                  */
#define SOCKETCALL_CONNECT      3               /* sys_connect(2)               */
#define SOCKETCALL_LISTEN       4               /* sys_listen(2)                */
#define SOCKETCALL_ACCEPT       5               /* sys_accept(2)                */
#define SOCKETCALL_GETSOCKNAME  6               /* sys_getsockname(2)           */
#define SOCKETCALL_GETPEERNAME  7               /* sys_getpeername(2)           */
#define SOCKETCALL_SOCKETPAIR   8               /* sys_socketpair(2)            */
#define SOCKETCALL_SEND         9               /* sys_send(2)                  */
#define SOCKETCALL_RECV         10              /* sys_recv(2)                  */
#define SOCKETCALL_SENDTO       11              /* sys_sendto(2)                */
#define SOCKETCALL_RECVFROM     12              /* sys_recvfrom(2)              */
#define SOCKETCALL_SHUTDOWN     13              /* sys_shutdown(2)              */
#define SOCKETCALL_SETSOCKOPT   14              /* sys_setsockopt(2)            */
#define SOCKETCALL_GETSOCKOPT   15              /* sys_getsockopt(2)            */
#define SOCKETCALL_SENDMSG      16              /* sys_sendmsg(2)               */
#define SOCKETCALL_RECVMSG      17              /* sys_recvmsg(2)               */
#define SOCKETCALL_ACCEPT4      18              /* sys_accept4(2)               */
#define SOCKETCALL_RECVMMSG     19              /* sys_recvmmsg(2)              */
#define SOCKETCALL_SENDMMSG     20              /* sys_sendmmsg(2)              */

#endif /* RK_SOCKETCALL_TYPES_H */
