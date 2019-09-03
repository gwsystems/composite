#!/bin/sh
if [ $# != 1 ]; then
  echo "Usage: $0 <run-script.sh>"
  exit 1
fi

if ! [ -r $1 ]; then
  echo "Can't open run-script"
  exit 1
fi

MODULES=$(sh $1 | awk '/^Writing image/ { print $3; }' | tr '\n' ' ')

#qemu-system-i386 -m 768 -nographic -kernel kernel.img -no-reboot -s -initrd "$(echo $MODULES | tr ' ' ',')"
qemu-system-i386 -enable-kvm -rtc base=localtime,clock=host,driftfix=none -smp sockets=1,cores=6,threads=1 -cpu host -nographic -m 2048 -kernel kernel.img -initrd "$(echo $MODULES | tr ' ' ',')"
