#!/bin/bash

if [ $# -lt 2 ]; then
  echo "Usage: $0 <.../cos.iso > <arch: [x86_64|i386]> [debug]"
  exit 1
fi 

num_sockets=1
num_cores=1
num_threads=1
vcpus=$[${num_sockets}*${num_cores}*${num_threads}]
mem_size=1024
kvm_flag=""

arch=$2
debug_flag=$3

if [ ! -f $1 ]
then
	echo "Cannot find the image iso"
	exit 1
fi

case ${arch} in
	x86_64 )
	;;
	i386 )
	;;
	* )
	echo "Unsupported architecture"
	exit 1
esac

if [ -e "/dev/kvm" ] && [ -r "/dev/kvm" ] && [ -w "/dev/kvm" ]
then
	kvm_flag="-enable-kvm"
fi

if [ "${debug_flag}" == "debug" ]
then
	debug_flag="-S"
fi

if [ "${arch}" == "x86_64" ]
then
	qemu-system-x86_64 ${kvm_flag} -cpu max -smp ${vcpus},cores=${num_cores},threads=${num_threads},sockets=${num_sockets} -m ${mem_size} -cdrom $1 -no-reboot -nographic -s ${debug_flag}
elif [ "${arch}" == "i386" ]
then
	qemu-system-i386 ${kvm_flag} -cpu max -smp ${vcpus},cores=${num_cores},threads=${num_threads},sockets=${num_sockets} -m ${mem_size} -cdrom $1 -no-reboot -nographic -s ${debug_flag}
else
	echo "Unsupported arch!"
fi
