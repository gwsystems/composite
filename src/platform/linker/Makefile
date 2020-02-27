include Makefile.src

CC=gcc
LD=ld
CFLAGS=-m32 -D__x86__ -D_GNU_SOURCE -lpthread -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-pointer-to-int-cast -Wno-int-to-pointer-cast -Wno-format -ggdb3 -I$(SHAREDINC)
LDFLAGS=-melf_i386
PRODUCTS=cos_linker gen_client_stub

default: $(PRODUCTS)
	@echo

%.o: %.c
	@$(CC) $(CFLAGS) -I$(CDIR)/include/ -c -o $@ $^

gen_client_stub: gen_client_stub.o
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -o $@ $<

cos_linker: main.o deserialize.o globals.o genstubs.o loadall.o printobjs.o output.o prepsymbs.o vdc.o vds.o $(CDIR)/lib/cobj_format.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -o $@ $^ -L/usr/lib -lbfd -I$(CDIR)/include

cp:
	$(info |     [CP]   Copying cos_linker and gen_client_stub to $(TRANS_DIR))
	@cp cos_linker gen_client_stub $(TRANS_DIR)

clean:
	@rm -f *~ *.o *.a $(PRODUCTS)

fresh: clean default
