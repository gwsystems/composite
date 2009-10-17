# include ../../../Makefile.inc first!

DIETDIR=$(LINKER_DIR)/dietlibc-0.29
DIETBIN=$(DIETDIR)/bin-i386/diet
DIETCC=$(DIETBIN) $(CC)
DIETINC=-I$(DIETDIR)/include

LWIPBASE=$(LINKER_DIR)/lwip
LWIPDIR=$(LWIPBASE)/lwip-1.3.0
LWIPOBJ=$(LWIPBASE)/cos/net_stack.o
LWIPINC=-I$(LWIPDIR)/src/include -I$(LWIPDIR)/src/include/ipv4/ -I$(LWIPBASE)/cos/include/ 

INC_PATH=-I./
# FIXME: dietlibc is now compiled with fno merge constants which is
# probably ballooning its size.  Fix this need for no merge constants
# in cos_loader.
SHARED_FLAGS=-fno-merge-constants -nostdinc -nostdlib
OPT= -ggdb3 #-O3 #-ggdb3
CFLAGS=-Wall -Wextra -Wno-unused-parameter -Wno-unused-function -fno-stack-protector -fno-omit-frame-pointer $(INC_PATH) $(DIETINC) $(LWIPINC) $(OPT) $(SHARED_FLAGS)
DIETCFLAGS=$(CFLAGS) -Xlinker -r
ASFLAGS=$(INC_PATH) $(SHARED_FLAGS)
