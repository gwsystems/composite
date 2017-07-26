CFLAGS=-Wall -fno-stack-protector -I../../include -I../../../kernel/include -I../../../kernel/include/shared -I../../interface/cbuf_mgr -I../../interface/evt

.PHONY: all

all: posix.o

posix.o: posix.c
	$(CC) $(INC) $< -o $@ -c $(CFLAGS)

clean:
	rm -f posix.o
