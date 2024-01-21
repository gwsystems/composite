include Makefile.src Makefile.comp

INTERFACES=$(filter-out skel/, $(wildcard */))
MAKEOPTIONS=-I$(shell pwd)

.PHONY: all
all:
	$(info ***********************************************)
	$(info ************[ Building interfaces ]************)
	$(info ***********************************************)
	$(info $(INTERFACES))
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

INTERFACE_DIRS := $(shell echo "$(MINLIB_DIRS)" | tr ' ' '\n' | grep '//interface//')
.PHONY: component_minlib
component_minlib:
	$(info ***********************************************)
	$(info ********[ ReBuilding MinLib Interface]*********)
	$(info ***********************************************)
	$(info $(INTERFACE_DIRS))
	@for dir in $(INTERFACE_DIRS) ; do \
		$(MAKE) $(MAKEOPTIONS) -C $$dir clean ; \
		$(MAKE) $(MAKEOPTIONS) -C $$dir ; \
	done