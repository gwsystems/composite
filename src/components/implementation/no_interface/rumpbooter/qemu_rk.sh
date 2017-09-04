#!/bin/sh
if [ $# != 1 ]; then
  echo "Usage: $0 <run-script.sh>"
  exit 1
fi

if ! [ -r $1 ]; then
  echo "Can't open run-script"
  exit 1
fi

MODULES=$(sh $1 | awk '/^Writing image/ { print $3; }' | tr '\n' ' ')

#qemu-system-i386 -no-kvm -net nic,model=virtio -net tap,script=no,ifname=tap0 -m 128 -nographic -net nic -net user -kernel kernel.img -no-reboot -s -initrd "$(echo $MODULES | tr ' ' ',')"
#qemu-system-i386 -no-kvm -net nic,model=virtio -net tap,script=no,ifname=tap0 -m 128 -nographic -kernel kernel.img -no-reboot -s -initrd "$(echo $MODULES | tr ' ' ',')"
#qemu-system-i386 -no-kvm -net nic,model=virtio -net tap,script=no,ifname=tap0 -nographic -m 128 -kernel kernel.img -initrd "$(echo $MODULES | tr ' ' ',')"
#qemu-system-i386 -no-kvm -net nic,model=e1000 -nographic -m 768 -kernel kernel.img -initrd "$(echo $MODULES | tr ' ' ',')"

clear

#sudo ip tuntap add tap0 mode tap
#sudo ip addr add 10.0.120.100/24 dev tap0
#sudo ip link set dev tap0 up
#sudo route add -net 111.111.111.0 netmask 255.255.255.0 gw 10.0.120.101 #CNIC0 located: DOM0
qemu-system-i386 -no-kvm -net nic,model=virtio -net tap,script=no,ifname=tap0 -nographic -m 768 -kernel kernel.img -initrd "$(echo $MODULES | tr ' ' ',')"
