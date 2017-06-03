include Makefile.src Makefile.comp

INTERFACES=$(wildcard */)

.PHONY: all
all:
	$(info ***********************************************)
	$(info ************[ Building interfaces ]************)
	$(info ***********************************************)
	@for dir in $(INTERFACES) ; do \
		$(MAKE) -C $$dir ; \
	done

.PHONY: clean
clean:
	$(info |     [RM]   Cleaning up interface directories)
	@for dir in $(INTERFACES) ; do \
		$(MAKE) -C $$dir clean ; \
	done

.PHONY: cp
cp:

.PHONY: init
init: clean

#.PHONY: idl
idl:
	$(info |     [IDL]   Compiling interface definitions)
	@for dir in $(INTERFACES) ; do \
		$(MAKE) -C $$dir idl ; \
	done
