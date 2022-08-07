LIBRARY_OUTPUT = memcached

OBJECT_OUTPUT =

INCLUDE_PATHS = memcached/

INTERFACE_DEPENDENCIES =

LIBRARY_DEPENDENCIES = 

include Makefile.src Makefile.comp Makefile.dependencies

.PHONY: all clean init distclean config clean_lib armv7a_config i386_config
CINC_ENV = $(CINC) $(DEPENDENCY_INCPATH)

CFLAGS   = $(CINC_ENV) -Wno-address-of-packed-member
export CINC_ENV
export CFLAGS

all: config
	$(info Building memcached library...)

clean_lib:
#	@rm -f libmemcached.a
#	make -C memcached clean

clean:

distclean: config clean_lib

build:
ifneq ($(PLATFORM),x86_64)
	@echo "Platform: $(PLATFORM) is not supported in Memcached"
endif
#	make -C memcached

armv7a_config:
	$(info Configuring the ps library)

i386_config:
	$(info Configuring the ps library)

x86_64_config:
	$(info Configuring the ps library)
#	cd memcached; ./autogen.sh && ./configure ; cd ..

config: $(PLATFORM)_config

init: distclean all
