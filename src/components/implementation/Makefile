# Note: I have added -fno-merge-constants so that all strings will be
# placed in the .rodata section as opposed to .rodata.str1.x | x in
# uints
#
# -Xlinker -r is used to pass flags into the linker, in this case to
# prevent dietlibc from creating an executable and linking in many of
# its normal symbols

# I never want to touch this again, but will have to

include ../../Makefile.inc
include ../Makefile.inc

#LIB=libcos_component.a
#LIB_C_OBJS=cos_component.o cos_alloc.o cos_synchronization.o
#LIB_C=$(LIB_C_OBJS:%.o=%.a)#$(wildcard *.c))
#LIB_OBJS=$(LIB_C_OBJS)
LIB_FLAGS=-L$(LIBDIR) -L$(INTERDIR) -lcos_component -lcos_synchronization -lcos_alloc

# These are compiled in with the components regardless of if they
# export an undefined symbol from the component.  They are a manditory
# link-in.
LIB_ASM_MANDITORY=$(LIBDIR)/$(MANDITORY_LIB)

C_COMPS=comp0.o print_comp.o mem_man.o cos_net.o lock.o timed_event.o mpd_mgr.o evt.o fd_api.o conn_mgr.o https.o stat_gather.o stack_trace.o static_content.o content_mgr.o netif.o cos_ip.o async_inv.o fake_net.o persist_cgi.o port_mgr.o sched_config.o base_case.o
PRODUCTS=$(C_COMPS)
COMPONENTS=c0.o print.o mm.o net.o l.o te.o mpd.o e.o fd.o conn.o http.o stat.o st.o cm.o sc.o if.o ip.o ainv.o fn.o cgi.o fd2.o fd3.o cgi2.o port.o ainv2.o schedconf.o bc.o
TMP_STR=tmp

SOURCES=$(C_COMPS:%.o=%.c) $(LIB_C_OBJS:%.o=%.c) 
#NOTE: assembly files don't have dependencies generated for them
DEPENDENCIES=$(SOURCES:%.c=%.d)

.PHONY: all
all: $(MPDLIB) $(PRODUCTS) stubs
include $(DEPENDENCIES)

stubs: $(PRODUCTS)
	$(info [CC]   Compiling heap.c)
	@$(CC) $(CFLAGS) $(CINC) -c -o heap.o heap.c

	$(info [LD]   Linking in the server stubs to components)
	@$(LD) -r -o c0.o comp0.o
	@$(LD) -r -o print.o print_comp.o $(INTERDIR)/s_stubprint.o
	@$(LD) -r -o mm.o mem_man.o $(INTERDIR)/s_stubmm.o
	@$(LD) -r -o te.o timed_event.o $(INTERDIR)/s_stubte.o
	@$(LD) -r -o l.o lock.o $(INTERDIR)/s_stublock.o
	@$(LD) -r -o net.o cos_net.o $(INTERDIR)/s_stubnet.o
	@$(LD) -r -o e.o evt.o $(INTERDIR)/s_stube.o
	@$(LD) -r -o fd.o fd_api.o $(INTERDIR)/s_stubfd.o
	@$(LD) -r -o conn.o conn_mgr.o
	@$(LD) -r -o http.o https.o $(INTERDIR)/s_stubhttp.o
	@$(LD) -r -o stat.o stat_gather.o
	@$(LD) -r -o st.o stack_trace.o $(INTERDIR)/s_stubst.o
	@$(LD) -r -o sc.o static_content.o $(INTERDIR)/s_stubsc.o
	@$(LD) -r -o cm.o content_mgr.o $(INTERDIR)/s_stubcm.o
	@$(LD) -r -o if.o netif.o $(INTERDIR)/s_stubif.o
	@$(LD) -r -o ip.o cos_ip.o $(INTERDIR)/s_stubip.o
	@$(LD) -r -o port.o port_mgr.o $(INTERDIR)/s_stubport.o
	@$(LD) -r -o ainv.o async_inv.o $(INTERDIR)/s_stubainv.o
	@$(LD) -r -o fn.o fake_net.o $(INTERDIR)/s_stubfn.o

	@cp fd.o fd2.o
	@$(LD) -r -o cgi.o persist_cgi.o

	@cp ainv.o ainv2.o
	@cp fd.o fd3.o
	@cp cgi.o cgi2.o

	@$(LD) -r -o mpd.o mpd_mgr.o heap.o
	@$(LD) -r -o bc.o base_case.o $(INTERDIR)/s_stubbc.o
	@$(LD) -r -o schedconf.o sched_config.o $(INTERDIR)/s_stubschedconf.o

	make -C sched

# This is specialized so that we can link with lwip, and it can link with dietlibc
cos_net.o: cos_net.c net_stack.o
	$(info [CC]   Compiling $< (w/lwip @ $(LWIPBASE)/cos))
	@$(CC) $(CFLAGS) $(CINC) -o $@.$(TMP_STR)1 -c $<
	@$(LD) -r -o $@.$(TMP_STR)2 $@.$(TMP_STR)1 $(LIB_ASM_MANDITORY) net_stack.o $(LIB_FLAGS)
	$(info [DIET] Linking with dietlibc to produce $@)
	@$(DIETCC) $(DIETCFLAGS) -Xlinker -r -o $@ $@.$(TMP_STR)2
	@rm $@.$(TMP_STR)1 $@.$(TMP_STR)2

net_stack.o: 
	$(info [CC] lwip in $(LWIPBASE)/cos)
	@cp $(LWIPOBJ) .

$(MPDLIB):$(MPDLIB_C)
	$(info [AR]   Creating $@ from $(LIB_OBJS))
	@$(AR) cr $@ $(MPDLIB_OBJS)

# we have to compile these without dietlibc so that there are not
# symbol conflicts and this is why we have the %.a here and don't
# output a .a file.
%.a:%.c
	$(info [CC]   Creating library file from $^)
	$(CC) $(CFLAGS) $(CINC) -o $(@:%.a=%.o) -c $<

# Jumping through hoops: want to link to our libraries first so that
# symbols there are used, thus the gcc, and the ld, but then (and only
# then) do we want to use dietlibc's functions
%.o:%.c
	$(info [CC]   Compiling $<)
	@$(CC) $(CFLAGS) $(CINC) -o $@.$(TMP_STR)1 -c $<
	$(info [LD]   Loading cos libraries into $<)
	@$(LD) -r -o $@.$(TMP_STR)2 $@.$(TMP_STR)1 $(LIB_ASM_MANDITORY) $(LIB_FLAGS)
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
	$(CC) -M $(CFLAGS) $(CINC) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

.PHONY: cp
cp:
	$(info [CP]   Copying all components to $(TRANS_DIR))
	@cp $(COMPONENTS) $(TRANS_DIR)
	make -C sched cp

.PHONY: clean
clean:
	$(info [RM]   Cleaning up directory)
	@rm -f a.out *.o *.a *.d *~
	make -C sched clean
