include ../Makefile.inc
include Makefile.multi

#RK_INC = #Add custom include paths needed for your application
RK_LIBS = paws
RK_SRC = rk_stub.c
RK_APP = rk_stub

RK_RECIPE_OBJ = recipe.o
RK_DEPS ?=

#specify as many stubs for as many apps you've running. in Makefile.multi.
RK_BIN=rkmulti.bin
RK_APP_OUT ?= rk_stub0.out
RK_APP_OBJ ?= rk_stub0.o

.SECONDARY: $(RK_APP_OBJ)
.PHONY:all
all:$(RK_APP).bin $(RK_BIN)

# since we are just compiling we need to manually get the recipe.s file from app-tools
# the app-tools/bake script looks for it.
# This is kinda an out of place command. It uses the host computer's gcc
# TODO, use composite toolchain gcc
$(RK_RECIPE_OBJ):
	$(info | [CC] Creating recipe object file $@)
	@gcc -m32 -O3 -c -o $(RK_RECIPE_OBJ) $(RK_RECIPE)

#For multi-app!!
$(RK_BIN): $(RK_APP_OUT)
	$(info | [BAKE] Baking multiple apps $(RK_APP_OUT) into $@)
	@$(RK_BAKE) $(RK_LIBS) $(RK_BIN) $(RK_APP_OUT)

%.o:%.c
	$(info | [CC] Creating object file $@ from $^)
	@$(RK_CC) $(RK_CFLAGS) -c $(@:%.o=%.c) 

%.out:$(RK_RECIPE_OBJ) %.o
	$(info | [LD] Linking $@ from $^)
	@$(RK_LD) $(RK_LDFLAGS) -o $@ $(RK_RECIPE_OBJ) $(@:%.out=%.o) $(RK_DEPS)

#For a single app!!
$(RK_APP).bin: $(RK_APP).out
	$(info | [BAKE] Baking single app $(RK_APP).out into $@)
	@$(RK_BAKE) $(RK_LIBS) $(RK_APP).bin $(RK_APP).out

$(RK_APP).out : $(RK_APP).o
	$(info | [LD] Linking $@ from $^)
	@$(RK_LD) $(RK_LDFLAGS) -o $(RK_APP).out $(RK_RECIPE_OBJ) $(RK_APP).o $(RK_DEPS)

$(RK_APP).o:$(RK_RECIPE_OBJ) $(RK_SRC)
	$(info | [CC] Creating object file $@ from $^)
	@$(RK_CC) $(RK_CFLAGS) -c $(RK_SRC)

clean :
	rm -f *.o
	rm -f *.out *.bin
	rm -f *.tmp
