CC=gcc
AR=ar
LD=ld
AS=$(CC)
INC_PATH=./include/
CFLAGS=-Wall -I$(INC_PATH) -ggdb3
ASFLAGS=-I./include/ -c
PRODUCTS=test
FILES=spd.o thread.o ipc.o kern_ipc.o #ST_ipc.o main.o

objs: $(FILES)

all: $(PRODUCTS)

%.a:%.o
	$(AR) r $@ $^

%.o:%.c
	$(CC) $(CFLAGS) -o $@ -c $^

%.o:%.S
	$(AS) $(ASFLAGS) -o $@ $^

test: $(FILES)
	$(CC) $(CFLAGS) -o $@ $^
# -L. -lipc

clean:
	rm -rf *.o *.a $(PRODUCTS) *~



