#!/bin/bash

ubuntu64()
{
        # This is used to set evnironment deps in Ubuntu-64 for compile&run x86_32/x86_64 bit Composite
        # Tested Ubuntu version: 18.04/20.04/21.04
        local dependencies="build-essential git cargo xorriso mtools qemu-system-i386 gcc-multilib python2.7"
        sudo apt install ${dependencies} -y
}

fedora64()
{
        # This is used to set evnironment deps in Fedora/CentOS-64 for compile&run x86_32/x86_64 bit Composite
        # Tested Fedora version: 34/35
        local dependencies="g++ git cargo python2.7 xorriso mtools qemu glibc-devel.i686 libstdc++-devel.i686"
        sudo yum groupinstall "Development Tools" -y
        sudo yum install ${dependencies} -y
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
