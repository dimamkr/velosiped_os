# ============================================================
#  Makefile — 32-битное ядро (рекурсивная структура src/)
# ============================================================
#
#  Цели:
#    all          релизная сборка + образ
#    run          собрать и запустить QEMU
#    build-debug  сборка с отладочной информацией
#    run-debug    QEMU с GDB-сервером
#    clean        удалить build/
#
#  Любые .c/.asm под src/ (кроме src/acpica и src/boot) собираются
#  рекурсивно с сохранением структуры каталогов в build/kernel/.
# ============================================================

# ------- Инструменты -------
CC      = gcc
NASM    = nasm
LD      = ld
OBJCOPY = objcopy
RM      = rm -rf

# libgcc для 32-битных рантайм-хелперов (__udivdi3, __udivmoddi4 и т.п.)
LIBGCC  = $(shell $(CC) -m32 -print-libgcc-file-name)
# внутренние заголовки GCC (stddef.h, stdarg.h, stdbool.h, ...)
GCC_INCLUDE_DIR = $(shell $(CC) -print-file-name=include)

# ------- Каталоги исходников -------
SRC_DIR        = src
BOOT_SRC_DIR   = $(SRC_DIR)/boot
ACPICA_DIR     = $(SRC_DIR)/third_party/acpica
ACPICA_INCLUDE = $(ACPICA_DIR)/include
LINKER_SCRIPT  = $(SRC_DIR)/arch/link.ld

USER_DIR       = userspace
USER_BIN_DIR   = $(USER_DIR)/bin
USER_INC_DIR   = $(USER_DIR)/include
USER_LIB_DIR   = $(USER_DIR)/lib
USER_LINKER_SCRIPT = $(USER_DIR)/link.ld



TEST_FILES_DIR = test/files

# ============================================================
#  Режим сборки
# ============================================================

ifeq ($(origin BUILD_MODE), undefined)
  ifneq ($(filter debug build-debug run-debug gdb,$(MAKECMDGOALS)),)
    BUILD_MODE := debug
  else
    BUILD_MODE := release
  endif
endif

BUILD_DIR_ROOT   = build
BUILD_DIR        = $(BUILD_DIR_ROOT)/$(BUILD_MODE)
BUILD_BOOT_DIR   = $(BUILD_DIR)/boot
BUILD_KERNEL_DIR = $(BUILD_DIR)/src
BUILD_ACPI_DIR   = $(BUILD_KERNEL_DIR)/third_party/acpica
BUILD_USER_BIN   = $(BUILD_DIR)/userspace/bin
BUILD_USER_LIB =  $(BUILD_DIR)/userspace/lib
BUILD_LOGS_DIR   = $(BUILD_DIR)/LOGS

# ---- Picolibc ----
PICOLIBC_DIR         = third_party/picolibc
PICOLIBC_CROSS       = $(PICOLIBC_DIR)/velosipedOS/cross-velosipedOS.txt
BUILD_PICOLIBC_DIR   = $(BUILD_DIR)/third_party/picolibc

PICOLIBC_INSTALL_DIR = $(BUILD_PICOLIBC_DIR)/install
PICOLIBC_INCLUDE_DIR = $(PICOLIBC_INSTALL_DIR)/include
PICOLIBC_LIB_DIR     = $(PICOLIBC_INSTALL_DIR)/lib
PICOLIBC_LIBC        = $(PICOLIBC_LIB_DIR)/libc.a
PICOLIBC_LIBM        = $(PICOLIBC_LIB_DIR)/libm.a

# ============================================================
#  Автоматический сбор include-путей
# ============================================================
# Все подкаталоги src/, кроме acpica и boot, добавляются как -I.
# Это позволяет писать #include "konsole.h" без пути,
# даже если konsole.h лежит в src/gui/konsole/.

KERNEL_SRC_DIRS := $(shell find $(SRC_DIR) -type d \
                       -not -path '$(ACPICA_DIR)*' \
                       -not -path '$(BOOT_SRC_DIR)*')

KERNEL_INCLUDES := $(addprefix -I,$(KERNEL_SRC_DIRS))

# ============================================================
#  Флаги
# ============================================================

KERNEL_CFLAGS_COMMON = -m32 -std=gnu11 -ffreestanding -nostdlib -fno-builtin \
                       -fno-stack-protector -fno-pic -mgeneral-regs-only \
                       -fno-strict-aliasing \
                       $(KERNEL_INCLUDES) \
                       -Werror -MMD -MP
KERNEL_CFLAGS_RELEASE = $(KERNEL_CFLAGS_COMMON) -O2
KERNEL_CFLAGS_DEBUG   = $(KERNEL_CFLAGS_COMMON) -g -O0 -fno-omit-frame-pointer

KERNEL_NASM_COMMON = -f elf32
KERNEL_NASM_RELEASE = $(KERNEL_NASM_COMMON)
KERNEL_NASM_DEBUG   = $(KERNEL_NASM_COMMON) -g

KERNEL_LDFLAGS = -m elf_i386 -T $(LINKER_SCRIPT) -nostdlib

# ---- ACPICA ----
ACPICA_CFLAGS  = -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-sign-compare
ACPICA_CFLAGS += -ffreestanding -nostdlib -fno-builtin
ACPICA_CFLAGS += -I$(ACPICA_INCLUDE) -I$(ACPICA_DIR)/components

# ---- Пользовательские программы ----
USER_CFLAGS_COMMON = -m32 -std=gnu11 -ffreestanding -nostdinc -nostdlib -nostartfiles \
                     -nodefaultlibs -static -no-pie -fno-pic -fno-builtin \
                     -fno-stack-protector -mgeneral-regs-only \
                     -I$(PICOLIBC_INCLUDE_DIR) \
                     -I$(USER_INC_DIR) \
                     -isystem $(GCC_INCLUDE_DIR) \
                     -Wall -Wextra -Werror -MMD -MP
USER_CFLAGS_RELEASE = $(USER_CFLAGS_COMMON) -O2
USER_CFLAGS_DEBUG   = $(USER_CFLAGS_COMMON) -g -O0 -fno-omit-frame-pointer

USER_NASM = -f elf32

USER_LDFLAGS = -m elf_i386 -T $(USER_LINKER_SCRIPT) -nostdlib

# объявление флагов для текущей сборки
ifeq ($(BUILD_MODE),debug)
  KERNEL_CFLAGS = $(KERNEL_CFLAGS_DEBUG)
  KERNEL_NASM   = $(KERNEL_NASM_DEBUG)
  USER_CFLAGS   = $(USER_CFLAGS_DEBUG)
else
  KERNEL_CFLAGS = $(KERNEL_CFLAGS_RELEASE)
  KERNEL_NASM   = $(KERNEL_NASM_RELEASE)
  USER_CFLAGS   = $(USER_CFLAGS_RELEASE)
endif

# ============================================================
#  Списки источников и целей
# ============================================================

# ---- Ядро: рекурсивный обход src/, кроме acpica и boot ----
KERNEL_C_SOURCES := $(shell find $(SRC_DIR) -name '*.c' \
                       -not -path '$(ACPICA_DIR)/*' \
                       -not -path '$(BOOT_SRC_DIR)/*')

KERNEL_ASM_SOURCES := $(shell find $(SRC_DIR) -name '*.asm' \
                         -not -path '$(ACPICA_DIR)/*' \
                         -not -path '$(BOOT_SRC_DIR)/*')

# Зеркалируем относительные пути в build/kernel/
KERNEL_C_OBJECTS   := $(patsubst $(SRC_DIR)/%.c,   $(BUILD_KERNEL_DIR)/%.o, $(KERNEL_C_SOURCES))
KERNEL_ASM_OBJECTS := $(patsubst $(SRC_DIR)/%.asm, $(BUILD_KERNEL_DIR)/%.o, $(KERNEL_ASM_SOURCES))

# ---- ACPICA ----
ALL_ACPICA_SOURCES = $(wildcard $(ACPICA_DIR)/components/*/*.c)
ACPICA_SOURCES = $(filter-out \
    $(ACPICA_DIR)/components/debugger/%.c \
    $(ACPICA_DIR)/components/disassembler/%.c \
    $(ACPICA_DIR)/components/resources/rsdump.c, \
    $(ALL_ACPICA_SOURCES))
ACPICA_OBJECTS = $(patsubst \
    $(ACPICA_DIR)/%.c, \
    $(BUILD_ACPI_DIR)/%.o, \
    $(ACPICA_SOURCES))

KERNEL_OBJECTS = $(KERNEL_C_OBJECTS) $(KERNEL_ASM_OBJECTS) $(ACPICA_OBJECTS)

# ---- Загрузчики ----
BOOT1_SRC = $(BOOT_SRC_DIR)/boot1.asm
BOOT2_SRC = $(BOOT_SRC_DIR)/boot2.asm
BOOT1_BIN = $(BUILD_BOOT_DIR)/boot1.bin
BOOT2_BIN = $(BUILD_BOOT_DIR)/boot2.bin

# ---- Пользовательские программы ----
USER_C_SOURCES = $(wildcard $(USER_BIN_DIR)/*.c)
USER_OBJECTS   = $(patsubst $(USER_BIN_DIR)/%.c, $(BUILD_USER_BIN)/%.o,   $(USER_C_SOURCES))
USER_BINARIES  = $(patsubst $(USER_BIN_DIR)/%.c, $(BUILD_USER_BIN)/%.elf, $(USER_C_SOURCES))

# ---- Пользовательская lib ----
USER_LIB_C_SOURCES := $(shell find $(USER_LIB_DIR) -name '*.c')
USER_LIB_ASM_SOURCES := $(shell find $(USER_LIB_DIR) -name '*.asm')

# Зеркалируем относительные пути
USER_LIB_C_OBJECTS   := $(patsubst $(USER_LIB_DIR)/%.c,   $(BUILD_USER_LIB)/%.o, $(USER_LIB_C_SOURCES))
USER_LIB_ASM_OBJECTS := $(patsubst $(USER_LIB_DIR)/%.asm, $(BUILD_USER_LIB)/%.o, $(USER_LIB_ASM_SOURCES))

USER_LIB_OBJECTS = $(USER_LIB_C_OBJECTS) $(USER_LIB_ASM_OBJECTS)

# ---- Зависимости ----
KERNEL_DEPS = $(KERNEL_C_OBJECTS:.o=.d) $(ACPICA_OBJECTS:.o=.d)
USER_DEPS   = $(USER_OBJECTS:.o=.d) $(USER_LIB_C_OBJECTS:.o=.d)

# ---- Образ ----
IMAGE = $(BUILD_DIR)/myos.img

# ============================================================
#  qemu
# ============================================================

QEMU = qemu-system-i386

QEMU_FLAGS_COMMON = -device ahci,id=ahci \
	    -device ide-hd,drive=disk,bus=ahci.0 \
	    -drive format=raw,file=$(IMAGE),if=none,id=disk \
	    -display sdl -vga std -m 256 -enable-kvm

QEMU_FLAGS_RELEASE = $(QEMU_FLAGS_COMMON) -enable-kvm

QEMU_FLAGS_DEBUG = -device ahci,id=ahci \
	    -device ide-hd,drive=disk,bus=ahci.0 \
	    -drive format=raw,file=$(IMAGE),if=none,id=disk \
	    -display sdl -vga std \
		-monitor unix:/tmp/qemu-monitor.sock,server,nowait\
		 -s -S -d int -D $(BUILD_LOGS_DIR)/interrupts.log\
	    > $(QEMU_LOG) 2>&1 & echo $$! > /tmp/qemu.pid

QEMU_LOG = $(BUILD_LOGS_DIR)/qemu.log

# ============================================================
#  Основные цели
# ============================================================

.PHONY: all release run build-debug debug run-debug clean picolibc_build

# парсер по словам определит флаги
debug: build-debug
build-debug: all
release: all

all: $(IMAGE)
	@echo "✅ Build complete: $<"

run: all
	@echo "Starting QEMU..."
	$(QEMU) $(QEMU_FLAGS_RELEASE)

run-debug: all | $(BUILD_LOGS_DIR)
	@echo "Starting QEMU with GDB server (port 1234)..."
	@(nohup $(QEMU) $(QEMU_FLAGS_DEBUG))
	@timeout=0; \
	while ! nc -z localhost 1234 2>/dev/null; do \
	    sleep 0.1; \
	    timeout=$$((timeout + 1)); \
	    if [ $$timeout -gt 300 ]; then \
	        echo "❌ Timeout waiting for QEMU"; \
	        tail -n 10 $(QEMU_LOG); \
	        kill $$(cat /tmp/qemu.pid) 2>/dev/null || true; \
	        exit 1; \
	    fi; \
	done
	@echo "✅ QEMU up, attach GDB to :1234"

clean:
	@echo "🧹 Cleaning..."
	$(RM) $(BUILD_DIR_ROOT)

# ============================================================
#  Каталоги
# ============================================================

$(BUILD_DIR) $(BUILD_BOOT_DIR) $(BUILD_KERNEL_DIR) $(BUILD_ACPI_DIR) $(BUILD_USER_BIN) \
$(BUILD_USER_LIB) $(BUILD_LOGS_DIR) $(BUILD_PICOLIBC_DIR):
	@mkdir -p $@

# ============================================================
#  Загрузчики
# ============================================================

$(BUILD_BOOT_DIR)/%.bin: $(BOOT_SRC_DIR)/%.asm | $(BUILD_BOOT_DIR)
	@echo "  [BOOT]  $<"
	$(NASM) -f bin -o $@ $<

# ============================================================
#  Ядро (рекурсивно)
# ============================================================

# правило компиляции для всех .o файлов которые сопоставляются
# по относительному пути .c файлам в исходниках
$(BUILD_KERNEL_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  [CC]    $<"
	$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(BUILD_KERNEL_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo "  [ASM]   $<"
	$(NASM) $(KERNEL_NASM) -o $@ $<

# ACPICA — отдельное правило, свой набор флагов
$(BUILD_ACPI_DIR)/%.o: $(ACPICA_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "  [ACPI]  $<"
	$(CC) $(KERNEL_CFLAGS) $(ACPICA_CFLAGS) -c -o $@ $<

$(BUILD_KERNEL_DIR)/kernel.elf: $(KERNEL_OBJECTS) $(LINKER_SCRIPT)
	@echo "  [LD]    kernel.elf"
	$(LD) $(KERNEL_LDFLAGS) -o $@ $(KERNEL_OBJECTS)

$(BUILD_KERNEL_DIR)/kernel.bin: $(BUILD_KERNEL_DIR)/kernel.elf
	@echo "  [BIN]   kernel.bin"
	$(OBJCOPY) -O binary -S $< $@
	@echo "          size: $$(wc -c < $@) bytes"


# ============================================================
#  picolibc
# ============================================================

$(BUILD_PICOLIBC_DIR)/build.ninja: $(PICOLIBC_CROSS) | $(BUILD_PICOLIBC_DIR)
	@echo "  [MESON] picolibc configure"
	meson setup $(BUILD_PICOLIBC_DIR) $(PICOLIBC_DIR) \
	    --cross-file $(abspath $(PICOLIBC_CROSS)) \
	    -Dpicocrt=false \
	    -Dsemihost=false \
	    -Dposix-console=false \
	    -Dtests=false \
	    -Dmultilib=false \
	    -Dsingle-thread=true \
	    -Dthread-local-storage=false \
	    -Dnewlib-global-errno=true \
	    -Denable-malloc=true \
	    -Dprefix=$(abspath $(PICOLIBC_INSTALL_DIR)) \
	    -Dincludedir=include \
	    -Dlibdir=lib

$(PICOLIBC_LIBC): $(BUILD_PICOLIBC_DIR)/build.ninja
	@echo "  [NINJA] picolibc build & install"
	ninja -C $(BUILD_PICOLIBC_DIR)
	ninja -C $(BUILD_PICOLIBC_DIR) install

picolibc_build: $(PICOLIBC_LIBC)

# ============================================================
#  Пользовательский lib
# ============================================================

$(BUILD_USER_LIB)/%.o: $(USER_LIB_DIR)/%.asm $(PICOLIBC_LIBC) | $(BUILD_USER_LIB)
	@mkdir -p $(dir $@)
	@echo "  [UASM]   $<"
	$(NASM) $(USER_NASM) -o $@ $<

$(BUILD_USER_LIB)/%.o: $(USER_LIB_DIR)/%.c $(PICOLIBC_LIBC) | $(BUILD_USER_LIB)
	@mkdir -p $(dir $@)
	@echo "  [UCC]   $<"
	$(CC) $(USER_CFLAGS) -c -o $@ $<

# ============================================================
#  Пользовательские программы
# ============================================================

$(BUILD_USER_BIN)/%.o: $(USER_BIN_DIR)/%.asm $(PICOLIBC_LIBC) | $(BUILD_USER_BIN)
	@mkdir -p $(dir $@)
	@echo "  [UASM]   $<"
	$(NASM) $(USER_NASM) -o $@ $<

$(BUILD_USER_BIN)/%.o: $(USER_BIN_DIR)/%.c $(PICOLIBC_LIBC) | $(BUILD_USER_BIN)
	@mkdir -p $(dir $@)
	@echo "  [UCC]   $<"
	$(CC) $(USER_CFLAGS) -c -o $@ $<

# линкуется с lib
$(BUILD_USER_BIN)/%.elf: $(BUILD_USER_BIN)/%.o $(USER_LIB_OBJECTS) \
                         $(USER_LINKER_SCRIPT) $(PICOLIBC_LIBC)
	@echo "  [ULD]   $@"
	$(LD) $(USER_LDFLAGS) -o $@ $< $(USER_LIB_OBJECTS) $(PICOLIBC_LIBC) $(LIBGCC)

# ============================================================
#  Диск
# ============================================================

IMAGE_FAT_SECTORS = 129024
FAT_TMP           = $(BUILD_DIR)/part.tmp
FAT_STAGE         = $(BUILD_DIR)/fat.stage

$(IMAGE): $(BOOT1_BIN) $(BOOT2_BIN) \
          $(BUILD_KERNEL_DIR)/kernel.bin \
          $(USER_BINARIES) | $(BUILD_DIR)
	@echo "========================================="
	@echo " Building disk image"
	@echo "========================================="

	@echo "  [1/6] Empty image (64 MiB)"
	@dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null

	@echo "  [2/6] MBR + bootable FAT32 partition"
	@parted -s $@ mklabel msdos
	@parted -s $@ mkpart primary fat32 1MiB 100%
	@parted -s $@ set 1 boot on

	@echo "  [3/6] mkfs.vfat"
	@dd if=/dev/zero of=$(FAT_TMP) bs=512 count=$(IMAGE_FAT_SECTORS) 2>/dev/null
	@mkfs.vfat -F 32 -s 8 -R 32 -n "MYOS" $(FAT_TMP) > /dev/null

	@echo "  [4/6] Staging FAT contents"
	@rm -rf $(FAT_STAGE)
	@mkdir -p $(FAT_STAGE)/bin $(FAT_STAGE)/test
	@cp $(BUILD_KERNEL_DIR)/kernel.bin $(FAT_STAGE)/kernel.bin
	@for elf in $(USER_BINARIES); do \
	    name=$$(basename $$elf .elf); \
	    cp $$elf $(FAT_STAGE)/bin/$$name; \
	    echo "          /bin/$$name"; \
	done
	@if [ -d $(TEST_FILES_DIR) ]; then \
	    cp -r $(TEST_FILES_DIR)/. $(FAT_STAGE)/test/; \
	fi

	@echo "  [5/6] mcopy into FAT32"
	@mcopy -s -i $(FAT_TMP) $(FAT_STAGE)/kernel.bin ::/
	@mcopy -s -i $(FAT_TMP) $(FAT_STAGE)/bin        ::/
	@if [ -d $(FAT_STAGE)/test ] && [ -n "$$(ls -A $(FAT_STAGE)/test)" ]; then \
	    mcopy -s -i $(FAT_TMP) $(FAT_STAGE)/test ::/; \
	fi

	@echo "  [6/6] Embedding into image"
	@dd if=$(FAT_TMP)   of=$@ bs=1M seek=1 conv=notrunc 2>/dev/null
	@dd if=$(BOOT1_BIN) of=$@ bs=446 conv=notrunc 2>/dev/null
	@dd if=$(BOOT2_BIN) of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null

	@rm -f $(FAT_TMP)
	@rm -rf $(FAT_STAGE)
	@echo "✅ Image: $@ ($$(du -h $@ | cut -f1))"

# ============================================================
#  Автозависимости
# ============================================================

-include $(KERNEL_DEPS)
-include $(USER_DEPS)

# Не удалять промежуточные .o при ошибке — иначе .d теряются
.PRECIOUS: $(KERNEL_C_OBJECTS) $(KERNEL_ASM_OBJECTS) $(ACPICA_OBJECTS) \
           $(USER_OBJECTS) $(USER_LIB_OBJECTS)