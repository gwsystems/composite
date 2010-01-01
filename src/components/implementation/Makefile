include Makefile.src Makefile.comp

SUBDIRS=$(wildcard */)
MAKEFLAGS=-I$(shell pwd)

.PHONY: all
all: 
	@for dir in $(SUBDIRS) ; do \
		$(MAKE) $(MAKEFLAGS) -C $$dir ; \
	done

.PHONY: cp
cp:
	$(info [CP]   Copying all components to $(TRANS_DIR))
	@for dir in $(SUBDIRS) ; do \
		$(MAKE) $(MAKEFLAGS) -C $$dir cp ; \
	done

.PHONY: clean
clean:
	$(info [RM]   Cleaning up implementation directory)
	@for dir in $(SUBDIRS) ; do \
		$(MAKE) $(MAKEFLAGS) -C $$dir clean ; \
	done

.PHONY: fresh
fresh: clean all
