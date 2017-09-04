include Makefile.src Makefile.comp

# HACK: For some reason this isn't being included here 
INC += -I../ps/

.PHONY: all

all: posix.o

posix.o: posix.c
	$(CC) $(INC) $< -o $@ -c $(CFLAGS)

clean:
	rm -f posix.o
