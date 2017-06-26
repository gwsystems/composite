#ifndef VOTER_H
#define VOTER_H

typedef unsigned int channel_id;

// to be fleshed out at a later time - long term solution is to have data in cbufs
size_t nread(spdid_t spdid, channel_id from, size_t sz);
int nwrite(spdid_t spdid, channel_id to, size_t sz);
int replica_confirm(spdid_t spdid);

// Todo: make this not look like shitty java getter code
cbuf_t get_write_buf(spdid_t spdid);
cbuf_t get_read_buf(spdid_t spdid);

/* 
 * Confirm to the voter that we are ready to be forked 
 * Needed because we must have created a cbuf_meta_range for forking to succeed
 * Order has to be confirm(), get_write/read_buf, confirm_fork
 */
void confirm_fork(spdid_t spdid);

#endif /* !VOTER_H */
