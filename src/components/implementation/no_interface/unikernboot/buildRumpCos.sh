#!/bin/bash

objcopy -L memmove   unikernboot.o
objcopy -L __umoddi3 unikernboot.o
objcopy -L __udivdi3 unikernboot.o
objcopy -L strtol    unikernboot.o
objcopy -L strlen    unikernboot.o
objcopy -L _start    cos.bin
objcopy -L _exit     cos.bin

ld -r -o rumpcos.o cos.bin unikernboot.o

cp rumpcos.o ~/transfer
