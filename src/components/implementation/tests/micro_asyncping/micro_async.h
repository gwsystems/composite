#ifndef MICRO_ASYNC_H
#define MICRO_ASYNC_H

#include <cos_types.h>

#define CLIENT_CORE 0
#define SERVER_CORE 1

#define CLIENT_KEY 10
#define SERVER_KEY 20

#define TEST_ITERS 100000

#define TEST_BUDGET 10000
#define TEST_WINDOW 10000
#define TEST_PRIO 1
#define TEST_SLEEP 500

#define SHMEM_KEY 30

#define IPC_UBENCH
#define IPC_RAW

#define CLIENT_READY(addr) ((unsigned long *)addr)
#define SERVER_READY(addr) ((unsigned long *)(addr+1))

#define IPC_TSC_ADDR(addr) ((cycles_t *)(addr + 2))

#endif /* MICRO_ASYNC_H */
