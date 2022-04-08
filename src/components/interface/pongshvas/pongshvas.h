#ifndef PONGSHVAS_H
#define PONGSHVAS_H

#include <cos_component.h>

void pongshvas_call(void);
int pongshvas_ret(void);
int pongshvas_arg(int p1);
int pongshvas_args(int p1, int p2, int p3, int p4);
int pongshvas_wideargs(long long p0, long long p1);
int pongshvas_argsrets(int p0, int p1, int p2, int p3, word_t *r0, word_t *r1);
long long pongshvas_widerets(long long p1, long long p3);
int pongshvas_subset(unsigned long p0, unsigned long p1, unsigned long *r0);
thdid_t pongshvas_ids(compid_t *client, compid_t *serv);

#endif /* PONGSHVAS_H */
