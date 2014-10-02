include Makefile.src

CC := gcc -m32
LD := ld -m elf_i386
AS := as --32 -g

CFLAGS := -g3 -Werror -ffreestanding -Wno-unused -nostdlib -mno-red-zone -I../../kernel/include
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
OBJS += vm.o
OBJS += printk.o
OBJS += string.o
OBJS += vtxprintf.o
OBJS += tss.o
OBJS += user.o
OBJS += serial.o
OBJS += timer.o
#OBJS += console.o

all: $(KERNEL)

DEPS :=$(patsubst %.o, %.d, $(OBJS))

$(KERNEL): $(DEPS) $(OBJS) loader.o
	$(LD) -T linker.ld loader.o $(OBJS)  -o $@

loader.o: loader.s interrupt.s sysenter.s
	$(AS) -o loader.o loader.s interrupt.s sysenter.s

%.d: %.c
	$(CC) -M -MT $(patsubst %.d, %.o, $@) $(CFLAGS) $(LDFLAGS) $< -o $@

clean:
	rm -f *.d *.o $(KERNEL)

cp: $(KERNEL)
	cp -f $(KERNEL) .gdbinit qemu.sh qemu-g.sh $(TRANS_DIR)
