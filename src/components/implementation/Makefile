include Makefile.src Makefile.comp

SUBDIRS=$(filter-out skel/, $(wildcard */))
MAKEOPTIONS=-I$(shell pwd)

.PHONY: all
all:
	$(info ***********************************************)
	$(info *********[ Building Implementations ]**********)
	$(info ***********************************************)
	@for dir in $(SUBDIRS) ; do \
		$(MAKE) $(MAKEOPTIONS) -C $$dir ; \
	done

.PHONY: clean
clean:
	$(info |     [RM]   Cleaning up implementation directories)
	@for dir in $(SUBDIRS) ; do \
		$(MAKE) $(MAKEOPTIONS) -C $$dir clean ; \
	done

.PHONY: init distclean
init:
distclean:

component:
	$(MAKE) $(MAKEOPTIONS) -C $(COMP_INTERFACE) component

dependencies_info:
	$(MAKE) $(MAKEOPTIONS) -C $(COMP_INTERFACE) dependencies_info