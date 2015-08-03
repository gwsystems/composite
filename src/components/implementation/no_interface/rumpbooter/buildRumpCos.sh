#!/bin/bash

cp ../../../../../../apps/hello/hello.bin .

objcopy -L memmove   rump_boot.o
objcopy -L __umoddi3 rump_boot.o
objcopy -L __udivdi3 rump_boot.o
objcopy -L strtol    rump_boot.o
objcopy -L strlen    rump_boot.o
objcopy -L _exit     hello.bin

ld -r -o rumpcos.o hello.bin rump_boot.o

cp rumpcos.o ~/transfer
