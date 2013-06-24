CC=gcc
AR=ar
JSMN_PATH=../jsmn
JSMN_OBJ=$(JSMN_PATH)/jsmn.o
CFLAGS=-D__x86__ -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -Wno-unused-variable -fno-stack-protector -fno-omit-frame-pointer -O3 -fno-merge-constants 
#-nostdinc -nostdlib
CINC=-I$(JSMN_PATH)/ -I../
TEST_OBJ=sconf_test.o
TEST=sconf_test
SCONF_OBJS=$(filter-out $(TEST_OBJ),$(patsubst %.c,%.o,$(wildcard *.c)))
LIB=sconf.a

all: test $(LIB)
	cp lib$(LIB) ../

test: $(TEST)

$(TEST): $(TEST_OBJ) $(SCONF_OBJS)
	$(CC) $^ $(JSMN_OBJ) -o $@

%.a: $(SCONF_OBJS)
	$(AR) cr lib$@ $^ $(JSMN_OBJ)

%.o:%.c
	$(CC) $(CFLAGS) $(CINC) -o $@ -c $^

clean:
	rm -f a.out *.o *.a *.d *~ $(TEST)
