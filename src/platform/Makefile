include Makefile.src

default:
	$(MAKE) -C $(PLATFORM)
	$(MAKE) -C linker

cp:
	$(MAKE) -C i386 cp
	$(MAKE) -C linker cp

clean:
	$(MAKE) -C i386 clean
	$(MAKE) -C linker clean

init: clean

ltests:
	$(MAKE) -C tests

#clean the linux tests
cltests:
	$(MAKE) -C tests clean
