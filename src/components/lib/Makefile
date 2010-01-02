include Makefile.src Makefile.comp

LIB_OBJS=cos_component.o heap.o
LIBS=$(LIB_OBJS:%.o=%.a)
MANDITORY=c_stub.o cos_asm_upcall.o
MAND=$(MANDITORY_LIB)

.PHONY: all
all: $(LIBS) $(MAND)

# we have to compile these without dietlibc so that there are not
# symbol conflicts and this is why we have the %.a here and don't
# output a .a file.
%.a:%.c
	$(info [CC]   Creating library file $@ from $^)
	@$(CC) $(CFLAGS) $(CINC) -o $(@:%.a=%.o) -c $<
	@$(AR) cr lib$@ $(@:%.a=%.o)

$(MAND): $(MANDITORY)
	@$(LD) -r -o $@ $^

%.o:%.S
	$(info [AS]   Compiling assembly file $^ into $@)
	@$(AS) $(ASFLAGS) $(CINC) -c -o $@ $^

clean:
	$(info [RM]   Cleaning up directory)
	@rm -f a.out *.o *.a *.d *~

init: clean all
	make -C dietlibc-0.29 clean
	make -C dietlibc-0.29
	make -C lwip/cos clean
	make -C lwip/cos
