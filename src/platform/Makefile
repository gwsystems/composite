include Makefile.src

default:
	$(MAKE) -C $(PLATFORM)

cp:
	$(MAKE) -C $(PLATFORM) cp

clean:
	$(MAKE) -C $(PLATFORM) clean

init: clean

ltests:
	$(MAKE) -C tests

#clean the linux tests
cltests:
	$(MAKE) -C tests clean
