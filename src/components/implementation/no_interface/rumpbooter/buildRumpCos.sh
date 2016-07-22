#!/bin/bash

prog=$1

if [ "$prog" == "" ]; then
	echo Please input an application name;
	exit;
fi

if [ "$prog" == "hello" ]; then
	cp ../../../../../../apps/hello/hello.bin .
fi

if [ "$prog" = "paws" ]; then
	cp ../../../../../../apps/paws/paws.bin .
fi

if [ "$prog" = "nginx" ]; then
	cp ../../../../../../apps/nginx/nginx.bin .
fi

if [ "$prog" = "snake" ]; then
	cp ../../../../../../apps/snake/snake.bin .
fi

# Defined in both cos and rk, localize one of them.
objcopy -L memmove      rump_boot.o
objcopy -L munmap       rump_boot.o
objcopy -L memset       rump_boot.o
objcopy -L memcpy       rump_boot.o
objcopy -L __umoddi3    rump_boot.o
objcopy -L __udivdi3    rump_boot.o
objcopy -L strtol       rump_boot.o
objcopy -L strlen       rump_boot.o
objcopy -L strcmp       rump_boot.o
objcopy -L strncpy      rump_boot.o
objcopy -L strcpy       rump_boot.o
objcopy -L __divdi3     rump_boot.o
objcopy -L puts         rump_boot.o
objcopy -L strtoul      rump_boot.o
objcopy -L strerror     rump_boot.o
objcopy -L getpid       rump_boot.o


objcopy -L abort        $prog.bin
objcopy -L kill         $prog.bin
objcopy -L raise        $prog.bin
objcopy -L sigprocmask  $prog.bin

objcopy -L _exit     $prog.bin
objcopy -L _start    $prog.bin


ld -melf_i386 -r -o rumpcos.o $prog.bin rump_boot.o

cp rumpcos.o ../../../../../transfer/
cp qemu_rk.sh ../../../../../transfer/
