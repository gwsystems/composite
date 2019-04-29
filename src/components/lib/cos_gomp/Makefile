include Makefile.src Makefile.comp

OBJS=cos_gomp.o
LIB=cos_gomp
CINC+=-m32

.PHONY: all clean
all: $(LIB)
	@cp *.a ../

%.o:%.c
	$(info |     [CC]   Compiling C file $^ into $@)
	@$(CC) $(CFLAGS) $(CINC) -o $@ -c $<

$(LIB): $(OBJS)
	$(info |     [LD]   Creating library file lib$(LIB).a)
	@$(AR) cr lib$(LIB).a $^

clean:
	@rm -f *.o *.a *.d
