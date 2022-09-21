LWIP_DIR		= lwip/src
LWIP_INC_DIR		= $(LWIP_DIR)/include

LWIP_ADAPTER_DIR	= cos_adapter/

LIBRARY_OUTPUT = lwip

OBJECT_OUTPUT =

INCLUDE_PATHS = cos_adapter lwip/src/include

INTERFACE_DEPENDENCIES =

LIBRARY_DEPENDENCIES = component

# this is to stop non supported build target
# this will override $(OBJS) so that compiler will not compile target files under unsupported cases
ifneq ($(PLATFORM),x86_64)
override OBJS =
endif

# CFILES+=$(wildcard adapter/*.c)
# SFILES+=$(wildcard adapter/*.S)

# LWIP_CORE_FILES, LWIP_CORE_IPV4_FILES: The minimum set of files needed for lwIP.
LWIP_CORE_FILES=$(LWIP_DIR)/core/mem.c $(LWIP_DIR)/core/memp.c $(LWIP_DIR)/core/netif.c \
	$(LWIP_DIR)/core/pbuf.c $(LWIP_DIR)/core/raw.c \
	$(LWIP_DIR)/core/stats.c $(LWIP_DIR)/core/sys.c \
	$(LWIP_DIR)/core/tcp.c $(LWIP_DIR)/core/tcp_in.c \
	$(LWIP_DIR)/core/tcp_out.c $(LWIP_DIR)/core/udp.c \
	$(LWIP_DIR)/core/init.c $(LWIP_DIR)/core/inet_chksum.c \
	$(LWIP_DIR)/core/def.c $(LWIP_DIR)/core/timeouts.c \
	$(LWIP_DIR)/core/ip.c \
	$(LWIP_DIR)/api/err.c

LWIP_CORE_IPV4_FILES=$(LWIP_DIR)/core/ipv4/icmp.c $(LWIP_DIR)/core/ipv4/ip4.c \
	$(LWIP_DIR)/core/ipv4/ip4_addr.c $(LWIP_DIR)/core/ipv4/ip4_frag.c $(LWIP_DIR)/core/ipv4/etharp.c $(LWIP_DIR)/netif/ethernet.c
COS_LWIP_ADAPTER_FILES=$(LWIP_ADAPTER_DIR)/sys_arch.c

LWIP_FILES=$(LWIP_CORE_FILES) $(LWIP_CORE_IPV4_FILES) $(COS_LWIP_ADAPTER_FILES)
LWIP_FILESW=$(wildcard $(LWIP_FILES))

CFILES+=$(LWIP_FILESW)

include Makefile.lib
CINC+=$(foreach I, $(INCLUDE_PATHS), -I$(I))

.PHONY: all   clean distclean init build 

build:
ifneq ($(PLATFORM),x86_64)
	@echo "Platform: $(PLATFORM) is not supported in LWIP"
endif

armv7a_init:

i386_init:

x86_64_init: 

platform_init:
	make $(PLATFORM)_init

init: platform_init

distclean:
	$(info lwip distclean)
	rm $(OBJS) $(CFILES:%.c=%.d)
clean:
