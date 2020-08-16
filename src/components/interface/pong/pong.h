#ifndef PONG_H
#define PONG_H

#include <cos_component.h>

void pong_call(void);
int pong_ret(void);
int pong_arg(int p1);
int pong_args(int p1, int p2, int p3, int p4);
int pong_wideargs(long long p0, long long p1);
int pong_argsrets(int p0, int p1, int p2, int p3, int *r0, int *r1);
long long pong_widerets(long long p1, long long p3);
int pong_subset(unsigned long p0, unsigned long p1, unsigned long *r0);
thdid_t pong_ids(compid_t *client, compid_t *serv);

#endif /* PONG_H */
