CC=$(HOME)/research/composite/src/components/lib/dietlibc-0.29/bin-i386/diet gcc
CFLAGS=-Wall -fno-stack-protector -I../../include -I../../../kernel/include -I../../../kernel/include/shared -I../../interface/cbuf_c -I../../interface/cbufp -I../../interface/evt

.PHONY: all

all: posix.o

posix.o: posix.c
	$(CC) $(INC) $< -o $@ -c $(CFLAGS)

clean:
	rm -f posix.o
