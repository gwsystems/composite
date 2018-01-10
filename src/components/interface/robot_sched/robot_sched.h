#ifndef ROBOT_SCHED_H
#define ROBOT_SCHED_H

typedef struct Task {
    int command;
    int coords[2];
}Task;
//int receive_task(struct Task* task);

int assign_task(struct Task* task);
int init_map(int height, int width);
int check_quadrant(int x, int y);
int check_green(int *triple, int x, int y);
int read_jpeg_file(void);
#endif /* ROBOT_SCHED_H */
