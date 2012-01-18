HELPERS=$(wildcard */)

all:
	@for dir in $(HELPERS) ; do \
		make -C $$dir ; \
	done

cp:
	@for dir in $(HELPERS) ; do \
		make -C $$dir cp; \
	done

clean:
	@for dir in $(HELPERS) ; do \
		make -C $$dir clean ; \
	done
