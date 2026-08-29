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
# The Linux guest image is built by the vmimg submodule.
#
# VM_IMAGE is the recipe ./cos build produces, so a default guest exists in a
# freshly built tree. Which image is actually embedded is chosen per-composition
# -- see VM_GUEST_IMAGE below.
VM_IMAGE  ?= shell
VMIMG_DIR := $(CURDIR)/../vmimg
GUEST_DIR := $(CURDIR)/guest

# Rule targets use the relative path, because that is how the composition script
# spells the image ("guest/vmlinux-shell-5.15.107.img") and make matches targets
# by name: an absolute prerequisite would never match a relative rule.
GUEST := guest

# Which guest image to embed comes from the composition script, as a compile-time
# constant. The composer writes it into a header and passes that header here as
# COMP_CONST_H="-include <path>", so read the path back out of it: parsing the
# same header the compiler sees is what stops the make dependency and the INCBIN
# from ever naming different files.
# COMP_CONST_H is two words -- "-include" and the path -- so drop the flag and
# keep the filename. (patsubst is per-word and matches neither.)
VM_CONST_H     := $(filter-out -include,$(COMP_CONST_H))
VM_GUEST_IMAGE := $(if $(VM_CONST_H),$(shell sed -n \
                    's/^#define VM_GUEST_IMAGE "\(.*\)"/\1/p' $(VM_CONST_H) 2>/dev/null))

# If the composer gave us a header but we could not find the define in it, stop.
# Carrying on would drop simple_vmm.o's prerequisite and silently embed whatever
# image happened to be lying in guest/.
ifneq ($(VM_CONST_H),)
ifeq ($(VM_GUEST_IMAGE),)
$(error Could not read VM_GUEST_IMAGE from $(VM_CONST_H). Add it to your \
composition script: constants = [{variable = "VM_GUEST_IMAGE", value = "\"guest/vmlinux-<recipe>-<kver>.img\""}])
endif
endif

# `./cos build` compiles every component with no composition in sight, so there
# is no constant then and no way to know which guest is wanted. Embed a
# placeholder rather than building a kernel: `./cos compose` runs
# `component: clean comp_header $(COMPOBJ)`, which discards this object and
# recompiles against the image the composition names, so nothing built here is
# ever used to boot.
#
# The alternative -- building a default guest at `./cos build` -- puts a 120MB
# download and a full kernel compile in the path of every Composite build,
# including for people who never compose a VM.
ifeq ($(VM_GUEST_IMAGE),)
VM_GUEST_IMAGE := $(GUEST)/.placeholder.img
COMP_FLAGS     += -DVM_GUEST_IMAGE='"$(VM_GUEST_IMAGE)"'

$(GUEST)/.placeholder.img:
	@mkdir -p $(GUEST)
	@printf 'not a guest image: build one with a composition that names it\n' > $@
	@echo "  [VMIMG] no composition yet -- embedding a placeholder;"
	@echo "          ./cos compose will embed the image its script names"
endif

# simple_vmm.c INCBINs both blobs, so they must exist before it is compiled.
# Makefile.subsubdir lists `all: ... $(COMPOBJ) private`, and make builds
# prerequisites left to right, so relying on `private` to populate guest/ would
# compile simple_vmm.c against whatever image a previous build happened to leave
# behind -- or nothing at all in a fresh checkout.
simple_vmm.o: $(VM_GUEST_IMAGE) $(GUEST)/guest.img

$(GUEST)/guest.img:
	$(MAKE) -C $(GUEST_DIR)

# Build whichever image a composition asks for, and only that one.
#
# The stem is <recipe>-<kver>, and recipe names contain hyphens
# ("vmexit-bench-5.15.107"), so split on the LAST hyphen. That works because
# released kernel versions have none; an -rc kernel would need a different split.
$(GUEST)/vmlinux-%.img:
	@test -f $(VMIMG_DIR)/Makefile || { \
		echo "ERROR: $(VMIMG_DIR) is empty."; \
		echo "       Run: git submodule update --init --recursive"; exit 1; }
	@stem='$*'; \
	 recipe="$${stem%-*}"; kver="$${stem##*-}"; \
	 echo "  [VMIMG] recipe=$$recipe kernel=$$kver"; \
	 $(MAKE) -C $(VMIMG_DIR) image   RECIPE="$$recipe" KVER="$$kver" && \
	 $(MAKE) -C $(VMIMG_DIR) install RECIPE="$$recipe" KVER="$$kver" DESTDIR=$(GUEST_DIR)

# At ./cos build time there is no composition yet, so COMP_CONST_H is unset and
# VM_GUEST_IMAGE is empty. Build the VM_IMAGE recipe then, so a default guest
# exists; the composition's constant governs what actually gets embedded.
# The guest image is a prerequisite of simple_vmm.o, so it is already built by
# the time this runs; private only needs the realmode stub.
private: $(GUEST)/guest.img

include Makefile.subsubdir
