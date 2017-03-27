<<<<<<< HEAD
CC=$(HOME)/research/composite/src/components/lib/musl-1.1.11/bin/musl-gcc
CFLAGS=-Wall -fno-stack-protector -I../../include -I../../../kernel/include -I../../../kernel/include/shared -I../../interface/cbuf_mgr -I../../interface/evt
=======
CC=$(CDIR)/lib/dietlibc-0.29/bin-i386/diet gcc
CFLAGS=-Wall -fno-stack-protector -I../../include -I../../../kernel/include -I../../../kernel/include/shared -I../../interface/cbuf_c -I../../interface/cbufp -I../../interface/evt
>>>>>>> 30617db6d411a37cacea71d2cc806cfb300d9c27

.PHONY: all

all: posix.o

posix.o: posix.c
	$(CC) $(INC) $< -o $@ -c $(CFLAGS)

clean:
	rm -f posix.o
