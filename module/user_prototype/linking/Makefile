AR=ar
CC=gcc
INC_DIR=../../include
CFLAGS=-Wall -ggdb3 -I$(INC_DIR)
LD=ld

PRODUCTS=cos_loader gen_client_stub

all: $(PRODUCTS)

cos.o: 
#	make -C ..
#	$(LD) -r -o cos.o ../*.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

gen_client_stub: gen_client_stub.o
	$(CC) $(CFLAGS) -o $@ $< 

cos_loader: cos_loader.o #cos.o
	$(CC) $(CFLAGS) -o $@ $^ -L/usr/lib -lbfd -liberty

clean :
	rm -f *~ *.o *.a $(PRODUCTS)
	make -C .. clean
