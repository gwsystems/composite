#!/bin/sh

TMPDIR=tmp/

LOOPDEVNAME=/dev/loop0p1
UDISK=uboot.disk
UDISKPATH=$TMPDIR/$UDISK
KERNELDIR=../src/platform/cav7/
KERNEL=kernel.elf
KERNELBIN=kernel.bin
LOCALDIRNAME=$TMPDIR/p1
UBOOTELF=u-boot.elf
UBOOTDIR=u-boot-xlnx/

QEMU=qemu-system-arm
MACHINE=xilinx-zynq-a9

cleanuploop() {
	sudo umount $LOCALDIRNAME
	LOOPDEVCLEAN=`losetup -l | grep uboot.disk | cut -f1 -d' '`
	sudo losetup -d $LOOPDEVCLEAN
}

cleanup() {
	cleanuploop
	sudo rm -rf $TMPDIR
}

setup() {
	LOOPDEV=`losetup -f`
	mkdir -p $TMPDIR
	dd if=/dev/zero of=$UDISKPATH bs=1M count=256
	sfdisk $UDISKPATH < ./uboot.sfdisk

	sudo losetup $LOOPDEV $UDISKPATH
	sudo partprobe $LOOPDEV
	sudo mkfs.ext2 $LOOPDEVNAME

	mkdir -p $LOCALDIRNAME
	sudo mount -t ext2 $LOOPDEVNAME $LOCALDIRNAME
	sudo cp $LOCALDIRNAME/$KERNELBIN
	sudo cp $UBOOTDIR/$UBOOTELF $TMPDIR
	ls -l $LOCALDIRNAME
	cleanuploop
}



qemu() {
	cd $TMPDIR ; $QEMU -nographic -M $MACHINE -m 1024 -serial /dev/null -serial mon:stdio -display none -kernel $UBOOTELF -sd $UDISK ; cd -
}

all() {
	cleanup
	setup
	qemu
}

usage() {
	echo "$0 <cleanup | setup | qemu | all | usage>"
	exit 1
}

if [ $# != 1 ]; then
  usage $0
fi

$1
