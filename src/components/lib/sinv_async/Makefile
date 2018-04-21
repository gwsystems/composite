include Makefile.src Makefile.comp

LIB_OBJS=sinv_client.o acom_client.o sinv_server.o
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
