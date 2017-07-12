include Makefile.src Makefile.comp

LIB_OBJS=sl.o sl_mod_fprr.o sl_lock.o sl_thd_static_backend.o
LIBS=$(LIB_OBJS:%.o=%.a)

.PHONY: all clean
all: $(LIBS)
	@cp *.a ../

%.a:%.c
	$(info |     [CC]   Creating library file $@ from $^)
	@$(CC) $(CFLAGS) $(CINC) -o $(@:%.a=%.o) -c $<
	@$(AR) cr lib$@ $(@:%.a=%.o)

clean:
	@rm -f *.o *.a *.d
