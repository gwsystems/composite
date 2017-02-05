#ifndef   	FD_H
#define   	FD_H

int cos_app_open(int type, struct cos_array *data);
int cos_socket(int domain, int type, int protocol);
int cos_listen(int fd, int queue);
int cos_bind(int fd, u32_t ip, u16_t port);
int cos_accept(int fd);
int cos_close(int fd);
int cos_split(int fd);
int cos_write(int fd, char *buf, int sz);
int cos_read(int fd, char *buf, int sz);
int cos_wait(int fd);
int cos_wait_all(void);

#endif 	    /* !FD_H */
