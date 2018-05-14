#!/bin/bash
PROG=$1
SRCDIR=../../../implementation/rk/cnic/
COSOBJ=cnic.o
FINALOBJ=rumpcos.o
QEMURK=qemu_rk.sh
TRANSFERDIR=../../../../../transfer/
RUNSCRIPT="$PROG"_rumpboot.sh

rkapps=( "udpserv"
	 "http"
	 "iperf"
	 "cfe_rk_http"
	)

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
		"puts"
		)
localizesymdst=( "_start"
		"exit"
		"socket"
		"bind"
		"recvfrom"
		"sendto"
		"printf"
		"strcspn"
		"strspn"
		"_GLOBAL_OFFSET_TABLE_"
		)

if [ "$PROG" == "" ]; then
	echo Please input an application name;
	echo Valid choices include:
	for a in "${rkapps[@]}"
	do
	echo $a
	done
#	pushd ../../../implementation/no_interface > /dev/null
#	echo
#	ls -1d */
#	echo "cfe_rk_http/"
#	echo
#	popd > /dev/null
#	echo Do no include \"/\" in your selection
	exit;
fi

if [ "$PROG" == "cfe_rk_http" ]; then
	PROG=http
	RUNSCRIPT=cfe_rk_http_rumpboot.sh
elif [ "$PROG" == "cfe_rk_http_smp" ]; then
	PROG=http
	RUNSCRIPT=cfe_rk_http_smp_rumpboot.sh
fi

PROGDIR=../../../implementation/no_interface/"$PROG"

# Compile RK stub
cd rk_stub
make clean
make all
cd ../

# Combine COSOBJ and application
cp $PROGDIR/$PROG.o ./tmp.o
objcopy --weaken ./tmp.o
objcopy -L listen ./tmp.o
objcopy -L getpid ./tmp.o
objcopy -L malloc ./tmp.o
objcopy -L calloc ./tmp.o
objcopy -L free ./tmp.o
objcopy -L mmap ./tmp.o
objcopy -L strdup ./tmp.o
ld -melf_i386 -r -o app.tmp tmp.o $SRCDIR/$COSOBJ

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
	echo "$RUNSCRIPT"
	./geniso.sh "$RUNSCRIPT"
	echo "WRITIING ISO IMAGE to /dev/sdb: $USB_DEV"
	sudo dd bs=8M if=composite.iso of=/dev/sdb
	sync
else
	echo "NO /dev/sdb: $USB_DEV"
	echo "RUNNING THE SYSTEM ON QEMU INSTEAD"
	echo "$RUNSCRIPT"
	./$QEMURK "$RUNSCRIPT"
fi
