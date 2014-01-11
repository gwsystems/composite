#ifndef _TASK_H
#define _TASK_H

#include "vm.h"
#include "types.h"

#define MAX_TASKS 32

typedef struct task {
  uint32_t system      :1;       // 0 = user mode; 1 = system mode
  uint32_t initialized :1;
  ptd_t ptd __attribute__((aligned(4096)));
} task_t;

task_t tasks[MAX_TASKS];

void task_switch(uint32_t task);

void task__init(void);

#endif
