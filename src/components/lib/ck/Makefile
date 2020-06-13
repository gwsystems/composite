include Makefile.src Makefile.comp

NCORES=1

.PHONY: all ck install uninstall
all: uninstall ck install 

uninstall:
	cd ck; make uninstall; cd ..

install:
	cd ck; make install; cd ..

ck:
	cd ck; ./configure --platform=i586 --disable-double --without-pic --cores=$(NCORES) --prefix=$(CKDIR); make all; cd ..
