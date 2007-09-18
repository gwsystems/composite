CC=gcc
AR=ar
LD=ld
AS=$(CC)
AR=ar
INC_PATH=./ -I../include/
CFLAGS=-Wall -I$(INC_PATH) -O3
ASFLAGS=-I$(INC_PATH) -c
PRODUCTS=c_stub.o comp0.o s_stub1.o comp1.o s_stub2.o comp2.o s_stubprint.o print_comp.o

all: $(PRODUCTS) libcos_component.a cos_asm_upcall.o
	$(LD) -r -o c0.o comp0.o c_stub.o cos_asm_upcall.o -L. -lcos_component
	$(LD) -r -o c1.o comp1.o s_stub1.o c_stub.o cos_asm_upcall.o -L. -lcos_component
	$(LD) -r -o c2.o comp2.o s_stub2.o c_stub.o cos_asm_upcall.o -L. -lcos_component

print_comp.o: print_comp.c s_stubprint.o libcos_component.a cos_asm_upcall.o
	../dietlibc-0.29/bin-i386/diet $(CC) -nostdinc -nostdlib -I./ -I../include -I../dietlibc-0.29/include -Xlinker -r -o pc_int.o $< cos_asm_upcall.o s_stubprint.o
# has to be a separate line as dietlibc messes up loading other libraries:
	$(LD) -r -o $@ pc_int.o -L. -lcos_component

libcos_component.a:cos_component.o
	$(AR) r $@ $^

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o:%.S
	$(AS) $(ASFLAGS) -o $@ $^

clean:
	rm -rf *.o *.a $(PRODUCTS) *~



