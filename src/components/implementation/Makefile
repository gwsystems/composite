include Makefile.src Makefile.comp

SUBDIRS=$(wildcard */)
MAKEOPTIONS=-I$(shell pwd)

.PHONY: all
all:
	$(info ***********************************************)
	$(info *********[ Building Implementations ]**********)
	$(info ***********************************************)
	@for dir in $(SUBDIRS) ; do \
		$(MAKE) $(MAKEOPTIONS) -C $$dir ; \
	done

.PHONY: cp
cp:
	$(info |     [CP]   Copying all components to $(TRANS_DIR))
	@for dir in $(SUBDIRS) ; do \
		$(MAKE) $(MAKEOPTIONS) -C $$dir cp ; \
	done

.PHONY: clean
clean:
	$(info |     [RM]   Cleaning up implementation directories)
	@for dir in $(SUBDIRS) ; do \
		$(MAKE) $(MAKEOPTIONS) -C $$dir clean ; \
	done

.PHONY: init
init: clean

component:
	$(MAKE) $(MAKEOPTIONS) -C $(COMP_INTERFACE) component
