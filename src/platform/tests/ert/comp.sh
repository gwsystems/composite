#!/bin/sh

gcc -funit-at-a-time -Wall -Wextra -ggdb3 -I/home/gparmer/research/composite/src/kernel/include/shared/ -I/home/gparmer/research/composite/src/components/include/ -o ert ert.c 
