DPDKDIR = dpdk
DPDKBUILD = dpdk/build

# Required variables used to drive the compilation process. It is OK
# for many of these to be empty.
#
# The library names associated with .a files output that are linked
# (via, for example, -ldpdk) into dependents. This list should be
# "dpdk" for output files such as libdpdk.a.
LIBRARY_OUTPUT = dpdk
#LIBRARY_OUTPUT = $(patsubst lib%.a,%,$(wildcard *.a))
# The .o files that are mandatorily linked into dependents. This is
# rarely used, and only when normal .a linking rules will avoid
# linking some necessary objects. This list is of names (for example,
# dpdk) which will generate dpdk.lib.o. Do NOT include the list of .o
# files here. Please note that using this list is *very rare* and
# should only be used when the .a support above is not appropriate.
OBJECT_OUTPUT =
# The path within this directory that holds the .h files for
# dependents to compile with (./ by default). Will be fed into the -I
# compiler arguments.
INCLUDE_PATHS = . dpdk/build/install/include
# The interfaces this component is dependent on for compilation (this
# is a list of directory names in interface/)
INTERFACE_DEPENDENCIES =
# The library dependencies this component is reliant on for
# compilation/linking (this is a list of directory names in lib/)
LIBRARY_DEPENDENCIES = component pci
# Note: Both the interface and library dependencies should be
# *minimal*. That is to say that removing a dependency should cause
# the build to fail. The build system does not validate this
# minimality; that's on you!

# There are two different *types* of Makefiles for libraries.
# 1. Those that are Composite-specific, and simply need an easy way to
#    compile and itegrate their code.
# 2. Those that aim to integrate external libraries into
#    Composite. These focus on "driving" the build process of the
#    external library, then pulling out the resulting files and
#    directories. These need to be flexible as all libraries are
#    different.

# Type 1, Composite library: This is the default Makefile for
# libraries written for composite. Get rid of this if you require a
# custom Makefile (e.g. if you use an existing
# (non-composite-specific) library. An example of this is `kernel`.
include Makefile.lib

## Type 2, external library: If you need to specialize the Makefile
## for an external library, you can add the external code as a
## subdirectory, and drive its compilation, and integration with the
## system using a specialized Makefile. The Makefile must generate
## lib$(LIBRARY_OUTPUT).a and $(OBJECT_OUTPUT).lib.o, and have all of
## the necessary include paths in $(INCLUDE_PATHS).
##
## To access the Composite Makefile definitions, use the following. An
## example of a Makefile written in this way is in `ps/`.
#
# include Makefile.src Makefile.comp Makefile.dependencies
# .PHONY: all clean init distclean
## Fill these out with your implementation
# all:
# clean:
#
## Default rules:
# init: clean all
# distclean: clean

.PHONY: all config install clean distclean init

DPDK_INC = -I$(shell pwd)

SHLIB_FILE=$(if $(LIBRARY_OUTPUT), lib$(LIBRARY_OUTPUT).a)

%.o:%.c
	$(info |     [CC]   Compiling C file $^ into $@)
	$(CC) $(CFLAGS) $(CINC) $(DEPENDENCY_INCPATH) -Idpdk/build/install/include -I/usr/lib/gcc/x86_64-linux-gnu/9/include/ -c -o $@ $^

config:
	$(info Configing DPDK)
#	TODO: make this copy more generic so that it can be applied to different OS platforms like Fedora
	cd ../libc/musl-1.2.0/include; \
	cp -rf /usr/include/linux/ ./; \
	cp -rf /usr/include/asm-generic/ ./; \
	cp -rf /usr/include/x86_64-linux-gnu/asm/ ./; \
	cp -rf /usr/include/x86_64-linux-gnu/sys/queue.h ./sys
	cd $(DPDKDIR);PATH=$(shell pwd)/../libc/musl-1.2.0/bin/:$(PATH) meson build --cross-file $(shell pwd)/cross_x86_64_composite_with_musl \
	-Dmachine=default -Dc_args="-nostdlib -nostdinc -mcmodel=large -DDPDK_COMPOSITE $(DPDK_INC)" -Ddefault_library=static -Dbuildtype=debug \
	-Ddebug=true -Doptimization=0 -Dincludedir=$(shell pwd)/$(DPDKBUILD)/install/include \
	-Dprefix=$(shell pwd)/$(DPDKBUILD)/install \
	-Ddisable_drivers=common/mvep,common/mlx5,af_xdp,net/ipn3ke,net/mlx4,net/mlx5,net/mvneta,net/mvpp2,net/nfb,net/szedata2,raw/ifpga,crypto/aesni_gcm,crypto/aesni_mb,crypto/ccp,crypto/kasumi,crypto/mvsam,crypto/openssl,crypto/snow3g,crypto/zuc,compress/isal,crypto/bcmfs,crypto/caam_jr,crypto/cnxk,crypto/dpaa_sec,crypto/dpaa2_sec,crypto/nitrox,crypto/null,crypto/octeontx,crypto/octeontx2,crypto/scheduler,crypto/virtio,compress/octeontx,compress/zlib,regex/octeontx2,vdpa/ifc,event/cnxk,event/dlb2,event/dsw,event/opdl,event/skeleton,event/sw,baseband/acc100,baseband/fpga_5gnr_fec,baseband/fpga_lte_fec,baseband/null,baseband/turbo_sw,raw/cnxk_bphy,raw/dpaa2_cmdif,raw/dpaa2_qdma,raw/ioat,raw/ntb,raw/octeontx2_dma,raw/octeontx2_ep,raw/skeleton,common/cpt,common/dpaax,common/iavf,common/octeontx,common/octeontx2,common/cnxk,common/qat,common/sfc_efx,bus/auxiliary,bus/dpaa,bus/fslmc,bus/ifpga,net/af_packet,net/ark,net/atlantic,net/avp,net/axgbe,net/bnx2x,net/bnxt,net/bond,net/cxgbe,net/ena,net/enic,net/failsafe,net/fm10k,net/hinic,net/hns3,net/i40e,net/igc,net/ionic,net/ixgbe,net/kni,net/liquidio,net/memif,net/nfp,net/ngbe,net/null,net/pcap,net/qede,net/softnic,net/thunderx,net/txgbe,net/vdev_netvsc,baseband/la12xx,dma/idxd,dma/ioat,dma/skeleton,net/octeontx_ep,

build:
	$(info Building DPDK)
	cd $(DPDKBUILD);PATH=$(shell pwd)/../libc/musl-1.2.0/bin/:$(PATH) ninja -j2

install:
	cd $(DPDKBUILD); meson install;
	rm libdpdk.a || true

$(SHLIB_FILE): build install $(OBJS)
	$(info |     [AR]   Creating library file $@ from $^)
	$(AR) crT libdpdk.a $(DPDKBUILD)/install/lib/*.a $(OBJS)

init: config build

distclean:
	cd $(DPDKBUILD) ninja clean
	cd $(DPDKDIR); rm -rf build
all: build
clean:
