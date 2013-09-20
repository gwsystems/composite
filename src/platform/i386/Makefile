ifeq ($(strip $(V)),)
	E= @echo
	Q= @
else
	E = @\#
	Q =
endif

CC := gcc -m32 
LD := ld -m elf_i386
AS := as --32 -g

CFLAGS := -g3 -Werror -ffreestanding -nostdlib -mno-red-zone
LDFLAGS := -nostdlib -fno-builtin -nostartfiles -nostdinc -nodefaultlibs 

KERNEL := kernel.img

WARNINGS += -Wall
WARNINGS += -Wcast-align
WARNINGS += -Wformat=2
WARNINGS += -Winit-self
WARNINGS += -Wmissing-declarations
WARNINGS += -Wmissing-prototypes
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
OBJS += gdt.o
OBJS += isr.o
OBJS += idt.o
OBJS += mm.o
OBJS += vm.o
OBJS += vga.o
OBJS += timer.o
OBJS += printk.o
OBJS += kbd.o
OBJS += string.o
OBJS += serial.o
OBJS += ports.o
OBJS += vtxprintf.o
OBJS += boot_log.o
OBJS += multiboot.o
#OBJS += user.o

all: $(KERNEL)
	$(E) "--[ Kernel Built"

DEPS :=$(patsubst %.o, %.d, $(OBJS))

$(KERNEL): $(DEPS) $(OBJS) loader.o
	$(E) "    LINK 	" $@
	$(Q) $(LD) -T linker.ld loader.o $(OBJS)  -o $@

loader.o:
	$(E) "    AS  	" $@
	$(Q) $(AS) -o loader.o loader.s gdt.s interrupt.s user.s sysenter.s

$(DEPS):

%.d: %.c
	$(Q) $(CC) -M -MT $(patsubst %.d, %.o, $@) $(CFLAGS) $(LDFLAGS) $< -o $@

$(OBJS):

%.o: %.c
	$(E) "    CC 		" $@
	$(Q) $(CC) -c $(CFLAGS) $(LDFLAGS) $< -o $@


floppy: $(KERNEL)

clean:
	$(Q) rm -f $(DEPS) $(OBJS) loader.o $(KERNEL)
	$(E) "--[ Clean"

qemu: kernel.img
	qemu-system-i386 -nographic -kernel kernel.img -no-reboot -s 

gdb: kernel.img
	@echo "Start gdb in another window"
	qemu-system-i386 -nographic -kernel kernel.img -no-reboot -s -S
