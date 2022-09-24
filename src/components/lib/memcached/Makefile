MEMCACHED_DIR		= memcached
MEMCACHE_INC_DIR	= cos_adapter/sys_include/

LIBRARY_OUTPUT = memcached

OBJECT_OUTPUT =

INCLUDE_PATHS = ./ memcached/ cos_adapter/

INTERFACE_DEPENDENCIES = netshmem

LIBRARY_DEPENDENCIES = component posix_cap posix_sched

# this is to stop non supported build target
# this will override $(OBJS) so that compiler will not compile target files under unsupported cases
ifneq ($(PLATFORM),x86_64)
override OBJS =
endif

MEMCACHED_SRC_FILES=$(MEMCACHED_DIR)/hash.c $(MEMCACHED_DIR)/jenkins_hash.c $(MEMCACHED_DIR)/murmur3_hash.c \
	$(MEMCACHED_DIR)/slabs.c $(MEMCACHED_DIR)/items.c \
	$(MEMCACHED_DIR)/assoc.c $(MEMCACHED_DIR)/thread.c \
	$(MEMCACHED_DIR)/daemon.c $(MEMCACHED_DIR)/stats_prefix.c \
	$(MEMCACHED_DIR)/util.c $(MEMCACHED_DIR)/cache.c \
	$(MEMCACHED_DIR)/bipbuffer.c $(MEMCACHED_DIR)/base64.c \
	$(MEMCACHED_DIR)/logger.c $(MEMCACHED_DIR)/crawler.c \
	$(MEMCACHED_DIR)/itoa_ljust.c $(MEMCACHED_DIR)/slab_automove.c \
	$(MEMCACHED_DIR)/authfile.c $(MEMCACHED_DIR)/restart.c \
	$(MEMCACHED_DIR)/proto_text.c $(MEMCACHED_DIR)/proto_bin.c \
	$(MEMCACHED_DIR)/memcached.c \
	cos_adapter/cos_mc_adapter.c 

CFILES+=$(MEMCACHED_SRC_FILES)

include Makefile.lib
CINC+=$(foreach I, $(INCLUDE_PATHS), -I$(I))
# Memcached source dose have some code that triggers some warnings, we close them here
CFLAGS+=-DCOS_MEMCACHED -DHAVE_CONFIG_H -I$(MEMCACHE_INC_DIR) -Wno-implicit-fallthrough -Wno-sign-compare -Wno-unused-but-set-parameter -Wno-old-style-declaration

.PHONY: all clean distclean init build 

build:
ifneq ($(PLATFORM),x86_64)
	@echo "Platform: $(PLATFORM) is not supported in memcached"
endif

armv7a_init:

i386_init:

x86_64_init: 

platform_init:
	@find /usr/lib/gcc/x86_64-linux-gnu/$(shell gcc -dumpversion)/include/ -iname "*.h" | grep -v "std.*" | xargs cp -rft $(MEMCACHE_INC_DIR)
	make $(PLATFORM)_init

init: platform_init

distclean:
	$(info memcached distclean)
	rm $(OBJS) $(CFILES:%.c=%.d)
clean:
