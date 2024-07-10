include Makefile.src Makefile.comp

INTERFACES=$(filter-out skel/, $(wildcard */))
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

INTERFACE_DIRS := $(shell echo "$(REBUILD_DIRS)" | tr ' ' '\n' | grep '/interface/')
component_rebuild:
	$(info ***********************************************)
	$(info *******[ Rebuilding Releated Interface]********)
	$(info ***********************************************)
	$(info $(INTERFACE_DIRS))
	@for dir in $(INTERFACE_DIRS) ; do \
		$(MAKE) $(MAKEOPTIONS) -C $$dir clean ; \
		$(MAKE) $(MAKEOPTIONS) -C $$dir ; \
	done