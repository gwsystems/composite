include Makefile.src Makefile.comp

SUBDIRS=$(filter-out skel/, $(wildcard */))
MAKEOPTIONS+= -I$(shell pwd)
export MAKEOPTIONS

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

# NOTE: we're skipping the compilation of the libraries in the
# interfaces directories (the immediate subdirectories here) on
# composer-directed compilation. Soon, we'll remove the need for
# interface directories, and will move all of their shared code into
# the `lib/` code.
#
# NOTE: COMP_PATH is relative to the `src/component` (CDIR) directory.
component:
	$(MAKE) $(MAKEOPTIONS) -C $(CDIR)/$(COMP_PATH) component
