TRANS_DIR=/home/grad3/gabep1/transfer/
DEST_DIR=/scratch/gabep1/

reset: clean cp build

run:
	./cos_loader "c0.o,fprr.o,net.o,d.o,print_comp.o,uc.o:c0.o-fprr.o;fprr.o-print_comp.o;net.o-fprr.o;net.o-print_comp.o;net.o-d.o;d.o-fprr.o;d.o-print_comp.o;uc.o-print_comp.o;uc.o-d.o" ./gen_client_stub

init: ready_hw first

ready_hw:
	ethtool -A eth0 autoneg off rx off tx off
	mknod /dev/net/cnet c 10 201
	echo 1 > /proc/sys/net/ipv4/ip_forward

first: cp build

cp:
	cp $(TRANS_DIR)/* $(DEST_DIR)

clean:
	rm cos_loader
	rmmod cnet
	rmmod cos

build: 
	insmod cos.ko
	insmod cnet.ko
	./cnet_user
