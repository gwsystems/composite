#include "printk.h"
#include "task.h"


uint32_t current_task;

void
task_switch(uint32_t task)
{
  uint32_t *sent;
  
  printk(INFO, "Attempting to switch to task %d\n", task);
  if (task >= MAX_TASKS) printk(ERROR, "Task number is too high (>= %d)\n", MAX_TASKS);
  else if (tasks[task].initialized == 0) printk(ERROR, "Task is not initialized\n");
  else {
    ptd_load(tasks[task].ptd);
    current_task = task;
    sent = (uint32_t*)((KERNEL_TABLES+1)*4096*1024);
    printk(INFO, "Contents of address %x: %x\n", sent, *sent);
  }
}

void
task__init(void)
{
  int i = 0;
  current_task = 0;
  tasks[0].system = 1;
  tasks[0].initialized = 1;
  //ptd_init(tasks[0].ptd);

  for (i = 2; i < MAX_TASKS; i++) {
    tasks[i].system = 0;
    tasks[i].initialized = 0;
    ptd_init(tasks[i].ptd);
    ptd_copy_global(tasks[i].ptd, tasks[0].ptd);
  }

  tasks[1].system = 0;
  tasks[1].initialized = 1;
}
