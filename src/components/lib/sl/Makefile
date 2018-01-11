include Makefile.src Makefile.comp

LIB_OBJS=sl.o sl_mod_fprr.o sl_lock.o sl_thd_static_backend.o
LIBS=$(LIB_OBJS:%.o=%.a)
CINC+=-m32

.PHONY: all clean
all: $(LIBS)
	@cp *.a ../
	$(CC) $(INC_PATH) $(CINC) -o sl_bind_check sl_bind_check.c

%.a:%.c
	$(info |     [CC]   Creating library file $@ from $^)
	@$(CC) $(CFLAGS) $(CINC) -o $(@:%.a=%.o) -c $<
	@$(AR) cr lib$@ $(@:%.a=%.o)

clean:
	@rm -f *.o *.a *.d
