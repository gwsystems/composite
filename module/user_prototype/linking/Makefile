include ../../Makefile.inc

INC_DIR=../../include
CFLAGS=-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -ggdb3 -I$(INC_DIR)
PRODUCTS=cos_loader gen_client_stub

all: $(PRODUCTS)
	make -C t
cos.o: 
#	make -C ..
#	$(LD) -r -o cos.o ../*.o

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $^

gen_client_stub: gen_client_stub.o
	$(CC) $(CFLAGS) -o $@ $< 

cos_loader: cos_loader.o #cos.o
	$(CC) $(CFLAGS) -o $@ $^ -L/usr/lib -lbfd -liberty

copy:
	cp cos_loader gen_client_stub $(TRANS_DIR)
	make -C t copy

clean :
	rm -f *~ *.o *.a $(PRODUCTS)
	make -C t clean
