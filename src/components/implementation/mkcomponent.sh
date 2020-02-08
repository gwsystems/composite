#!/bin/sh

COMPNAME=default

if [ $# -eq 0 ] || [ $# -gt 2 ]
then
    echo "Usage: " $0 " <new_ifname> <new_compname>"
    exit
fi

if [ $# -eq 2 ]
then
    COMPNAME=$2
fi

if [ -f "$1" ]
then
    echo "Cannot create component interface $1: already exists as a file."
    exit
fi

if [ ! -d "$1" ]
then
    mkdir $1
    cp skel/Makefile $1/
fi

if [ -f "$1/$COMPNAME" ]
then
    echo "Cannot create component at $1/$COMPNAME: already exists as a file."
    exit
fi

if [ -d "$1/$COMPNAME" ]
then
    echo "Cannot create component at $1/$COMPNAME: already exists."
    exit
fi

mkdir $1/$COMPNAME
cp skel/default/* $1/$COMPNAME/
mv $1/$COMPNAME/default.c $1/$COMPNAME/$COMPNAME.c
sed -i 's/SKEL_COMP/'`echo $COMPNAME`'/g' $1/$COMPNAME/Makefile
sed -i 's/SKEL_IF/'`echo $1`'/g' $1/$COMPNAME/Makefile
