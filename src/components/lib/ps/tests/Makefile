CINC    = -I../ -I.
CC      = gcc
LD      = $(CC)
LDFLAGS = -pthread
COPT    = -O3
CWARN   = -Wno-long-long
CFLAGS  = -Wall -Wpedantic -Wextra -pthread $(CWARN) $(COPT)

# library
CFILES  = $(wildcard ../*.c)
COBJS   = $(patsubst %.c,%.o,$(CFILES))
CDEPS   = $(patsubst %.c,%.d,$(CFILES))

# tests
TCFILES = $(wildcard *.c) # test .c files
TCOBJS  = $(patsubst %.c,%.o,$(TCFILES))
TCDEPS  = $(patsubst %.c,%.d,$(TCFILES))
EXECS   = $(patsubst %.c,%.test,$(TCFILES))

.PHONY: all setup setup-linux64 clean

all: $(CDEPS) $(TCDEPS) $(EXECS)

setup-linux64:
	cd .. ; rm ps_plat.h ; ln -s ps_plat_linux.h ps_plat.h

setup: setup-linux64

%.d:%.c
	@set -e; rm -f $@; \
	$(CC) -M $(CINC) $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o:%.c
	$(CC) -I.. $(CINC) $(CFLAGS) -o $@ -c $<

%.test:%.o $(COBJS)
	$(LD) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(EXECS) $(TCOBJS) $(TCDEPS) $(COBJS) $(CDEPS) *.d* ../*.d*

-include $(CDEPS) $(TCDEPS)
