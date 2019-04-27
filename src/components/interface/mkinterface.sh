#!/bin/sh

if [ $# -eq 0 ]
  then
    echo "Usage: " $0 " <new_ifname>"
    exit
fi

# TODO checking that we aren't overwriting an existing interface
cp -r skel $1
mv $1/skel.h $1/$1.h
sed -i 's/SKEL/'`echo $1 | tr '[a-z]' '[A-Z]'`'/g' $1/$1.h
