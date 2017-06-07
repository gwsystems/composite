include Makefile.src Makefile.comp
SUBDIRS=$(wildcard */)
MAKEFLAGS=-I$(shell pwd)

.PHONY: all
all:
	$(info ***********************************************)
	$(info *********[ Building Implementations ]**********)
	$(info ***********************************************)
	@for dir in $(SUBDIRS) ; do \
		$(MAKE) $(MAKEFLAGS) -C $$dir ; \
	done

.PHONY: cp
cp:
	$(info |     [CP]   Copying all components to $(TRANS_DIR))
	@for dir in $(SUBDIRS) ; do \
		$(MAKE) $(MAKEFLAGS) -C $$dir cp ; \
	done

.PHONY: clean
clean:
	$(info |     [RM]   Cleaning up implementation directories)
	@for dir in $(SUBDIRS) ; do \
		$(MAKE) $(MAKEFLAGS) -C $$dir clean ; \
	done

.PHONY: init
init: clean
