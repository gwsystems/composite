#ifndef TEST_VM_H
#define TEST_VM_H

#define CLIENT_XXX_AEPKEY 1
#define SERVER_XXX_AEPKEY 2

#define SERVER_CORE 0
#define CLIENT_CORE 0

#define TEST_VM_ITERS 1000

#undef TEST_IPC
#undef IPC_RAW
#undef VM_IPC

#define TEST_INT
#define TEST_INT_PERIOD_US 10000 //10ms

#endif
