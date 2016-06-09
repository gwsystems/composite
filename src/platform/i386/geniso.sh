#!/bin/sh
if [ $# != 1 ]; then
  echo "Usage: $0 <run-script.sh>"
  exit 1
fi

if ! [ -r $1 ]; then
  echo "Can't open run-script"
  exit 1
fi

COS_KERNEL=kernel.img
COS_MODULES=$(sh $1 | awk '/^Writing image/ { print $3; }' | tr '\n' ' ')
COS_ISO=composite.iso
ISO_DIR=bootable/
BOOT_DIR=$ISO_DIR/boot/
GRUB_DIR=$BOOT_DIR/grub/
GRUB_CFG=$GRUB_DIR/grub.cfg

if ! [ -f $COS_KERNEL ]; then
  echo "Can't find kernel image"
  exit 1
fi

if [ -d $ISO_DIR ]; then
  rm -rf $ISO_DIR
fi

mkdir -p $GRUB_DIR
cp $COS_KERNEL $BOOT_DIR

printf 'set timeout=0\n\n' >> $GRUB_CFG
printf 'menuentry composite {\n' >>$GRUB_CFG
printf '\tmultiboot /boot/%s\n' $COS_KERNEL >>$GRUB_CFG
for MODULE in "$COS_MODULES"
do 
  cp $MODULE $BOOT_DIR
  printf '\tmodule /boot/%s /boot/%s\n' $MODULE $MODULE >>$GRUB_CFG
done
printf '\tboot\n' >>$GRUB_CFG
printf '}\n' >>$GRUB_CFG

grub-mkrescue -o $COS_ISO $ISO_DIR
rm -rf $ISO_DIR

