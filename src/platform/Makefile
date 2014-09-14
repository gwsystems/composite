include Makefile.src

default:
	$(MAKE) -C i386
	$(MAKE) -C loader

cp:
	$(MAKE) -C i386 cp
	$(MAKE) -C loader cp

clean:
	$(MAKE) -C i386 clean
	$(MAKE) -C loader clean

init: clean default

ltests:
	$(MAKE) -C tests 

#clean the linux tests
cltests:
	$(MAKE) -C tests clean
