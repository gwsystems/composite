include Makefile.src Makefile.comp

INTERFACES=$(wildcard */)
MAKEOPTIONS=-I$(shell pwd)

.PHONY: all
all:
	$(info ***********************************************)
	$(info ************[ Building interfaces ]************)
	$(info ***********************************************)
	@for dir in $(INTERFACES) ; do \
		$(MAKE) $(MAKEOPTIONS) -C $$dir ; \
	done

.PHONY: clean
clean:
	$(info |     [RM]   Cleaning up interface directories)
	@for dir in $(INTERFACES) ; do \
		$(MAKE) $(MAKEOPTIONS) -C $$dir clean ; \
	done

.PHONY: cp
cp:

.PHONY: init
init: clean
