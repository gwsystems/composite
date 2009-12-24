include ../../Makefile.inc
include ../Makefile.inc

LIB_OBJS=cos_alloc.o cos_synchronization.o
LIBS=$(LIB_OBJS:%.o=%.a)
ASM_STUBS=s_stubprint.o s_stubmm.o s_stubnet.o s_stubds.o s_stublock.o s_stubte.o s_stubmb.o s_stube.o s_stubfd.o s_stubhttp.o s_stubst.o s_stubsc.o s_stubcm.o s_stubif.o s_stubip.o s_stubainv.o s_stubfn.o s_stubport.o s_stubschedconf.o s_stubbc.o

.PHONY: all
all: $(ASM_STUBS) $(LIBS)

# we have to compile these without dietlibc so that there are not
# symbol conflicts and this is why we have the %.a here and don't
# output a .a file.
%.a:%.c
	$(info [CC]   Creating library file $@ from $^)
	@$(CC) $(CFLAGS) $(CINC) -o $(@:%.a=%.o) -c $<
	@$(AR) cr lib$@ $(@:%.a=%.o)

%.o:%.S
	$(info [AS]   Compiling assembly file $^ into $@)
	@$(AS) $(ASFLAGS) -c -o $@ $^

clean:
	$(info [RM]   Cleaning up directory)
	@rm -f a.out *.o *.a *.d *~

fresh: clean all
