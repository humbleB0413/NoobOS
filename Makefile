# ── Toolchain ──────────────────────────────────────────────────────────────────
CROSS   := i686-elf
CC      := $(CROSS)-gcc
LD      := $(CROSS)-ld
OBJCOPY := $(CROSS)-objcopy

# ── Directories ────────────────────────────────────────────────────────────────
INC_DIR   := include
BUILD_DIR := build

# Add new subsystem directories here (e.g. fs, ipc, arch/x86)
SRC_DIRS  := . boot kernel driver lib mm fs

# ── Build mode ─────────────────────────────────────────────────────────────────
# Usage: make          → debug build
#        make MODE=release → optimised build
MODE ?= debug

CFLAGS_COMMON := \
    -m32 -std=gnu11 \
    -ffreestanding -fno-builtin -fno-stack-protector \
    -Wall -Wextra \
    -I$(INC_DIR) \
    -MMD -MP

ifeq ($(MODE),debug)
    CFLAGS := $(CFLAGS_COMMON) -g3 -O0 -DDEBUG
else ifeq ($(MODE),release)
    CFLAGS := $(CFLAGS_COMMON) -O0 -DNDEBUG
else
    $(error Unknown MODE "$(MODE)". Valid values: debug, release)
endif

# .S files go through CC so the C preprocessor runs (#include, #define, etc.)
ASFLAGS := $(CFLAGS)

LDFLAGS := -T linker.ld -nostdlib
# 64비트 나눗셈(__udivdi3 등) 같은 컴파일러 런타임 헬퍼. -nostdlib 이라 직접 링크해야 한다
LIBGCC  := $(shell $(CC) -print-libgcc-file-name)

OBJ_DIR := $(BUILD_DIR)/$(MODE)
KERNEL  := $(OBJ_DIR)/kernel.elf

# initrd: rootfs/ 내용을 ustar 아카이브로 묶어 Multiboot 모듈로 넘긴다 (QEMU -initrd)
ROOTFS_SRC   := rootfs
ROOTFS_STAGE := $(OBJ_DIR)/rootfs
INITRD       := $(OBJ_DIR)/initrd.tar
ROOTFS_FILES := $(shell find $(ROOTFS_SRC) -type f 2>/dev/null)

# ── Sources & objects ──────────────────────────────────────────────────────────
C_SRCS  := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.c))
S_SRCS  := $(foreach d,$(SRC_DIRS),$(wildcard $(d)/*.S))

C_OBJS  := $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SRCS))
S_OBJS  := $(patsubst %.S,$(OBJ_DIR)/%.o,$(S_SRCS))

# boot/boot.o must come first so the Multiboot header is within the
# first 8 KiB of the binary (linker.ld loads .text at 1 MiB).
BOOT_OBJ   := $(OBJ_DIR)/boot/boot.o
OTHER_OBJS := $(filter-out $(BOOT_OBJ),$(S_OBJS)) $(C_OBJS)
OBJS       := $(BOOT_OBJ) $(OTHER_OBJS)

DEPS := $(OBJS:.o=.d)

# ── QEMU ───────────────────────────────────────────────────────────────────────
QEMU      := qemu-system-i386
QEMUFLAGS := -kernel $(KERNEL) -initrd $(INITRD) -no-reboot

# ── Targets ────────────────────────────────────────────────────────────────────
.PHONY: all run debug-qemu gdb clean info

all: $(KERNEL) $(INITRD)
	@echo "[$(MODE)] $(KERNEL) ready"

$(INITRD): $(ROOTFS_FILES)
	@rm -rf $(ROOTFS_STAGE)
	@mkdir -p $(ROOTFS_STAGE)
	cp -r $(ROOTFS_SRC)/. $(ROOTFS_STAGE)/
	tar --format=ustar --owner=0 --group=0 -cf $@ -C $(ROOTFS_STAGE) .

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS) $(LIBGCC)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(ASFLAGS) -c $< -o $@

# Run kernel in QEMU (serial output to stdio)
run: all
	$(QEMU) $(QEMUFLAGS) -serial stdio

# Start QEMU frozen, waiting for GDB on port 1234
# In a second terminal: make gdb
debug-qemu: all
	@echo "Waiting for GDB on :1234 …  (run 'make gdb' in another terminal)"
	$(QEMU) $(QEMUFLAGS) -s -S -no-shutdown

# Attach GDB to a running debug-qemu session (requires MODE=debug build)
gdb: $(KERNEL)
	gdb \
	    -ex "set architecture i386" \
	    -ex "symbol-file $(KERNEL)" \
	    -ex "target remote :1234"

clean:
	rm -rf $(BUILD_DIR)

# Print resolved variables (useful when diagnosing build issues)
info:
	@echo "MODE     : $(MODE)"
	@echo "KERNEL   : $(KERNEL)"
	@echo "CC       : $(CC)"
	@echo "CFLAGS   : $(CFLAGS)"
	@echo "C_SRCS   : $(C_SRCS)"
	@echo "S_SRCS   : $(S_SRCS)"

# Auto-generated header dependency rules
-include $(DEPS)
