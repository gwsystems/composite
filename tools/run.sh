#!/bin/sh

if [ $# != 1 ]; then
  echo "Usage: $0 <.../cos.img>"
  exit 1
fi

if ! [ -r $1 ]; then
  echo "Can't access system image " $1
  exit 1
fi

if [ -e "/dev/kvm" ] && [ -r "/dev/kvm" ] && [ -w "/dev/kvm" ]; then
  KVM_FLAG="-enable-kvm"
else
  KVM_FLAG=""
fi

# TODO: inherit number of cores from the build

qemu-system-i386 $KVM_FLAG -m 768 -nographic -smp 1 -kernel $1 -no-reboot -s
