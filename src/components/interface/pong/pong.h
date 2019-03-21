#ifndef PONG_H
#define PONG_H

#include <cos_component.h>

void call(void);
int call_ret(void);
int call_arg(int p1);
int call_args(int p1, int p2, int p3, int p4);
int call_argsrets(int p0, int p1, int p2, int p3, int *r0, int *r1);
int call_subset(unsigned long p0, unsigned long p1, unsigned long *r0);
thdid_t call_ids(compid_t *client, compid_t *serv);

#endif /* PONG_H */
