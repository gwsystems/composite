#!/bin/sh

LIBNAME=default

if [ $# -eq 0 ] || [ $# -gt 1 ]
then
    echo "Usage: " $0 " <new_lib_name>"
    exit
fi

LIBNAME=$1

if [ -f "$LIBNAME" ]
then
    echo "Cannot create library at `pwd`/$LIBNAME: already exists as a file."
    exit
fi

if [ -d "$LIBNAME" ]
then
    echo "Cannot create library at `pwd`/$LIBNAME: already exists."
    exit
fi

mkdir $LIBNAME
cp skel/* $LIBNAME/
sed -i 's/SKEL/'`echo $LIBNAME`'/g' $LIBNAME/Makefile
