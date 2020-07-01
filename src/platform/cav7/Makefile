CC := arm-none-eabi-gcc -O3 -mcpu=cortex-a9 -march=armv7-a
# -target armv7a-unknown-none-elf
# CC := clang -c -target armv7a-unknown-none-elf -mtune=cortex-a9
# LD := ld.lld -v -m armelf
LD := arm-none-eabi-ld
AS := arm-none-eabi-gcc -O3 -mcpu=cortex-a9
# -fuse-ld=lld -target armv7a-unknown-none-elf -mtune=cortex-a9
OBJCP := arm-none-eabi-objcopy

INCPATH := ../../kernel/include
INCS    := -I$(INCPATH)
CFLAGS  := -g3 -O3 -ffreestanding -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -fno-pic $(INCS)
LDFLAGS := -nostdlib -nostdinc

KERNEL := kernel.bin

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
#OBJS += gdt.o
#OBJS += idt.o
OBJS += vm.o
OBJS += printk.o
OBJS += string.o
#OBJS += vtxprintf.o
##OBJS += tss.o
OBJS += user.o
OBJS += serial.o
#OBJS += hpet.o
OBJS += chal.o
OBJS += boot_comp.o
#OBJS += miniacpi.o
#OBJS += console.o
#OBJS += vga.o
#OBJS += exception.o
OBJS += lapic.o
OBJS += chal_pgtbl.o
OBJS += irq.o
# OBJS += start.o
OBJS += vtxprintf.o 
OBSJ += div.o

COS_OBJ += pgtbl.o
COS_OBJ += retype_tbl.o
COS_OBJ += liveness_tbl.o
COS_OBJ += tcap.o
COS_OBJ += capinv.o
COS_OBJ += captbl.o

DEPS :=$(patsubst %.o, %.d, $(OBJS))

OBJS += $(COS_OBJ)

all: $(KERNEL)

start.o: start.S
	$(AS) -c start.S -o $@

$(KERNEL): linker.ld $(DEPS) $(OBJS) start.o
	$(info |     [LD]   Linking $@)
	@$(LD) -T linker.ld $(OBJS) start.o -o $(patsubst %.bin, %.elf, $@)
	@$(OBJCP) -O binary $(patsubst %.bin, %.elf, $@) $@

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
	@rm -f *.d *.o *.elf *.bin

cp: $(KERNEL)
	$(info |     [CP]   Copying native booter to $(TRANS_DIR))
	@cp -f $(KERNEL) .gdbinit *.sh $(TRANS_DIR)
	@cp runscripts/*.sh $(TRANS_DIR)
