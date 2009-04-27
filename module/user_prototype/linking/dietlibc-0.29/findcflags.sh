#!/bin/sh

## gap: I want frame pointers!  Change -fomit-frame-pointer to -fno-""

version=`${1:-gcc} -v 2>&1 |sed -n '/gcc version/ {s/gcc version //;p;}'`
case $version in
  2.9*) echo -march=i386 -Os -fno-omit-frame-pointer -malign-functions=1 -malign-jumps=1 -malign-loops=1 -mpreferred-stack-boundary=2 ;;
  3.0*) echo -march=i386 -Os -fno-omit-frame-pointer -malign-functions=1 -malign-jumps=1 -malign-loops=1 -mpreferred-stack-boundary=2 ;;
  [34]*) echo -Os -fno-omit-frame-pointer -falign-functions=1 -falign-jumps=1 -falign-loops=1 -mpreferred-stack-boundary=2;;
  *) echo -O2 -pipe -fno-omit-frame-pointer ;;
esac
