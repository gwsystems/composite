#!/bin/sh
COS_ISO=composite.iso

qemu-system-i386 -nographic -cdrom $COS_ISO
