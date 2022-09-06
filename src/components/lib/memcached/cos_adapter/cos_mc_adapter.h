#ifndef COS_MC_ADAPTER_H
#define COS_MC_ADAPTER_H

#include <sys/socket.h>

#define evutil_socket_t int

/* This is the default fd, minic the linstening master thread */
#define COS_MC_LISTEN_FD 0

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))

int cos_getpeername(int fd, struct sockaddr *restrict addr, socklen_t *restrict len);

int cos_printf(const char *fmt,...);

void* cos_select_thd(void);

ssize_t cos_recvfrom(void *c);
ssize_t cos_sendmsg(void *c, struct msghdr *msg, int flags);
#endif /* COS_MC_ADAPTER_H */
