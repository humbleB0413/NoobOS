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

# ── User programs (ring 3) ─────────────────────────────────────────────────────
# user/bin/<name>.c 하나가 /bin/<name> 실행 파일 하나. 커널과 별개로 user/linker.ld 로 링크한다
USER_CFLAGS := -m32 -std=gnu11 -ffreestanding -fno-builtin -fno-stack-protector \
               -fno-pie -Wall -Wextra -O2 -g -Iinclude -Iuser/lib -MMD -MP
USER_LIB    := $(OBJ_DIR)/user/lib/crt0.o $(OBJ_DIR)/user/lib/ulib.o
USER_PROGS  := $(patsubst user/bin/%.c,%,$(wildcard user/bin/*.c))
USER_BINS   := $(addprefix $(OBJ_DIR)/user/bin/,$(USER_PROGS))
DEPS_USER   := $(USER_LIB:.o=.d) $(addsuffix .d,$(USER_BINS))

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
# make run DISK=disk.img  → primary master ATA 디스크로 붙인다 (셸의 disk 명령)
ifneq ($(DISK),)
QEMUFLAGS += -drive file=$(DISK),format=raw,if=ide,index=0
endif

# ── Targets ────────────────────────────────────────────────────────────────────
.PHONY: all run debug-qemu gdb test clean info

all: $(KERNEL) $(INITRD)
	@echo "[$(MODE)] $(KERNEL) ready"

$(OBJ_DIR)/user/%.o: user/%.c
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(OBJ_DIR)/user/%.o: user/%.S
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(OBJ_DIR)/user/bin/%: $(OBJ_DIR)/user/bin/%.o $(USER_LIB) user/linker.ld
	$(LD) -T user/linker.ld -nostdlib -o $@ $(USER_LIB) $< $(LIBGCC)

$(INITRD): $(ROOTFS_FILES) $(USER_BINS)
	@rm -rf $(ROOTFS_STAGE)
	@mkdir -p $(ROOTFS_STAGE)/bin
	cp -r $(ROOTFS_SRC)/. $(ROOTFS_STAGE)/
	@for b in $(USER_BINS); do $(OBJCOPY) --strip-debug $$b $(ROOTFS_STAGE)/bin/$$(basename $$b); done
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
	    -ex "target remote :1234" \
	    -x tools/gdbinit

# QEMU 를 화면 없이 띄워 시리얼로 셸을 조작하는 자동 스모크 테스트 (selftest 명령이 있는 debug 빌드 필요)
test:
	$(MAKE) MODE=debug all
	python3 tools/smoke_test.py $(BUILD_DIR)/debug/kernel.elf $(BUILD_DIR)/debug/initrd.tar

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
	@echo "USER     : $(USER_PROGS)"

# Auto-generated header dependency rules
-include $(DEPS) $(DEPS_USER)
