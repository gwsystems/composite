#ifndef VOTER_H
#define VOTER_H

// to be fleshed out at a later time - long term solution is to have data in cbufs
int nread(spdid_t spdid, int from, int data);
int nwrite(spdid_t spdid, int to, int data);
int confirm(spdid_t spdid);

#endif /* !VOTER_H */
