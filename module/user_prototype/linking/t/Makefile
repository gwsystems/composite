CC=gcc
AR=ar
LD=ld
AS=$(CC)
INC_PATH=./ -I../include/
CFLAGS=-Wall -I$(INC_PATH) -O3
ASFLAGS=-I$(INC_PATH) -c
PRODUCTS=c_stub.o comp0.o s_stub1.o comp1.o s_stub2.o comp2.o s_stubprint.o

all: $(PRODUCTS) print_comp.o
	$(LD) -r -o c0.o comp0.o c_stub.o
	$(LD) -r -o c1.o comp1.o s_stub1.o c_stub.o
	$(LD) -r -o c2.o comp2.o s_stub2.o c_stub.o 

print_comp.o: print_comp.c s_stubprint.o
	../dietlibc-0.29/bin-i386/diet $(CC) -nostdinc -nostdlib -I./ -I../include -I../dietlibc-0.29/include -Xlinker -r -o $@ $< s_stubprint.o

%.a:%.o
	$(AR) r $@ $^

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o:%.S
	$(AS) $(ASFLAGS) -o $@ $^

clean:
	rm -rf *.o *.a $(PRODUCTS) *~



