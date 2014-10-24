include Makefile.src

default:
	$(MAKE) -C i386
	$(MAKE) -C linker
	$(MAKE) -C linux

cp:
	$(MAKE) -C i386 cp
	$(MAKE) -C linker cp
	$(MAKE) -C linux cp

clean:
	$(MAKE) -C i386 clean
	$(MAKE) -C linker clean
	$(MAKE) -C linux clean

init: clean default

ltests:
	$(MAKE) -C tests 

#clean the linux tests
cltests:
	$(MAKE) -C tests clean
