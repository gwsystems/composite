#ifndef COS_MEMCACHED_H
#define COS_MEMCACHED_H

#define ARRAY_SIZE(array) (sizeof(array)/sizeof(array[0]))

int cos_mc_new_conn(int proto);
int cos_mc_init(int argc, char **argv);
u16_t cos_mc_process_command(int fd, char *r_buf, u16_t r_buf_len, char *w_buf, u16_t w_buf_len);

void mc_test(void);


#endif /* COS_MEMCACHED_H */
