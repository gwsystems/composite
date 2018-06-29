#ifndef RK_H
#define RK_H

int     test_entry(int arg1, int arg2, int arg3, int arg4);
int     test_fs(int arg1, int arg2, int arg3, int arg4);
int     get_boot_done(void);
int     rk_socket(int domain, int type, int protocol);
/* @struct sockaddr is on the shared-mem identified by @shmid */
int     rk_connect(int sockfd, int shmid, unsigned int addrlen);
int     rk_bind(int socketfd, int shdmem_id, unsigned int addrlen);
/* TODO rename parameters to include information about what is being packed */
ssize_t rk_recvfrom(int arg1, int arg2, int arg3);
ssize_t rk_sendto(int arg1, int arg2, int arg3);
int     rk_setsockopt(int arg1, int arg2, int arg3);
int     rk_getsockopt(int sockfd_shmid, int level, int optname);
void   *rk_mmap(int arg1, int arg2, int arg3);
long    rk_write(int arg1, int arg2, int arg3);
ssize_t rk_writev(int fd, int iovcnt, int shmid);
long    rk_read(int arg1, int arg2, int arg3);
int     rk_listen(int arg1, int arg2);
int     rk_clock_gettime(int arg1, int arg2);
int	rk_select(int arg1, int arg2);
int	rk_accept(int arg1, int arg2);
int	rk_open(int arg1, int arg2, int arg3);
int	rk_unlink(int arg1);
int	rk_ftruncate(int arg1, int arg2);
int	rk_getsockname(int arg1, int arg2);
int	rk_getpeername(int arg1, int arg2);
int     rk_getsockopt(int sockfd_shmid, int level, int optname);
int     rk_close(int fd);
/* arg3 is either shmid or an int value in fcntl call */
int     rk_fcntl(int fd, int cmd, int arg3);

#endif /* RK_H */
