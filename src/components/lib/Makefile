
# LIB_OBJS=heap.o cobj_format.o cos_kernel_api.o cos_defkernel_api.o cos_ubench.o crt.o
# LIBS=$(LIB_OBJS:%.o=%.a)
# MANDITORY=c_stub.o cos_asm_upcall.o cos_asm_ainv.o cos_component.o
# MAND=$(MANDITORY_LIB)
# SIMPLE_STACKS=c_stub.o cos_asm_upcall_simple_stacks.o cos_asm_ainv.o cos_component.o
# SIMPLE_STKLIB=simple_stklib.o

# CINC_ENV=$(CINC)
# export CINC_ENV

# # HACK: remove with new library structure
# CINC += -I$(INTERDIR)/init/

# .PHONY: all libs ps ck sinv
# all: $(LIBS) $(MAND) $(SIMPLE_STKLIB) libs sinv

# %.a:%.c
# 	$(info |     [CC]   Creating library file $@ from $^)
# 	@$(CC) $(CFLAGS) $(CINC) -o $(@:%.a=%.o) -c $<
# 	@$(AR) cr lib$@ $(@:%.a=%.o)

# $(MAND): $(MANDITORY)
# 	@$(LD) $(LDFLAGS) -r -o $@ $^

# $(SIMPLE_STKLIB): $(SIMPLE_STACKS)
# 	@$(LD) $(LDFLAGS) -r -o $@ $^

# libs:
# 	make -C sl
# 	make -C initargs

# sinv:
# 	make $(MAKEFLAGS) -C sinv_async

# %.o:%.S
# 	$(info |     [AS]   Compiling assembly file $^ into $@)
# 	@$(AS) $(ASFLAGS) $(CINC) -c -o $@ $^

# %.o:%.c
# 	$(info |     [CC]   Compiling C file $^ into $@)
# 	@$(CC) $(CFLAGS) $(CINC) -o $@ -c $^

# clean:
# 	$(info |     [RM]   Cleaning up directory)
# 	@rm -f a.out *.o *.a *.d *~
# 	make -C sl clean
# 	make -C initargs clean

# distclean:
# 	make -C musl-1.1.11 distclean
# # keep the following commands in one line. make executes each line
# # with a new shell.
# 	make -C posix clean
# 	make -C libcxx clean

# musl:
# 	cd musl-1.1.11; ./configure "CFLAGS=-m32 -O3" "LDFLAGS=-Wl,-melf_i386" --disable-shared --target=i386; cd ..
# 	make -C musl-1.1.11
# 	make -C musl-1.1.11 install

# ps:
# 	cd ps; ./configure cos x86 general; cd ..; make -C ps config ; make -C ps all


# ck:
# 	make -C ck all

# init: ck musl ps sinv all
# # keep the following commands in one line. Same as above.
# 	make -C posix
# 	make -C libcxx

include Makefile.src Makefile.comp

SUBDIRS=$(filter-out skel/, $(wildcard */))
MAKEOPTIONS=-I$(shell pwd)

.PHONY: all clean init distclean

all:
	$(info **************************************************)
	$(info **************[ Building Libraries ]**************)
	$(info **************************************************)
	@for dir in $(SUBDIRS) ; do \
		echo "*************************[TASK START]*************************";\
		echo "                    [ Building $$dir ]";\
		$(MAKE) PLATFORM=$(PLATFORM) $(MAKEOPTIONS) -C $$dir  || exit $$?; \
		echo "*************************[TASK  STOP]*************************";\
		echo ""; \
	done

clean:
	$(info |     [RM]   Cleaning up library directories)
	@for dir in $(SUBDIRS) ; do \
		echo "*************************[TASK START]*************************";\
		echo "                    [ Cleaning $$dir ]";\
		$(MAKE) PLATFORM=$(PLATFORM) $(MAKEOPTIONS) -C $$dir clean || exit $$?; \
		echo "*************************[TASK  STOP]*************************";\
		echo ""; \
	done

init: clean
	$(info **************************************************)
	$(info **********[Explicit libc initialization]**********)
	$(info **************************************************)
	$(MAKE) $(MAKEOPTIONS) -C libc init
	$(info **************************************************)
	$(info ************[ Initializing Libraries ]************)
	$(info **************************************************)
	@for dir in $(SUBDIRS) ; do \
		echo "*************************[TASK START]*************************";\
		echo "                    [ Initializing $$dir ]";\
		$(MAKE) PLATFORM=$(PLATFORM) $(MAKEOPTIONS) -C $$dir init || exit $$?; \
		echo "*************************[TASK  STOP]*************************";\
		echo ""; \
	done

distclean:
	$(info |     [RM]   Distribution cleaning up library directories)
	@for dir in $(SUBDIRS) ; do \
		echo "*************************[TASK START]*************************";\
		echo "                    [ Distcleaning $$dir ]";\
		$(MAKE) PLATFORM=$(PLATFORM) $(MAKEOPTIONS) -C $$dir distclean ; \
		echo "*************************[TASK  STOP]*************************";\
		echo ""; \
	done

LIB_DIRS := $(shell echo "$(REBUILD_DIRS)" | tr ' ' '\n' | grep '/lib/')
component_rebuild:
	$(info **************************************************)
	$(info **********[ Rebuilding Related Libraries]*********)
	$(info **************************************************)
	$(info LIB_DIRS: $(LIB_DIRS))
	@for dir in $(LIB_DIRS) ; do \
		echo "*************************[TASK START]*************************";\
		echo "                    [ Building $$dir ]";\
		$(MAKE) PLATFORM=$(PLATFORM) $(MAKEOPTIONS) -C $$dir clean || exit $$?; \
		$(MAKE) PLATFORM=$(PLATFORM) $(MAKEOPTIONS) -C $$dir  || exit $$?; \
 		echo "*************************[TASK  STOP]*************************";\
		echo ""; \
	done