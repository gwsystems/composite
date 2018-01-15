MAKEFLAGS=-I$(shell pwd)

rk_all: backing
	cd ./rumprun; ./build-rr.sh cos -- -F ACFLAGS=-m32 -r

rk_init: patch rk_all

backing:
	- rm -f ./tools/backing.o
	- rm -f ./tools/data.iso
	- cd ./rumprun/platform/cos; make clean;
	@ python ./tools/buildbacking.py

patch:
	@cp ./tools/netbsd-modifications.patch ./rumprun/src-netbsd
	@cd ./rumprun/src-netbsd; git apply --stat netbsd-modifications.patch
	@cd ./rumprun/src-netbsd; git apply netbsd-modifications.patch; rm netbsd-modifications.patch

rk_clean:
	cd ./rumprun/platform/cos; make clean
