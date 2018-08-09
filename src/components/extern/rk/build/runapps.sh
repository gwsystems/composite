#/bin/bash
SRCDIR=../../../implementation/rk/default/
COSOBJ=rk_default.o
FINALOBJ=rumpcos.o
QEMURK=qemu_rk.sh
TRANSFERDIR=../../../../../transfer/
RUNSCRIPT=micro_boot.sh

rkapps=( "udpserv"
	 "http"
	 "iperf"
	 "udptrace"
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

combine_final_cos()
{
	for sym in "${localizesymdst[@]}"
	do
		objcopy -L $sym rk_stub/$1
	done

	# Combine RK stub (all drivers with no application) with applicaiton
	ld -melf_i386 -r -o $FINALOBJ rk_stub/$@ $SRCDIR/$COSOBJ
}

combine_symbols_app()
{
	#we're not running apps on rk.. they're stubs.. why combine PROG objects here??
	echo "Combining symbols in the app with rk?? why???"
	PROG=$1
	TMPFILE=tmp.o
	PROGOUT=$2
	PROGDIR=../../../implementation/no_interface/"$PROG"
	cp $PROGDIR/$PROG.o $TMPFILE
	objcopy --weaken  $TMPFILE
	objcopy -L listen  $TMPFILE
	objcopy -L getpid  $TMPFILE
	objcopy -L malloc  $TMPFILE
	objcopy -L calloc  $TMPFILE
	objcopy -L free  $TMPFILE
	objcopy -L mmap  $TMPFILE
	objcopy -L strdup  $TMPFILE
	# Combine COSOBJ and application
	ld -melf_i386 -r -o $PROGOUT $TMPFILE $SRCDIR/$COSOBJ
	rm $TMPFILE

	# Defined in both cos and rk, localize one of them.
	for sym in "${localizesymsrc[@]}"
	do
		objcopy -L $sym $PROGOUT
	done
}

combine_stub_symbols()
{
	PROGOUT=$1

	# Defined in both cos and rk, localize one of them.
	for sym in "${localizesymsrc[@]}"
	do
		objcopy -L $sym $PROGOUT
	done
}

usage()
{
	echo "Usage: $1 <app1>[ <app2> <app3>...]"
	exit
}

# single app function!
combine_single_app()
{
	combine_stub_symbols rk_stub/rk_stub.bin
	combine_final_cos rk_stub.bin
	RUNSCRIPT="$1"_rumpboot.sh
}

combine_multi_app()
{
	for app in "$@"
	do
		RUNARG+=$app
		RUNARG+="_"
	done
	RUNARG+="rumpboot.sh"

	combine_stub_symbols rk_stub/rkmulti.bin
	combine_final_cos rkmulti.bin
	RUNSCRIPT=$RUNARG
} 
if [ $# -lt 1 ]; then
	usage $0
elif [ $# -gt 8 ]; then
	echo "Rumpkernel doesn't support baking >= 8.."
	exit
fi

cd rk_stub; ./rk_nstubs.sh 8 rm; cd ../

if [ $# -eq 1 ]; then
	cd rk_stub; make clean; make rk_stub.bin; cd ../
	combine_single_app $1
else
	NUMAPPS=$#
	APPSTR=$@
	echo "APPS: $APPSTR"
	echo "NUMBER: $NUMAPPS"
	cd rk_stub; ./rk_nstubs.sh $NUMAPPS cp; make clean; make rkmulti.bin; cd ../
	combine_multi_app $APPSTR
fi

echo "Runscript: $RUNSCRIPT"
mv $FINALOBJ $TRANSFERDIR

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

