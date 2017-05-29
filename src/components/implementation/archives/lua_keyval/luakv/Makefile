
pre: first all
first:
	@echo PRE
	$(info |     [CP]  Copying lua library)
	@cp $(LUAOBJ) .

C_OBJS=lkv.o
ASM_OBJS=
COMPONENT=lua_keyval.o
INTERFACES=lua_keyval
IF_LIB=$(LUAOBJ)
DEPENDENCIES=printc

include ../../Makefile.subsubdir
