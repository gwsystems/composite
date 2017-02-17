#ifndef VOTER_H
#define VOTER_H

typedef enum {
	ping = 0, 
	pong
} replica_type;

// to be fleshed out at a later time - long term solution is to have data in cbufs
int nread(spdid_t spdid, int from, int data);
int nwrite(spdid_t spdid, int to, int data);
int confirm(spdid_t spdid, replica_type type);

#endif /* !VOTER_H */
