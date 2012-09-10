include Makefile.src

default:
	$(MAKE) -C linux

cp:
	$(MAKE) -C linux cp

clean:
	$(MAKE) -C linux clean

init: clean default

ltests:
	$(MAKE) -C tests 

#clean the linux tests
cltests:
	$(MAKE) -C tests clean
