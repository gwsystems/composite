include Makefile.src

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
AS := $(CROSS_COMPILE)as
OBJCP := $(CROSS_COMPILE)objcopy

INCPATH := ../../kernel/include
INCS    := -I$(INCPATH) -I$(INCPATH)/shared/ -I$(INCPATH)/chal/
CFLAGS  := -march=armv7-a -DZYNQ_ZC702 -g3 -O3 -ffreestanding -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -nostdinc -nostdlib -fno-pic $(INCS)
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

CFLAGS += $(WARNINGS)

OBJS += kernel.o
OBJS += vm.o
OBJS += printk.o
OBJS += string.o
OBJS += user.o
OBJS += serial.o
OBJS += chal.o
OBJS += boot_comp.o
OBJS += timer.o
OBJS += l2cache.o
OBJS += tlb.o
OBJS += chal_pgtbl.o
OBJS += irq.o
OBJS += vtxprintf.o
#OBJS += div.o

COS_OBJ += pgtbl.o
COS_OBJ += retype_tbl.o
COS_OBJ += liveness_tbl.o
COS_OBJ += tcap.o
COS_OBJ += capinv.o
COS_OBJ += captbl.o

DEPS :=$(patsubst %.o, %.d, $(OBJS))

OBJS += $(COS_OBJ)

all: $(KERNEL)

$(KERNEL): linker.ld $(DEPS) $(OBJS) start.o
	$(if $(CONSTRUCTOR_COMP), $(info |     [LD]   Linking in the constructor component $@))
	$(if $(CONSTRUCTOR_COMP), @cp $(CONSTRUCTOR_COMP) constructor; $(LD) -r -b binary constructor -o constructor.o; objcopy --rename-section .data=.initial_component constructor.o; rm constructor)
	$(if $(KERNEL_OUTPUT), $(info |     [LD]   Linking $@))
	$(if $(KERNEL_OUTPUT), $(LD) -T linker.ld start.o $(OBJS) $(if $(CONSTRUCTOR_COMP), constructor.o) -o $@)
	$(if $(KERNEL_OUTPUT), $(info |     [CP]   Copying $@ into $(KERNEL_OUTPUT)))
	$(if $(KERNEL_OUTPUT), @cp $@ $(KERNEL_OUTPUT))

start.o: start.S 
	$(info |     [AS]   Assembling $@)
	@$(AS) -c start.S -o $@


%.d: %.c
	@$(CC) -M -MT $(patsubst %.d, %.o, $@) $(CFLAGS) $(LDFLAGS) $< -o $@

pgtbl.o: ../../kernel/pgtbl.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

tcap.o: ../../kernel/tcap.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

retype_tbl.o: ../../kernel/retype_tbl.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

liveness_tbl.o: ../../kernel/liveness_tbl.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

capinv.o: ../../kernel/capinv.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

captbl.o: ../../kernel/captbl.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@


%.o: %.c
	$(info |     [CC]   Compiling $@)
	@$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -f *.d *.o $(KERNEL)
