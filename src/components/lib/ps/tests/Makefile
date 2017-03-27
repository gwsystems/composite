CINC    = -I../ -I. -I../plat/
CC      = gcc
LD      = $(CC)
LDFLAGS = -pthread -L..
COPT    = -g
CWARN   = -Wno-long-long
CFLAGS  = -Wall -Wpedantic -Wextra -pthread $(CWARN) $(COPT)

# library
CFILES  = $(wildcard ../*.c)
COBJS   = $(patsubst %.c,%.o,$(CFILES))
CDEPS   = $(patsubst %.c,%.d,$(CFILES))
CNAME   = ps
CLIB    = ../lib$(CNAME).a

# platform
PCFILES = $(wildcard ../plat/*.c)
PCOBJS  = $(patsubst %.c,%.o,$(PCFILES))
PCDEPS  = $(patsubst %.c,%.d,$(PCFILES))

# tests
TCFILES = $(wildcard *.c) # test .c files
TCOBJS  = $(patsubst %.c,%.o,$(TCFILES))
TCDEPS  = $(patsubst %.c,%.d,$(TCFILES))
EXECS   = $(patsubst %.c,%.test,$(TCFILES))

.PHONY: setup setup-linux64 clean all

all: $(EXECS)

setup-linux64:
	cd .. ; rm -f ps_plat.h ; ln -s plat/ps_plat_linux.h ps_plat.h

setup: setup-linux64

%.d:%.c
	@set -e; rm -f $@; \
	$(CC) -M $(CINC) $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

%.o:%.c
	$(CC) -I.. $(CINC) $(CFLAGS) -o $@ -c $<

$(CLIB):$(COBJS) $(PCOBJS)
	$(AR) cr $@ $^

%.test:%.o $(CLIB)
	$(LD) $(LDFLAGS) -o $@ $< -l$(CNAME)

clean:
	rm -f $(EXECS) $(TCOBJS) $(COBJS) $(PCOBJS) $(CLIB) *.d* ../*.d* ../plat/*.d*

-include $(CDEPS) $(TCDEPS) $(PCDEPS)
