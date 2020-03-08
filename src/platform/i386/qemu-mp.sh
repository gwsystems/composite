#!/bin/sh
if [ $# != 1 ]; then
  echo "Usage: $0 <run-script.sh>"
  exit 1
fi

if ! [ -r $1 ]; then
  echo "Can't open run-script"
  exit 1
fi

if [ -e "/dev/kvm" ] && [ -r "/dev/kvm" ] && [ -w "/dev/kvm" ]; then
  KVM_FLAG="-enable-kvm"
else
  KVM_FLAG=""
fi

MODULES=$(sh $1 | awk '/^Writing image/ { print $3; }' | tr '\n' ' ')

qemu-system-i386 $KVM_FLAG -m 768 -smp 4 -nographic -kernel kernel.img -no-reboot -s -initrd "$(echo $MODULES | tr ' ' ',')"
