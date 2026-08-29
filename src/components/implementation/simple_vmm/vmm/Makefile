# Required variables used to drive the compilation process. It is OK
# for many of these to be empty.
#
# The set of interfaces that this component exports for use by other
# components. This is a list of the interface names.
INTERFACE_EXPORTS = vmm_netio_rx vmm_netio_tx vmm_netio_shmem
# The interfaces this component is dependent on for compilation (this
# is a list of directory names in interface/)
INTERFACE_DEPENDENCIES = contigmem netshmem nic_netio_rx nic_netio_tx nic_netio_shmem sched
# The library dependencies this component is reliant on for
# compilation/linking (this is a list of directory names in lib/)
LIBRARY_DEPENDENCIES = ubench component kernel initargs vmrt shm_bm sync time ck fastmem
# Note: Both the interface and library dependencies should be
# *minimal*. That is to say that removing a dependency should cause
# the build to fail. The build system does not validate this
# minimality; that's on you!

CFILES+=devices/serial/serial.c
CFILES+=devices/vpci/vpci.c
CFILES+=devices/vpci/vpci_io.c
CFILES+=devices/vpci/virtio_net_vpci.c
CFILES+=devices/vpci/virtio_net_io.c
CFILES+=devices/vpic/vpic.c
CFILES+=devices/vrtc/vrtc.c
CFILES+=devices/vps2/vps2.c

C_OBJS+=$(CFILES:%.c=%.o)
COMP_FLAGS= -mavx -mavx2 -mfma -I$(LIBDIR)/dpdk/dpdk/build/install/include
# --- guest images -------------------------------------------------------------
# The Linux guest image is built by the vmimg submodule. VM_IMAGE selects a
# recipe (see vmimg/recipes/); the kernel version comes from whichever vmimg
# commit the submodule is pinned to, so it is not overridden here.
VM_IMAGE  ?= shell
VMIMG_DIR := $(CURDIR)/../vmimg
GUEST_DIR := $(CURDIR)/guest

# simple_vmm.c INCBINs both blobs, so they must exist before it is compiled.
# Makefile.subsubdir lists `all: ... $(COMPOBJ) private`, and make builds
# prerequisites left to right, so relying on `private` to populate guest/ would
# compile simple_vmm.c against whatever image a previous build happened to leave
# behind -- or nothing at all in a fresh checkout.
simple_vmm.o: $(GUEST_DIR)/vmlinux.img $(GUEST_DIR)/guest.img

$(GUEST_DIR)/guest.img:
	$(MAKE) -C $(GUEST_DIR)

$(GUEST_DIR)/vmlinux.img:
	@test -f $(VMIMG_DIR)/Makefile || { \
		echo "ERROR: $(VMIMG_DIR) is empty."; \
		echo "       Run: git submodule update --init --recursive"; exit 1; }
	$(MAKE) -C $(VMIMG_DIR) image RECIPE=$(VM_IMAGE)
	$(MAKE) -C $(VMIMG_DIR) install RECIPE=$(VM_IMAGE) DESTDIR=$(GUEST_DIR)

private: $(GUEST_DIR)/guest.img $(GUEST_DIR)/vmlinux.img

include Makefile.subsubdir
