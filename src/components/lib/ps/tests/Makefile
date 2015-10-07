CINC    = -I../
CC      = gcc
LD      = gcc
LDFLAGS =
COPT    = -O2
CWARN   = -Wno-long-long
CFLAGS  = -Wall -Wpedantic -Wextra $(CWARN) $(COPT)
CFILES  = $(wildcard *.c) $(wildcard ../*.c)
COBJS   = $(patsubst %.c,%.o,$(CFILES))
CDEPS   = $(patsubst %.c,%.d,$(CFILES))

EXEC    = slab_test

all: $(CDEPS) $(EXEC)

%.o:%.c
	$(CC) $(CINC) $(CFLAGS) -o $@ -c $<

%.d:%.c
	$(info Creating dependency files: $(CDEPS))
	@set -e; rm -f $@; \
	$(CC) -M $(CINC) $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

$(EXEC): $(COBJS)
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(EXEC) $(COBJS) $(CDEPS)

-include $(CDEPS)
