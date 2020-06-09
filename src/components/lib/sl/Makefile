include Makefile.src Makefile.comp

LIB_OBJS=sl_capmgr.o sl_raw.o sl_sched.o sl_xcpu.o sl_child.o sl_mod_fprr.o sl_lock.o sl_thd_static_backend.o sl_blkpt.o
LIBS=$(LIB_OBJS:%.o=%.a)
CINC+=-m32

.PHONY: all clean
all: $(LIBS)
	@cp *.a ../

%.a:%.c
	$(info |     [CC]   Creating library file $@ from $^)
	@$(CC) $(CFLAGS) $(CINC) -o $(@:%.a=%.o) -c $<
	@$(AR) cr lib$@ $(@:%.a=%.o)

clean:
	@rm -f *.o *.a *.d
