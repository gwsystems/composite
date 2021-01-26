# #!/bin/sh
# if [ $# != 1 ]; then
#   echo "Usage: $0 <run-script.sh>"
#   exit 1
# fi

# if ! [ -r $1 ]; then
#   echo "Can't open run-script"
#   exit 1
# fi

# MODULES=$(sh $1 | awk '/^Writing image/ { print $3; }' | tr '\n' ' ')

# qemu-system-i386 -m 768 -nographic -kernel kernel.img -no-reboot -s -initrd "$(echo $MODULES | tr ' ' ',')"

mkdir -p iso/boot/grub
cp grub.cfg iso/boot/grub/
cp kernel.img iso/boot/
grub-mkrescue -o kernel.iso iso

# use ctrl+a,x to exit qemu
qemu-system-x86_64 -m 1024 -cdrom kernel.iso -cpu max -no-reboot -nographic -S -s