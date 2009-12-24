# Note: I have added -fno-merge-constants so that all strings will be
# placed in the .rodata section as opposed to .rodata.str1.x | x in
# uints
#
# -Xlinker -r is used to pass flags into the linker, in this case to
# prevent dietlibc from creating an executable and linking in many of
# its normal symbols

# I never want to touch this again, but will have to

include ../../Makefile.inc
include ../../../Makefile.inc

# These are compiled in with the components regardless of if they
# export an undefined symbol from the component.  They are a manditory
# link-in.
LIB_ASM_MANDITORY=$(LIBDIR)/$(MANDITORY_LIB)

C_OBJS=fp_rr.o cos_sched_base.o cos_scheduler.o
ASM_OBJS=s_stubfprr.o cos_asm_scheduler.o
OBJS=$(C_OBJS) $(ASM_OBJS)
COMPONENT=fprr.o
TMP_STR=tmp
INCLUDE=-I../ -I./ $(CINC)
LIB_FLAGS=-L$(LIBDIR) -L$(INTERDIR) -lcos_component -lcos_synchronization -lcos_alloc

SOURCES=$(C_OBJS:%.o=%.c)
#NOTE: assembly files don't have dependencies generated for them
DEPENDENCIES=$(SOURCES:%.c=%.d)

.PHONY: all
all: $(COMPONENT)
include $(DEPENDENCIES)

# Jumping through hoops: want to link to our libraries first so that
# symbols there are used, thus the gcc, and the ld, but then (and only
# then) do we want to use dietlibc's functions
$(COMPONENT): $(OBJS)
	$(info [LD]   Loading cos libraries into $<)
	@$(LD) -r -o $@.$(TMP_STR) $^ $(LIB_ASM_MANDITORY) $(LIB_FLAGS)
	$(info [DIET] Linking with dietlibc to produce $@)
	@$(DIETCC) $(DIETCFLAGS) -Xlinker -r -o $@ $@.$(TMP_STR)
	@rm $@.$(TMP_STR)

%.o:%.c
	$(info [CC]   Compiling $<)
	@$(CC) $(CFLAGS) $(INCLUDE) -o $@ -c $<

%.o:%.S
	$(info [AS]   Compiling assembly file $^ into $@)
	@$(AS) $(ASFLAGS) $(INCLUDE)  -c -o $@ $^

# see the make manual: create the .d dependencies from include
# statements.
%.d:%.c
	$(info [DEP]  Creating dependency file for $<)
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $(INCLUDE) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.PHONY: cp
cp:
	$(info [CP]   Copying all components to $(TRANS_DIR))
	@cp $(COMPONENT) $(TRANS_DIR)

.PHONY: clean
clean:
	$(info [RM]   Cleaning up directory)
	@rm -f a.out *.o *.a *.d *~

fresh: clean all
