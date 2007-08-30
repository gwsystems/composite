CC=gcc
AR=ar
LD=ld
AS=$(CC)
INC_PATH=./ -I../include/
CFLAGS=-Wall -I$(INC_PATH) -O3
ASFLAGS=-I$(INC_PATH) -c
PRODUCTS=c0.o c1.o c2.o c3.o c4.o

all: $(PRODUCTS)

%.a:%.o
	$(AR) r $@ $^

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $^

%.o:%.S
	$(AS) $(ASFLAGS) -o $@ $^

clean:
	rm -rf *.o *.a $(PRODUCTS) *~



