#!/bin/sh
make clean
cd ..
cd transfer
rm -rf *
cd ..
cd src
make config
make init
make ; make cp

# Other problems:
# 1. The kernel is using types that are provided by the C compiler. This is not good for portability.
# The movements:
# 1. move the page table stuff to there, and use general flags instead
