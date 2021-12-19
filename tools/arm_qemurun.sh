#!/bin/sh

TMPDIR=$2/tmp/

UDISK=uboot.disk
USFDISKPATH=$TMPDIR/uboot.sfdisk
UDISKPATH=$TMPDIR/$UDISK
KERNELDIR=$2/
KERNELBIN=cos.img.bin
LOCALDIRNAME=$TMPDIR/p1
UBOOTELF=u-boot.elf
UBOOTDIR=$2/../../../cos_u-boot-xlnx/

QEMU=qemu-system-arm
MACHINE=xilinx-zynq-a9

cleanuploop() {
	sudo umount $LOCALDIRNAME
	LOOPDEVNAME=`losetup -l | grep uboot.disk | cut -f1 -d' '`
	if [ "$LOOPDEVNAME" != "" ]; then
		echo $LOOPDEVNAME 
		sudo losetup -d $LOOPDEVNAME
	fi
}

cleanup() {
	cleanuploop
	sudo rm -rf $TMPDIR
}

create_sfdisk() {
cat > $USFDISKPATH <<EOF
label: dos
label-id: 0xedb0411a
device: tmp/uboot.disk
unit: sectors

tmp/uboot.disk1 : start=        2048, size=       40960, type=83

EOF
}

setup() {
	mkdir -p $TMPDIR
	create_sfdisk
	LOOPDEV=`losetup -f`
	echo "Using $LOOPDEV here"
	dd if=/dev/zero of=$UDISKPATH bs=1M count=256
	sfdisk $UDISKPATH < $USFDISKPATH

	sudo losetup $LOOPDEV $UDISKPATH
	LOOPDEVNAME=`losetup -l | grep uboot.disk | cut -f1 -d' '`
	LOOPPART=$LOOPDEVNAME"p1"
	sudo partprobe $LOOPDEV
	sudo mkfs.ext2 -O ^64bit $LOOPPART

	mkdir -p $LOCALDIRNAME
	sudo mount -t ext2 $LOOPPART $LOCALDIRNAME
	sudo cp $KERNELDIR/$KERNELBIN $LOCALDIRNAME/$KERNELBIN
	sudo cp $UBOOTDIR/$UBOOTELF $TMPDIR
	ls -l $LOCALDIRNAME
	cleanuploop
}



qemu() {
	cd $TMPDIR ; $QEMU -s -nographic -M $MACHINE -m 1024 -serial /dev/null -serial mon:stdio -display none -kernel $UBOOTELF -sd $UDISK ; cd -
}

all() {
	cleanup
	setup
	qemu
}

usage() {
	echo "$0 <cleanup | setup | qemu | all | usage> <system_binaries composed build>"
	exit 1
}

if [ $# != 2 ]; then
  usage $0
fi

$1
