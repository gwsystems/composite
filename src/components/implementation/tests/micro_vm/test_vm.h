#ifndef TEST_VM_H
#define TEST_VM_H

#define CLIENT_XXX_AEPKEY 1
#define SERVER_XXX_AEPKEY 2

#define SERVER_CORE 0
#define CLIENT_CORE 0

#define TEST_VM_ITERS 100

#define TEST_IPC
#undef IPC_RAW
#define VM_IPC

#undef TEST_INT
#define TEST_INT_PERIOD_US 10000 //10ms

#define SHM_KEY 10

#endif
