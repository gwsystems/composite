
pre: first all
first:
	@echo PRE
	$(info |     [CP]  Copying lua library)
	@cp $(LUAOBJ) .

C_OBJS=ltest.o
ASM_OBJS=
COMPONENT=luatests.o
INTERFACES=
IF_LIB=$(LUAOBJ)
DEPENDENCIES=sched printc

include ../../Makefile.subsubdir
