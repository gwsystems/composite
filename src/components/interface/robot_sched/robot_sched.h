#ifndef ROBOT_SCHED_H
#define ROBOT_SCHED_H

typedef struct Task {
    int command;
    int coords[2];
}Task;
//int receive_task(struct Task* task);

int assign_task(struct Task* task);

#endif /* ROBOT_SCHED_H */
