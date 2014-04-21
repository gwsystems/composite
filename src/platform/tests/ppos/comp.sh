#!/bin/sh

gcc -funit-at-a-time -Wall -Wextra -Wframe-larger-than=256 -ggdb3 -I. -I../../../kernel/include/shared/ -I../../../kernel/include/ -o kern kern.c

# -Wconversion
