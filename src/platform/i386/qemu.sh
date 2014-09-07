#!/bin/sh
if [ $# != 1 ]; then
  echo "Usage: $0 <run-script.sh>"
  exit 1
fi

if ! [ -r $1 ]; then
  echo "Can't open run-script"
  exit 1
fi
  
TMPSCRIPT=/tmp/cos-rs-$$.sh
TMPIMAGE=cos-image-$$.img


sed -e "s/^\.\/cos_loader/\.\/cos_loader -o $TMPIMAGE/" $1 > $TMPSCRIPT
sh $TMPSCRIPT
rm $TMPSCRIPT

qemu-system-i386 -m 128 -nographic -kernel kernel.img -no-reboot -s -initrd $TMPIMAGE

rm $TMPIMAGE

