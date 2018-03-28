#ifndef RK_H
#define RK_H

int     test_entry(int arg1, int arg2, int arg3, int arg4);
int     test_fs(int arg1, int arg2, int arg3, int arg4);
int     get_boot_done(void);
int     rk_socket(int domain, int type, int protocol);
int     rk_bind(int socketfd, int shdmem_id, unsigned addrlen);
/* TODO rename parameters to include information about what is being packed */
ssize_t rk_recvfrom(int arg1, int arg2, int arg3);
ssize_t rk_sendto(int arg1, int arg2, int arg3);

#endif /* RK_H */
