#!/bin/sh

gcc -funit-at-a-time -Wall -Wextra -Wframe-larger-than=256 -Wconversion -ggdb3 -I. -I/home/gparmer/research/composite/src/kernel/include/shared/ -I/home/gparmer/research/composite/src/kernel/include/ -o kern kern.c livenss_tbl.c
