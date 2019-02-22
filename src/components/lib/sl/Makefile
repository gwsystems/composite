include Makefile.src Makefile.comp

LIB_OBJS=sl_capmgr.o sl_raw.o sl_sched.o sl_xcpu.o sl_child.o sl_mod_fprr.o sl_mod_rr.o sl_lock.o sl_thd_static_backend.o
LIBS=$(LIB_OBJS:%.o=%.a)
CINC+=-m32

.PHONY: all clean
all: $(LIBS)
	@cp *.a ../

%.a:%.c
	$(info |     [CC]   Creating library file $@ from $^)
	@$(CC) $(CFLAGS) $(CINC) -o $(@:%.a=%.o) -c $<
	@$(AR) cr lib$@ $(@:%.a=%.o)

%.a:%.S
	$(info |     [AS] Creating library file $@ from $^)
	@$(AS) $(ASFLAGS) -c -o $(@:%.a=%.o) $^
	@$(AR) cr lib$@ $(@:%.a=%.o)

clean:
	@rm -f *.o *.a *.d
