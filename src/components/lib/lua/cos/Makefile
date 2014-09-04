include ../../../../Makefile.src
include ../../../Makefile.comp
$(info $(CINC))

# This is the name of the output library.
#
OUTPUT_OBJ=lua_lang.o

CCDEP=gcc
CC=gcc

OPT=-O3 #-ggdb3
CFLAGS=-m32 -Wall -DIPv4 -nostdinc -nostdlib $(OPT)
LDFLAGS=-m elf_i386

#Set this to where you have the lua and the dietlibc library files installed
LUADIR=../lua-5.2.1/src
DIET=../../dietlibc-0.33
DIETINC=$(DIET)/include
DIETI386=$(DIET)/bin-i386

INCLUDES=-I$(LUADIR)/include -I./include $(CINC) -I$(DIETINC) -I$(DIET)
CFLAGS:=$(CFLAGS) $(INCLUDES)

COREFILES=$(LUADIR)/lapi.c \
$(LUADIR)/lcode.c    \
$(LUADIR)/lctype.c   \
$(LUADIR)/ldebug.c   \
$(LUADIR)/ldo.c      \
$(LUADIR)/ldump.c    \
$(LUADIR)/lfunc.c    \
$(LUADIR)/lgc.c      \
$(LUADIR)/llex.c     \
$(LUADIR)/lmem.c     \
$(LUADIR)/lobject.c  \
$(LUADIR)/lopcodes.c \
$(LUADIR)/lparser.c  \
$(LUADIR)/lstate.c   \
$(LUADIR)/lstring.c  \
$(LUADIR)/ltable.c   \
$(LUADIR)/ltm.c      \
$(LUADIR)/lundump.c  \
$(LUADIR)/lvm.c      \
$(LUADIR)/lzio.c     \
$(LUADIR)/lauxlib.c  \
$(LUADIR)/lbaselib.c \
$(LUADIR)/lbitlib.c  \
$(LUADIR)/lcorolib.c \
$(LUADIR)/ldblib.c   \
$(LUADIR)/liolib.c   \
$(LUADIR)/lmathlib.c \
$(LUADIR)/loslib.c   \
$(LUADIR)/lstrlib.c  \
$(LUADIR)/ltablib.c  \
$(LUADIR)/loadlib.c  \
$(LUADIR)/linit.c    

DIETLIBCA=$(DIETI386)/dietlibc.a
DIETMATH=$(DIETI386)/libm.a
DIETMIPS=$(DIET)/mips/divdi3.c

LUAFILES=$(COREFILES) $(DIETMIPS)
LUAFILESW=$(wildcard $(LUAFILES))
LUAOBJS=$(notdir $(LUAFILESW:.c=.o)) $(DIETLIBCA) $(DIETMATH)



all: .depend $(OUTPUT_OBJ)
.PHONY: all

clean:
	rm -f *.o $(OUTPUT_OBJ) .depend* 

depend dep: .depend

include .depend

.depend: $(LUAFILES) 
	$(CCDEP) $(CFLAGS) -MM $^ > .depend || rm -f .depend

$(OUTPUT_OBJ): $(LUAOBJS)
	ld $(LDFLAGS) -r -o $@ $^

%.o:
	$(CC) $(CFLAGS) -c $(<:.o=.c) 
