#!/bin/sh

if [ $# -ne 1 ]
  then
    echo "Usage: " $0 " <new_ifname>"
    exit
fi

if [ -f "$1" ] || [ -d "$1" ]
  then
    echo "Cannot create interface $1: already exists."
    exit
fi

cp -r skel $1
mv $1/skel.h $1/$1.h
sed -i 's/SKEL/'`echo $1 | tr '[a-z]' '[A-Z]'`'/g' $1/$1.h
