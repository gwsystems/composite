#!/bin/bash

cp ../../../../../../apps/hello/hello.bin .


# Defined in both cos and rk, localize one of them.
objcopy -L memmove   rump_boot.o
objcopy -L munmap    rump_boot.o
objcopy -L memset    rump_boot.o
objcopy -L memcpy    rump_boot.o
objcopy -L __umoddi3 rump_boot.o
objcopy -L __udivdi3 rump_boot.o
objcopy -L strtol    rump_boot.o
objcopy -L strlen    rump_boot.o
objcopy -L strcmp    rump_boot.o
objcopy -L strncpy   rump_boot.o
objcopy -L __divdi3  rump_boot.o
objcopy -L puts      rump_boot.o
objcopy -L _exit     hello.bin


ld -r -o rumpcos.o hello.bin rump_boot.o

cp rumpcos.o ~/transfer
