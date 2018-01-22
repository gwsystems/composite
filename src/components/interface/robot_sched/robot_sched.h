#ifndef ROBOT_SCHED_H
#define ROBOT_SCHED_H

int assign_task(int compid, int x, int y);
int init_map(int height, int width);
int check_quadrant(int x, int y);
int check_green(int *triple, int x, int y);

#endif /* ROBOT_SCHED_H */
