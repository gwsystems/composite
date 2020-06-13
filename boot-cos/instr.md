# adapted from https://github.com/LeMaker/u-boot/blob/master/doc/README.clang

clone u-boot-xlinx from github
export TRIPLET=armv7a-none-elf && export CROSS_COMPILE="$TRIPLET-"
make configurations with:
	- make distclean
	- make HOSTCC=clang CC="clang -target $TRIPLET -mllvm -arm-use-movt=0 -no-integrated-as" zynq_zc702_defconfig
make u-boot with
	- make HOSTCC=clang CC="clang -target $TRIPLET" all V=1 -j8

