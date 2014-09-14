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

sed -e "s/^\.\/cos_loader/\.\/cos_loader -q/" $1 > $TMPSCRIPT
MODULES=$(sh $TMPSCRIPT | awk '/^creating module/ { print $3; }' | tr '\n' ' ')
rm $TMPSCRIPT

qemu-system-i386 -m 128 -nographic -kernel kernel.img -no-reboot -s -initrd "$(echo $MODULES | tr ' ' ',')"

#rm -f $MODULES
