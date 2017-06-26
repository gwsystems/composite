include Makefile.src

default:
	$(MAKE) -C $(PLATFORM)
	$(MAKE) -C linker

cp:
	$(MAKE) -C $(PLATFORM) cp
	$(MAKE) -C linker cp

clean:
	$(MAKE) -C $(PLATFORM) clean
	$(MAKE) -C linker clean

init: clean

ltests:
	$(MAKE) -C tests

#clean the linux tests
cltests:
	$(MAKE) -C tests clean
