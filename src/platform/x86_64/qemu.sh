#!/bin/sh

# we need this file to test the kernel img, so not to remove it
mkdir -p iso/boot/grub
cp grub.cfg iso/boot/grub/
cp kernel.img iso/boot/
grub-mkrescue -o kernel.iso iso

# use ctrl+a,x to exit qemu, ctrl+a,c to switch into qemu monitor
qemu-system-x86_64 -m 1024 -cdrom kernel.iso -cpu max -no-reboot -nographic -S -s