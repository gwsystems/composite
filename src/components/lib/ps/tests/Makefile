CINC    = -I../ -I.
CC      = gcc
LD      = gcc
LDFLAGS =
COPT    = -O2
CWARN   = -Wno-long-long
CFLAGS  = -Wall -Wpedantic -Wextra $(CWARN) $(COPT)

CFILES  = $(wildcard ../*.c)
COBJS   = $(patsubst %.c,%.o,$(CFILES))
CDEPS   = $(patsubst %.c,%.d,$(CFILES))

TCFILES = $(wildcard *.c) # test .c files
TCOBJS  = $(patsubst %.c,%.o,$(TCFILES))
TCDEPS  = $(patsubst %.c,%.d,$(TCFILES))
EXECS   = $(patsubst %.c,%.test,$(TCFILES))

all: $(CDEPS) $(TCDEPS) $(EXECS)

setup:
	cd .. ; ln -s ps_plat_linux.h ps_plat.h

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
	rm -f $(EXECS) $(TCOBJS) $(TCDEPS) $(COBJS) $(CDEPS) *.d*

-include $(CDEPS) $(TCDEPS)
