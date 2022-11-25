include Makefile.src
include Makefile.arch

CC := gcc -m64
LD := ld -m elf_x86_64 --nmagic
AS := as --64 -g

INCPATH := ../../kernel/include
INCS    := -I$(INCPATH) -I$(INCPATH)/shared/ -I$(INCPATH)/chal/
CFLAGS  := $(ARCH_CFLAGS) -g3 -O3 -ffreestanding -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -nostdinc -nostdlib -fno-pic -mno-red-zone -mcmodel=large -mno-sse -mno-sse2 -mgeneral-regs-only $(INCS)
LDFLAGS := -nostdlib -fno-builtin -nostartfiles -nostdinc -nodefaultlibs

KERNEL := kernel.img
IMAGE  := cos.img

WARNINGS += -Wall
WARNINGS += -Wcast-align
WARNINGS += -Wformat=2
WARNINGS += -Winit-self
#WARNINGS += -Wmissing-declarations
#WARNINGS += -Wmissing-prototypes
WARNINGS += -Wnested-externs
WARNINGS += -Wno-system-headers
WARNINGS += -Wold-style-definition
WARNINGS += -Wredundant-decls
WARNINGS += -Wsign-compare
WARNINGS += -Wstrict-prototypes
WARNINGS += -Wundef
WARNINGS += -Wvolatile-register-var
WARNINGS += -Wwrite-strings
WARNINGS += -Wno-address-of-packed-member

CFLAGS += $(WARNINGS)

KERNEL_CFILES += kernel.c
KERNEL_CFILES += gdt.c
KERNEL_CFILES += idt.c
KERNEL_CFILES += vm.c
KERNEL_CFILES += printk.c
KERNEL_CFILES += string.c
KERNEL_CFILES += vtxprintf.c
KERNEL_CFILES += tss.c
KERNEL_CFILES += user.c
KERNEL_CFILES += serial.c
KERNEL_CFILES += hpet.c
KERNEL_CFILES += chal.c
KERNEL_CFILES += boot_comp.c
KERNEL_CFILES += miniacpi.c
KERNEL_CFILES += exception.c
KERNEL_CFILES += lapic.c
KERNEL_CFILES += chal_pgtbl.c
KERNEL_CFILES += fpu.c
KERNEL_CFILES += ulinv.c

OBJS := $(KERNEL_CFILES:%.c=%.o)
DEPS := $(KERNEL_CFILES:%.c=%.d)

# COS KERNEL_CFILES
COS_CFILES += ../../kernel/pgtbl.c
COS_CFILES += ../../kernel/retype_tbl.c
COS_CFILES += ../../kernel/liveness_tbl.c
COS_CFILES += ../../kernel/tcap.c
COS_CFILES += ../../kernel/capinv.c
COS_CFILES += ../../kernel/captbl.c

OBJS += $(COS_CFILES:../../kernel/%.c=%.o)
DEPS += $(COS_CFILES:../../kernel/%.c=%.d)

.PHONY: $(CONSTRUCTOR_COMP)

all: $(KERNEL)

$(KERNEL): linker.ld $(OBJS) loader.o $(CONSTRUCTOR_COMP)
	$(if $(CONSTRUCTOR_COMP), $(info |     [LD]   Linking in the constructor component $@))
	$(if $(CONSTRUCTOR_COMP), cp $(CONSTRUCTOR_COMP) constructor; $(LD) -r -b binary constructor -o constructor.o; objcopy --rename-section .data=.initial_component constructor.o;)
	$(if $(KERNEL_OUTPUT), $(info |     [LD]   Linking $@))
	$(if $(KERNEL_OUTPUT), $(LD) -T linker.ld loader.o entry.o $(OBJS) $(if $(CONSTRUCTOR_COMP), constructor.o) -o $@)
	$(if $(KERNEL_OUTPUT), $(info |     [CP]   Copying $@ into $(KERNEL_OUTPUT)))
	$(if $(KERNEL_OUTPUT), cp $@ $(KERNEL_OUTPUT))

loader.o: loader.S entry.S
	$(info |     [AS]   Assembling $@)
	@$(CC) -c -I$(INCPATH) entry.S
	@$(CC) -c -I$(INCPATH) loader.S

define COMPILE_OBJ_RULE
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@
endef

define COMPILE_DEP_RULE
	@set +e; rm -f $@ $@.*; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$ 2>/dev/null ; \
	if [ -s $@.$$$$ ]; then \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	fi; \
	rm -f $@.$$$$
endef

%.o:../../kernel/%.c ; $(COMPILE_OBJ_RULE)
%.o: %.c ; $(COMPILE_OBJ_RULE)

ulinv.o: ../../kernel/ulinv.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@


%.d:../../kernel/%.c ; $(COMPILE_DEP_RULE)
%.d:%.c ; $(COMPILE_DEP_RULE)

clean:
	@rm -f $(OBJS) $(DEPS) *.o $(KERNEL) kernel.iso
	@rm -rf iso
	@rm -f constructor

gdb:
	gdb -x ./gdbinit

-include $(DEPS)
