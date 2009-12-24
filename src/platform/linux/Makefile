include ../../Makefile.inc

default:
	$(MAKE) -C module
	$(MAKE) -C net
	$(MAKE) -C link_load

cp:
	$(MAKE) -C module cp
	$(MAKE) -C net cp
	$(MAKE) -C link_load cp
	cp ./util/run.sh ./util/Makefile $(TRANS_DIR)

clean:
	$(MAKE) -C module clean
	$(MAKE) -C net clean
	$(MAKE) -C link_load clean

fresh: clean default
