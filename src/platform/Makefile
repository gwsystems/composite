include Makefile.src

default:
	$(MAKE) -C $(PLATFORM)
	$(MAKE) -C linker

cp:
	if [ -f linux/module/cos.ko ]; then $(MAKE) -C linux cp; else $(MAKE) -C i386 cp; fi
	$(MAKE) -C linker cp

clean:
	$(MAKE) -C linux clean
	$(MAKE) -C i386 clean
	$(MAKE) -C linker clean

init: clean

ltests:
	$(MAKE) -C tests

#clean the linux tests
cltests:
	$(MAKE) -C tests clean
