#!/bin/bash

rm -rf /home/user/projects/composite/system_binaries/cos_build-pingpong

set -e
./cos init
#./cos reset
./cos build
./cos compose composition_scripts/ping_pong.toml pingpong
#make -C src COMP_INTERFACES="init/stubs" COMP_IFDEPS="init/kernel" COMP_LIBDEPS="" COMP_INTERFACE=tests COMP_NAME=kernel_tests COMP_VARNAME=global.tests COMP_OUTPUT=/home/user/projects/composite/system_binaries/cos_build-test/global.tests/tests.kernel_tests.global.tests COMP_BASEADDR=0x00400000 COMP_INITARGS_FILE=/home/user/projects/composite/system_binaries/cos_build-test/global.tests/initargs_constructor.c  component
#cp system_binaries/cos_build-test/global.tests/tests.kernel_tests.global.tests system_binaries/cos_build-test/constructor
#make -C src KERNEL_OUTPUT="/home/user/projects/composite/system_binaries/cos_build-test/cos.img" CONSTRUCTOR_COMP="/home/user/projects/composite/system_binaries/cos_build-test/constructor" plat
