#ifndef VOTER_H
#define VOTER_H

typedef enum {
	none = 0,
	ping, 
	pong
} replica_type;

// to be fleshed out at a later time - long term solution is to have data in cbufs
int nread(spdid_t spdid, replica_type from, int data);
int nwrite(spdid_t spdid, replica_type to, int data);
int confirm(spdid_t spdid, replica_type type);

#endif /* !VOTER_H */
