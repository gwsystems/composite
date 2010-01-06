include Makefile.src

default:
	$(MAKE) -C module
	$(MAKE) -C net
	$(MAKE) -C link_load

cp:
	$(MAKE) -C module cp
	$(MAKE) -C net cp
	$(MAKE) -C link_load cp
# we want to avoid having to include anything in the transfer
# makefile, so intelligently make the makefile dynamically with the
# includes patched in!
	cat $(TOP_DIR)/Makefile.src ./util/Makefile.end > $(TRANS_DIR)/Makefile
	cp ./util/*.sh $(TRANS_DIR)

clean:
	$(MAKE) -C module clean
	$(MAKE) -C net clean
	$(MAKE) -C link_load clean

fresh: clean default
