#!/bin/bash

ubuntu64()
{

        # This is used to set evnironment deps in Ubuntu-64 for compile&run x86_32/x86_64 bit Composite
        # Fully Tested Ubuntu version: 20.04
        local dependencies="build-essential git cargo xorriso mtools qemu-system-i386 gcc-multilib python2.7"
        sudo apt install ${dependencies} -y
}

fedora64()
{

        # This is used to set evnironment deps in Fedora/CentOS-64 for compile&run x86_32/x86_64 bit Composite
        # Tested Fedora version: 34/35
        # FIXME: Fedora 34/35 can compile x86_32 and x86_64, but have problem running x86_32 version
        sudo yum groupinstall "Development Tools" -y
        sudo yum install g++ -y
        sudo yum install git -y
        sudo yum install cargo -y
        sudo yum install python2 -y
        sudo yum install xorriso -y
        sudo yum install mtools -y
        sudo yum install qemu -y
        sudo yum install glibc-devel.i686 libstdc++-devel.i686 -y
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
