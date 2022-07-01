#!/bin/bash

# grub-mkrescue/grub2-mkrescue is used to make an ISO image in build_iso.sh, thus install it here

set +e
grub=$(command -v grub-mkrescue)
grub2=$(command -v grub2-mkrescue)
set -e

ubuntu64()
{
	# This is used to set evnironment deps in Ubuntu-64 for compile&run x86_32/x86_64 bit Composite
	# Tested Ubuntu version: 18.04/20.04/21.04
	local dependencies="build-essential git cargo xorriso mtools qemu-system-i386 gcc-multilib python2.7 python3-pip"
	sudo apt install ${dependencies} -y

	if [ "${grub}" == "" ] && [ "${grub2}" == "" ]
	then
		sudo apt install grub2-common
	fi

	pip3 install pyelftools
	pip3 install meson>=0.61.0
	pip3 install ninja

	# Install rust
	curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
}

fedora64()
{
	# This is used to set evnironment deps in Fedora/CentOS-64 for compile&run x86_32/x86_64 bit Composite
	# Tested Fedora version: 34/35
	local dependencies="g++ git cargo python2.7 xorriso mtools qemu glibc-devel.i686 libstdc++-devel.i686 python3-pip"
	sudo yum groupinstall "Development Tools" -y
	sudo yum install ${dependencies} -y

	if [ "${grub}" == "" ] && [ "${grub2}" == "" ]
	then
		sudo yum install grub2
	fi

	pip3 install pyelftools
	pip3 install meson>=0.61.0
	pip3 install ninja
	# Install rust
	curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
}

usage()
{
	echo "Usage: " $0 " ubuntu-64|fedora-64"
	exit 1
}

case $1 in
	ubuntu-64 )
		ubuntu64
		;;
	fedora-64 )
		fedora64
		;;
	centos-64 )
		fedora64
		;;
	* )
		usage
esac
