#!/bin/bash 
PROG=$1
SRCOBJ=rump_boot.o
DSTOBJ=$PROG.bin
FINALOBJ=rumpcos.o
QEMURK=qemu_rk.sh
RUMPAPPDIR=../../../../../../apps
TRANSFERDIR=../../../../../transfer/

localizesymsrc=( "__fpclassifyl"
		"memset"
		"memcpy"
		"memchr"
		"__umoddi3"
		"__udivdi3"
		"strlen"
		"strcmp"
		"strncpy"
		"strcpy"
		"__divdi3"
		"strerror"
		"sprintf"
		"vsprintf"
		"vfprintf"
		"fwrite"
		"wcrtomb"
		"__fini_array_end"
		"__fini_array_start"
		"__init_array_end"
		"__init_array_start"
		"fputc"
		)
localizesymdst=( "_start"
		"exit"
		"socket"
		"bind"
		"recvfrom"
		"sendto"
		"printf"
		)

if [ "$PROG" == "" ]; then
	echo Please input an application name;
	exit;
fi

if [ ! -d $RUMPAPPDIR ]; then
	echo "Woops! $RUMPAPPDIR doesn't exist"
	exit
fi

cp $RUMPAPPDIR/$PROG/$DSTOBJ .

# Defined in both cos and rk, localize one of them.
for sym in "${localizesymsrc[@]}"
do
	objcopy -L $sym $SRCOBJ
done

for sym in "${localizesymdst[@]}"
do
	objcopy -L $sym $DSTOBJ
done

ld -melf_i386 -r -o $FINALOBJ $DSTOBJ $SRCOBJ

cp $FINALOBJ $TRANSFERDIR
cp $QEMURK $TRANSFERDIR

cd $TRANSFERDIR
USB_DEV=`stat --format "%F" /dev/sdb`

if [ "$USB_DEV" = "block special file" ]; then
	echo "GENERATING ISO"
	./geniso.sh rumpkernboot.sh
	echo "WRITIING ISO IMAGE to /dev/sdb: $USB_DEV"
	sudo dd bs=8M if=composite.iso of=/dev/sdb
	sync
else
	echo "NO /dev/sdb: $USB_DEV"
	echo "RUNNING THE SYSTEM ON QEMU INSTEAD"
	./qemu_rk.sh rumpkernboot.sh
fi

