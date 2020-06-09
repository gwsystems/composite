#!/bin/sh

# sudo qemu-system-i386 -smp 4 -enable-kvm -m 768 -nographic -kernel ../platform/i386/kernel.img  -no-reboot -s -initrd $1
qemu-system-i386 -m 768 -nographic -smp 1 -enable-kvm -kernel $1 -no-reboot -s
