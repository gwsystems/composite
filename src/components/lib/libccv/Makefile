CC := ../musl-1.1.11/bin/musl-gcc
AR := ar
CFLAGS := -Wall -nostdinc -nostdlib -fno-stack-protector

.PHONY: all clean

all: cos_libccv.a

clean:
	rm -f *.o cos_libccv.a

ccv_io.o: ccv_io.c ccv.h ccv_internal.h io/*.c
	$(CC) $< -o $@ -c $(CFLAGS) 

%.o: %.c ccv.h ccv_internal.h
	$(CC) $< -o $@ -c $(CFLAGS)

cos_libccv.a:ccv_basic.o ccv_classic.o ccv_bbf.o ccv_util.o ccv_memory.o ccv_cache.o ccv_resample.o ccv_io.o ccv_math.o 3rdparty/sha1/sha1.o
	$(AR) rcs $@ $^
