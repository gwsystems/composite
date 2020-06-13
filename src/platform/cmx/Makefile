include Makefile.src

CC := @arm-none-eabi-gcc
# we use gcc's built-in LD
LD := @arm-none-eabi-gcc

#INCPATH := ../../kernel/include
INCS    := \
-I"../../kernel/include" \
-I"../../kernel/include/shared" \
-I"../../components/include" \
-I"../../components/lib/ps" \
-I"../../components/lib/ps/plat/arch/cmx" \
-I"../../platform/cmx" \
-I"../../platform/cmx/src/extra" \
-I"../../platform/cmx/include/cmsis" \
-I"../../platform/cmx/include/stm32f7-hal"

C_DEFS:= \
-DDEBUG \
-DUSE_FULL_ASSERT \
-DTRACE \
-DSTM32F767xx \
-DUSE_HAL_DRIVER \
-DHSE_VALUE=8000000 

C_OPT   := -O0

CFLAGS:= \
-mcpu=cortex-m7 \
-mthumb \
-mfpu=fpv4-sp-d16 \
-mfloat-abi=softfp \
-mno-unaligned-access \
-Wno-packed-bitfield-compat \
$(C_OPT) \
-fmessage-length=0 \
-fsigned-char \
-ffunction-sections \
-fdata-sections \
-ffreestanding \
-fno-move-loop-invariants \
-g3 \
-std=gnu11 \
-fomit-frame-pointer
# -fPIC \
# -mno-pic-data-is-text-relative \
# -msingle-pic-base \
# -mword-relocations \
# -mpic-register=r9

LDFLAGS := -T frtsections.ld -nostartfiles -Xlinker --gc-sections -L"../src/platform/cmx" -Wl,-Map,"test.map" --specs=nano.specs

KERNEL := kernel.img

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

#CFLAGS += $(WARNINGS)

OBJS += kernel.o
OBJS += vm.o
OBJS += chal.o
OBJS += boot_comp.o
OBJS += chal_pgtbl.o
OBJS += utmem.o

COS_OBJ += pgtbl.o
COS_OBJ += retype_tbl.o
COS_OBJ += liveness_tbl.o
COS_OBJ += tcap.o
COS_OBJ += capinv.o
COS_OBJ += captbl.o

DEPS :=$(patsubst %.o, %.d, $(OBJS))

OBJS += $(COS_OBJ)

all: $(KERNEL)

$(KERNEL): linker.ld $(DEPS) $(OBJS) loader.o
	$(info |     [LD]   Linking $@)
	@$(LD) -T linker.ld loader.o entry.o $(OBJS) -o $@

loader.o: loader.S entry.S
	$(info |     [AS]   Assembling $@)
	@$(CC) -c -I$(INCPATH) entry.S
	@$(CC) -c -I$(INCPATH) loader.S

%.d: %.c
	@$(CC) $(CFLAGS) $(C_DEFS) $(INCS) -MMD -MP -MT $(patsubst %.d, %.o, $@) $(CFLAGS) $< -c -o $@

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
	@$(CC) $(CFLAGS) $(C_DEFS) $(INCS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -c -o "$@" "$<"

clean:
	@rm -f *.d *.o $(KERNEL)

cp: $(KERNEL)
	$(info |     [CP]   Copying native booter to $(TRANS_DIR))
	@cp -f $(KERNEL) .gdbinit *.sh $(TRANS_DIR)
	@cp runscripts/*.sh $(TRANS_DIR)
