
#ifndef ROBOT_CONT_H
#define ROBOT_CONT_H

#include <robot_sched.h>
void diagonal(int dx, int dy);
int receive_task(struct Task* task);
int* get_loc(void);
int send_task(int *curr, struct Task *task);
int send_cmd(int x);

#endif /* ROBOT_CONT_H */
