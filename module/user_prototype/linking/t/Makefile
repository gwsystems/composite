# Note: I have added -fno-merge-constants so that all strings will be
# placed in the .rodata section as opposed to .rodata.str1.x | x in
# uints
#
# -Xlinker -r is used to pass flags into the linker, in this case to
# prevent dietlibc from creating an executable and linking in many of
# its normal symbols

# I never want to touch this again, but will have to

CC=gcc
AR=ar
LD=ld
AS=$(CC)

#DIETDIR=/home/grad3/gabep1/research/code/composite/composite/module/user_prototype/linking/dietlibc-0.29
DIETDIR=../dietlibc-0.29
DIETBIN=$(DIETDIR)/bin-i386/diet
DIETCC=$(DIETBIN) $(CC)
DIETINC=-I$(DIETDIR)/include

LWIPBASE=../lwip
LWIPDIR=$(LWIPBASE)/lwip-1.3.0
LWIPOBJ=$(LWIPBASE)/cos/net_stack.o
LWIPINC=-I$(LWIPDIR)/src/include -I$(LWIPDIR)/src/include/ipv4/ -I$(LWIPBASE)/cos/include/ 

TRANS_DIR=/home/grad3/gabep1/transfer/
INC_PATH=-I./
# FIXME: dietlibc is now compiled with fno merge constants which is
# probably ballooning its size.  Fix this in cos_loader.
SHARED_FLAGS=-fno-merge-constants -nostdinc -nostdlib
OPT= -ggdb3 #-O3
CFLAGS=-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -fno-omit-frame-pointer $(INC_PATH) $(DIETINC) $(LWIPINC) $(OPT) $(SHARED_FLAGS)
DIETCFLAGS=$(CFLAGS) -Xlinker -r
ASFLAGS=$(INC_PATH) $(SHARED_FLAGS)

LIB=libcos_component.a
LIB_ASM_OBJS=cos_asm_scheduler.o
LIB_C_OBJS=cos_component.o cos_alloc.o cos_synchronization.o cos_scheduler.o
LIB_C=$(LIB_C_OBJS:%.o=%.a)#$(wildcard *.c))
# Order here is important: cos_scheduler.o must be before
# cos_asm_scheduler.o so that it includes cos_force_sched_link which
# forces the assembly file to be included.  Without it, the weak
# symbols preclude it linking.
LIB_OBJS=$(LIB_C_OBJS) $(LIB_ASM_OBJS) 
# These are compiled in with the components regardless of if they
# export an undefined symbol from the component.  They are a manditory
# link-in.
LIB_ASM_MANDITORY=cos_asm_upcall.o c_stub.o

C_COMPS=comp0.o comp1.o comp2.o comp3.o comp4.o print_comp.o mem_man.o fp_rr.o cos_net.o ds.o uc_test.o wf_test.o lock.o timed_event.o mailbox.o mpd_mgr.o evt.o fd_api.o conn_mgr.o https.o stat_gather.o stack_trace.o
ASM_STUBS=s_stub1.o s_stub2.o s_stub3.o s_stub4.o s_stubprint.o s_stubmm.o s_stubfprr.o s_stubnet.o s_stubds.o s_stublock.o s_stubte.o s_stubmb.o s_stube.o s_stubfd.o s_stubhttp.o s_stubst.o
PRODUCTS=$(C_COMPS) $(ASM_STUBS)
COMPONENTS=c0.o c1.o c2.o c3.o c4.o print.o mm.o fprr.o net.o d.o uc.o wftest.o l.o te.o mb.o mpd.o e.o fd.o conn.o http.o stat.o st.o
TMP_STR=tmp

SOURCES=$(C_COMPS:%.o=%.c) $(LIB_C_OBJS:%.o=%.c)
#NOTE: assembly files don't have dependencies generated for them
DEPENDENCIES=$(SOURCES:%.c=%.d)

.PHONY: all
all: $(LIB) $(LIB_ASM_MANDITORY) $(PRODUCTS) stubs
include $(DEPENDENCIES)

stubs: $(PRODUCTS)
	$(info [LD]   Linking in the server stubs to components)
	@$(LD) -r -o c0.o comp0.o
	@$(LD) -r -o c1.o comp1.o s_stub1.o
	@$(LD) -r -o c2.o comp2.o s_stub2.o
	@$(LD) -r -o c3.o comp3.o s_stub3.o
	@$(LD) -r -o c4.o comp4.o s_stub4.o

	@$(LD) -r -o print.o print_comp.o s_stubprint.o
	@$(LD) -r -o mm.o mem_man.o s_stubmm.o
	@$(LD) -r -o fprr.o fp_rr.o s_stubfprr.o
	@$(LD) -r -o wftest.o wf_test.o
	@$(LD) -r -o te.o timed_event.o s_stubte.o
	@$(LD) -r -o l.o lock.o s_stublock.o
	@$(LD) -r -o d.o ds.o s_stubds.o
	@$(LD) -r -o mb.o mailbox.o s_stubmb.o
	@$(LD) -r -o uc.o uc_test.o
	@$(LD) -r -o mpd.o mpd_mgr.o
	@$(LD) -r -o net.o cos_net.o s_stubnet.o
	@$(LD) -r -o e.o evt.o s_stube.o
	@$(LD) -r -o fd.o fd_api.o s_stubfd.o
	@$(LD) -r -o conn.o conn_mgr.o
	@$(LD) -r -o http.o https.o s_stubhttp.o
	@$(LD) -r -o stat.o stat_gather.o
	@$(LD) -r -o st.o stack_trace.o s_stubst.o

# This is specialized so that we can link with lwip, and it can link with dietlibc
cos_net.o: cos_net.c net_stack.o
	$(info [CC]   Compiling $< (w/lwip @ $(LWIPBASE)/cos))
	@$(CC) $(CFLAGS) -o $@.$(TMP_STR)1 -c $<
	@$(LD) -r -o $@.$(TMP_STR)2 $@.$(TMP_STR)1 $(LIB_ASM_MANDITORY) net_stack.o -L. -lcos_component
	$(info [DIET] Linking with dietlibc to produce $@)
	@$(DIETCC) $(DIETCFLAGS) -Xlinker -r -o $@ $@.$(TMP_STR)2
	@rm $@.$(TMP_STR)1 $@.$(TMP_STR)2

net_stack.o: 
	$(info [CC] lwip in $(LWIPBASE)/cos)
	make -C $(LWIPBASE)/cos 
	cp $(LWIPOBJ) .

$(LIB):$(LIB_C) $(LIB_ASM_OBJS)
	$(info [AR]   Creating $@ from $(LIB_OBJS))
	@$(AR) cr $@ $(LIB_OBJS)

# we have to compile these without dietlibc so that there are not
# symbol conflicts and this is why we have the %.a here and don't
# output a .a file.
%.a:%.c
	$(info [CC]   Creating library file from $^)
	@$(CC) $(CFLAGS) -o $(@:%.a=%.o) -c $<

# Jumping through hoops: want to link to our libraries first so that
# symbols there are used, thus the gcc, and the ld, but then (and only
# then) do we want to use dietlibc's functions
%.o:%.c
	$(info [CC]   Compiling $<)
	@$(CC) $(CFLAGS) -o $@.$(TMP_STR)1 -c $<
	$(info [LD]   Loading cos libraries ($(LIB), ...) into $<)
	@$(LD) -r -o $@.$(TMP_STR)2 $@.$(TMP_STR)1 $(LIB_ASM_MANDITORY) -L. -lcos_component
	$(info [DIET] Linking with dietlibc to produce $@)
	@$(DIETCC) $(DIETCFLAGS) -Xlinker -r -o $@ $@.$(TMP_STR)2
	@rm $@.$(TMP_STR)1 $@.$(TMP_STR)2

%.o:%.S
	$(info [AS]   Compiling assembly file $^ into $@)
	@$(AS) $(ASFLAGS) -c -o $@ $^

# see the make manual: create the .d dependencies from include
# statements.
%.d:%.c
	$(info [DEP]  Creating dependency file for $<)
	@set -e; rm -f $@; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.PHONY: copy
copy:
	$(info [CP]   Copying all components to $(TRANS_DIR))
	@cp $(COMPONENTS) $(TRANS_DIR)

.PHONY: clean
clean:
	$(info [RM]   Cleaning up directory)
	@rm -f a.out *.o *.a *.d *~
	@make -C $(LWIPBASE)/cos clean


