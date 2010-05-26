include Makefile.src Makefile.comp

LIB_OBJS=cos_component.o heap.o cobj_format.o
LIBS=$(LIB_OBJS:%.o=%.a)
MANDITORY=c_stub.o cos_asm_upcall.o
MAND=$(MANDITORY_LIB)
SIMPLE_STACKS=c_stub.o cos_asm_upcall_simple_stacks.o
SIMPLE_STKLIB=simple_stklib.o

.PHONY: all
all: $(LIBS) $(MAND) $(SIMPLE_STKLIB)

# we have to compile these without dietlibc so that there are not
# symbol conflicts and this is why we have the %.a here and don't
# output a .a file.
%.a:%.c
	$(info [CC]   Creating library file $@ from $^)
	@$(CC) $(CFLAGS) $(CINC) -o $(@:%.a=%.o) -c $<
	@$(AR) cr lib$@ $(@:%.a=%.o)

$(MAND): $(MANDITORY)
	@$(LD) -r -o $@ $^


$(SIMPLE_STKLIB): $(SIMPLE_STACKS)
	@$(LD) -r -o $@ $^

%.o:%.S
	$(info [AS]   Compiling assembly file $^ into $@)
	@$(AS) $(ASFLAGS) $(CINC) -c -o $@ $^

clean:
	$(info [RM]   Cleaning up directory)
	@rm -f a.out *.o *.a *.d *~

distclean:
	make -C dietlibc-0.29 clean
	make -C lwip/cos clean	

init: clean distclean all
	make -C dietlibc-0.29
	make -C lwip/cos
