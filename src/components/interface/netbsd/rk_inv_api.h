#ifndef RK_INV_API_H
#define RK_INV_API_H

#include <sys/socket.h>

int rk_inv_op1(void);
void rk_inv_op2(int shmid); /* OP TEST */
int rk_inv_get_boot_done(void);
int rk_inv_socket(int domain, int type, int protocol);
int rk_inv_bind(int sockfd, int shdmem_id, socklen_t addrlen);
ssize_t rk_inv_recvfrom(int s, int buff_shdmem_id, size_t len, int flags, int from_shdmem_id, int fromlenaddr_shdmem_id);
int rk_inv_logdata(void); /* FIXME: using ringbuff sharedmem */
void *rk_inv_mmap(void *addr, size_t len, int prot, int flags, int fd, long long off);

int rk_socketcall(int callid, unsigned long *cargs);
int rk_libcmod_init(void);

#endif /* RK_INV_API_H */
