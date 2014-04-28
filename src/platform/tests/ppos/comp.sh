#!/bin/sh

gcc -DUNITTEST -funit-at-a-time -Wall -Wextra -Wframe-larger-than=256 -Wno-unused-function -ggdb3 -I. -I../../../kernel/include/shared/ -I../../../kernel/include/ -o kern kern.c captbl.c pgtbl.c liveness_tbl.c

# -Wconversion
