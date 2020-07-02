include Makefile.src

default:
	$(MAKE) -C $(PLATFORM)

clean:
	$(MAKE) -C $(PLATFORM) clean

init: clean

ltests:
	$(MAKE) -C tests

#clean the linux tests
cltests:
	$(MAKE) -C tests clean
