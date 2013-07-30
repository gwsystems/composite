CC=$(HOME)/research/composite/src/components/lib/dietlibc-0.29/bin-i386/diet gcc
AR=ar
CFLAGS=-Wall -fno-stack-protector -I../../include -I../../../kernel/include -I../../../kernel/include/shared -I../../interface/cbuf_c -I../../interface/cbufp -I../../interface/evt

all: posix.a

clean:
	rm -f posix.a posix.o

posix.a: posix.o
	$(AR) rcs $@ $^

posix.o: posix.c posix.h
	$(CC) $(INC) $< -o $@ -c $(CFLAGS)
