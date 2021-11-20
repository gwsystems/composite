#!/bin/sh

if [ $# != 1 ]; then
  echo "Usage: $0 <.../cos.img>"
  exit 1
fi

if ! [ -r $1 ]; then
  echo "Can't access system image " $1
  exit 1
fi

build_iso()
{
	echo "[cos generating ISO image]"
	local dir=$(cd "$(dirname "$1")"; pwd)  
	local bin_name=$(basename $1 .img)

	set +e
	grub=$(command -v grub-mkrescue)
	grub2=$(command -v grub2-mkrescue)
	set -e

	local grub_command=""
	if [ "${grub}" != "" ]
	then
		grub_command="grub-mkrescue"
	elif [ "${grub2}" != "" ]
	then
		grub_command="grub2-mkrescue"
	else
		echo "Cannot find grub-mkrescue/grub2-mkrescue to generate ISO image."
		exit 1
	fi

	cd ${dir}
	echo "set timeout=0" > grub.cfg
	echo "set default=0" >> grub.cfg
	echo "menuentry "composite" {" >> grub.cfg
	echo "  multiboot2 /boot/${bin_name}.img" >> grub.cfg
	echo "}" >> grub.cfg

	mkdir -p iso/boot/grub
	cp grub.cfg iso/boot/grub/
	cp ${bin_name}.img iso/boot/
	${grub_command} -d /usr/lib/grub/i386-pc -o ${bin_name}.iso iso

	rm -rf iso grub.cfg

	echo "Successfully generated ISO image: ${dir}/${bin_name}.iso"
}

build_iso $1