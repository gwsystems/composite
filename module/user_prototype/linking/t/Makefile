# Note: I have added -fno-merge-constants so that all strings will be
# placed in the .rodata section as opposed to .rodata.str1.x | x in
# uints

CC=gcc
AR=ar
LD=ld
AS=$(CC)
AR=ar
INC_PATH=./ -I../include/
CFLAGS=-Wall -I$(INC_PATH) -O3
#-ggdb3
# -nostdinc -nostdlib -fno-merge-constants
ASFLAGS=-I$(INC_PATH) -c -nostdinc -nostdlib -fno-merge-constants
PRODUCTS=c_stub.o comp0.o s_stub1.o comp1.o s_stub2.o comp2.o s_stub3.o comp3.o s_stub4.o comp4.o s_stubprint.o print_comp.o s_stubmm.o mem_man.o cos_asm_scheduler.o s_stubfprr.o fp_rr.o cos_scheduler.o s_stubnet.o cos_net.o
COMPONENTS=c0.o c1.o c2.o c3.o c4.o print_comp.o mm.o fprr.o net.o
TRANS_DIR=/home/grad3/gabep1/transfer/

all: $(PRODUCTS) libcos_component.a cos_asm_upcall.o
	$(LD) -r -o c0.o comp0.o c_stub.o cos_asm_upcall.o -L. -lcos_component
	$(LD) -r -o c1.o comp1.o s_stub1.o c_stub.o cos_asm_upcall.o -L. -lcos_component
	$(LD) -r -o c2.o comp2.o s_stub2.o c_stub.o cos_asm_upcall.o cos_asm_scheduler.o -L. -lcos_component
	$(LD) -r -o c3.o comp3.o s_stub3.o c_stub.o cos_asm_upcall.o -L. -lcos_component
	$(LD) -r -o c4.o comp4.o s_stub4.o c_stub.o cos_asm_upcall.o -L. -lcos_component

	$(LD) -r -o mm.o mem_man.o s_stubmm.o c_stub.o cos_asm_upcall.o -L. -lcos_component
	$(LD) -r -o fprr.o fp_rr.o s_stubfprr.o c_stub.o cos_asm_upcall.o cos_asm_scheduler.o cos_scheduler.o -L. -lcos_component
	$(LD) -r -o net.o cos_net.o s_stubnet.o c_stub.o cos_asm_upcall.o -L. -lcos_component

print_comp.o: print_comp.c s_stubprint.o libcos_component.a cos_asm_upcall.o
	../dietlibc-0.29/bin-i386/diet $(CC) -fno-merge-constants -nostdinc -nostdlib -I./ -I../include -I../dietlibc-0.29/include -Xlinker -r -o pc_int.o $< cos_asm_upcall.o s_stubprint.o
# has to be a separate line as dietlibc messes up loading other libraries:
	$(LD) -r -o $@ pc_int.o -L. -lcos_component

libcos_component.a:cos_component.o cos_alloc.o cos_synchronization.o cos_asm_synchronization.o
	$(AR) r $@ $^

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $<

%.o:%.S
	$(AS) $(ASFLAGS) -o $@ $^

copy:
	cp $(COMPONENTS) $(TRANS_DIR)

clean:
	rm -rf *.o *.a $(PRODUCTS) *~



