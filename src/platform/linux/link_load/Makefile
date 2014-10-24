include Makefile.src

CFLAGS=-D__x86__ -D_GNU_SOURCE -lpthread -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -ggdb3 -I$(KINC) 
PRODUCTS=cos_loader gen_client_stub

default: $(PRODUCTS)

%.o: %.c
	$(CC) $(CFLAGS) -I$(CDIR)/include/ -c -o $@ $^ -I$(SHAREDINC)

gen_client_stub: gen_client_stub.o
	$(CC) $(CFLAGS) -o $@ $< 

cos_loader: cos_loader.o
	$(CC) $(CFLAGS) -o $@ $^ -L/usr/lib -lbfd -liberty -L$(CDIR)/lib/ -lcobj_format

cp:
	cp cos_loader gen_client_stub $(TRANS_DIR)

clean:
	rm -f *~ *.o *.a $(PRODUCTS)

fresh: clean default
