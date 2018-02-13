#ifndef RK_H
#define RK_H

enum rk_inv_ops {
	RK_INV_OP1 = 0,
	RK_INV_OP2,
	RK_GET_BOOT_DONE,
	RK_SOCKET,
	RK_BIND,
	RK_RECVFROM,
	RK_SENDTO,
	RK_LOGDATA,
};

int     rk_entry(int arg1, int arg2, int arg3, int arg4);
int     test_entry(int arg1, int arg2, int arg3, int arg4);
int     test_fs(int arg1, int arg2, int arg3, int arg4);
int     test_shdmem(int shm_id, int arg2, int arg3, int arg4);
int     get_boot_done(void);
int     rk_socket(int domain, int type, int protocol);
int     rk_bind(int socketfd, int shdmem_id, unsigned addrlen);
ssize_t rk_recvfrom(int s, int buff_shdmem_id, size_t len,
		    int flags, int from_shdmem_id, int from_addr_len);
ssize_t rk_sendto(int sockfd, int buff_shdmem_id, size_t len,
		  int flags, int addr_shdmem_id, unsigned addrlen);

#endif /* RK_H */
