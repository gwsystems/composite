#!/bin/bash
PROG=$1
SRCDIR=../../../implementation/rk/cnic/
PROGDIR=../../../implementation/netbsd/$PROG
COSOBJ=cnic.o
FINALOBJ=sl_rumpcos.o
QEMURK=qemu_rk.sh
TRANSFERDIR=../../../../../transfer/

cp ./$QEMURK ./$TRANSFERDIR

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
	echo Valid choices include:
	ls ../../../implementation/netbsd | grep -v "Makefile"
	exit;
fi

# Compile RK stub
cd rk_stub
make clean
make all
cd ../

# Combine COSOBJ and application
objcopy --weaken $PROGDIR/$PROG.o
ld -melf_i386 -r -o app.tmp $PROGDIR/$PROG.o $SRCDIR/$COSOBJ

# Defined in both cos and rk, localize one of them.
for sym in "${localizesymsrc[@]}"
do
	objcopy -L $sym app.tmp
done

for sym in "${localizesymdst[@]}"
do
	objcopy -L $sym rk_stub/rk_stub.bin
done

# Combine RK stub (all drivers with no application) with applicaiton
ld -melf_i386 -r -o $FINALOBJ app.tmp rk_stub/rk_stub.bin
rm app.tmp

cp $FINALOBJ $TRANSFERDIR

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
	./$QEMURK rumpkernboot.sh
fi

