# ============================================================
#  Makefile — 32-битное ядро
# ============================================================
#
#  Цели:
#    all          релизная сборка + образ
#    run          собрать и запустить QEMU
#    build-debug  сборка с отладочной информацией
#    run-debug    QEMU с GDB-сервером
#    clean        удалить build/
#
#  Дерево исходников:
#    src/            ядро
#    src/boot/       загрузчики (boot1, boot2)
#    userspace/bin/  пользовательские программы (1 .c = 1 программа)
#    userspace/include/ общие заголовки
#    test/files/     файлы для /test на образе
#
#  Дерево сборки:
#    build/boot/
#    build/kernel/   и build/kernel/acpica/
#    build/user/bin/
# ============================================================

# ------- Инструменты -------
CC      = gcc
NASM    = nasm
LD      = ld
OBJCOPY = objcopy
RM      = rm -rf

# ------- Каталоги исходников -------
SRC_DIR        = src
BOOT_SRC_DIR   = $(SRC_DIR)/boot
ACPICA_DIR     = $(SRC_DIR)/acpica
ACPICA_INCLUDE = $(ACPICA_DIR)/include

USER_DIR       = userspace
USER_BIN_DIR   = $(USER_DIR)/bin
USER_INC_DIR   = $(USER_DIR)/include

TEST_FILES_DIR = test/files

# ------- Каталоги сборки -------
BUILD_DIR        = build
BUILD_BOOT_DIR   = $(BUILD_DIR)/boot
BUILD_KERNEL_DIR = $(BUILD_DIR)/kernel
BUILD_ACPI_DIR   = $(BUILD_KERNEL_DIR)/acpica
BUILD_USER_BIN   = $(BUILD_DIR)/user/bin

# ============================================================
#  Флаги
# ============================================================

# ---- Ядро ----
# -MMD -MP  →  gcc генерирует .d-файлы с зависимостями от заголовков,
#             которые затем подключаются через -include в конце.
KERNEL_CFLAGS_COMMON = -m32 -std=gnu11 -ffreestanding -nostdlib -fno-builtin \
                       -fno-stack-protector -fno-pic -mgeneral-regs-only \
                       -I$(SRC_DIR) -Werror -MMD -MP
KERNEL_CFLAGS_RELEASE = $(KERNEL_CFLAGS_COMMON) -O2
KERNEL_CFLAGS_DEBUG   = $(KERNEL_CFLAGS_COMMON) -g -O0 -fno-omit-frame-pointer

KERNEL_NASM_RELEASE = -f elf32
KERNEL_NASM_DEBUG   = -f elf32 -g

KERNEL_LDFLAGS = -m elf_i386 -T $(SRC_DIR)/link.ld -nostdlib

# ---- ACPICA ----
ACPICA_CFLAGS  = -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-sign-compare
ACPICA_CFLAGS += -ffreestanding -nostdlib -fno-builtin
ACPICA_CFLAGS += -I$(ACPICA_INCLUDE) -I$(ACPICA_DIR)/components

# ---- Пользовательские программы ----
USER_CFLAGS_COMMON = -m32 -std=gnu11 -ffreestanding -nostdlib -nostartfiles \
                     -nodefaultlibs -static -no-pie -fno-pic -fno-builtin \
                     -fno-stack-protector -I$(USER_INC_DIR) \
                     -Wall -Wextra -Werror -MMD -MP
USER_CFLAGS_RELEASE = $(USER_CFLAGS_COMMON) -O2
USER_CFLAGS_DEBUG   = $(USER_CFLAGS_COMMON) -g -O0 -fno-omit-frame-pointer

USER_LDFLAGS = -m elf_i386 -T $(USER_DIR)/link.ld -nostdlib

# ---- Значения по умолчанию (release) ----
KERNEL_CFLAGS = $(KERNEL_CFLAGS_RELEASE)
KERNEL_NASM   = $(KERNEL_NASM_RELEASE)
USER_CFLAGS   = $(USER_CFLAGS_RELEASE)

# ============================================================
#  Списки исходников и целей
# ============================================================

# ---- Ядро ----
KERNEL_C_SOURCES   = $(wildcard $(SRC_DIR)/*.c)
KERNEL_ASM_SOURCES = $(wildcard $(SRC_DIR)/*.asm)

KERNEL_C_OBJECTS   = $(patsubst $(SRC_DIR)/%.c,   $(BUILD_KERNEL_DIR)/%.o, $(KERNEL_C_SOURCES))
KERNEL_ASM_OBJECTS = $(patsubst $(SRC_DIR)/%.asm, $(BUILD_KERNEL_DIR)/%.o, $(KERNEL_ASM_SOURCES))

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

# ---- Файлы автозависимостей (.d), генерируются -MMD ----
KERNEL_DEPS = $(KERNEL_C_OBJECTS:.o=.d) $(ACPICA_OBJECTS:.o=.d)
USER_DEPS   = $(USER_OBJECTS:.o=.d)

# ---- Образ ----
IMAGE = $(BUILD_DIR)/myos.img

# ============================================================
#  Основные цели
# ============================================================

.PHONY: all run build-debug run-debug clean

all: $(IMAGE)
	@echo "✅ Build complete: $<"

# Отладочная сборка — отдельный build-каталог, чтобы не смешивать с release
build-debug:
	@echo "Building with debug info..."
	$(MAKE) \
	    BUILD_DIR=build/debug \
	    KERNEL_CFLAGS='$(KERNEL_CFLAGS_DEBUG)' \
	    KERNEL_NASM='$(KERNEL_NASM_DEBUG)' \
	    USER_CFLAGS='$(USER_CFLAGS_DEBUG)' \
	    all

run: all
	@echo "Starting QEMU..."
	qemu-system-i386 -monitor stdio \
	    -device ahci,id=ahci \
	    -device ide-hd,drive=disk,bus=ahci.0 \
	    -drive format=raw,file=$(IMAGE),if=none,id=disk \
	    -display sdl -vga std -m 256 -enable-kvm

run-debug: build-debug
	@echo "Starting QEMU with GDB server (port 1234)..."
	@(nohup qemu-system-i386 -d int -D build/debug/interrupts.log \
	    -monitor unix:/tmp/qemu-monitor.sock,server,nowait \
	    -device ahci,id=ahci \
	    -device ide-hd,drive=disk,bus=ahci.0 \
	    -drive format=raw,file=build/debug/myos.img,if=none,id=disk \
	    -display sdl -vga std -s -S -m 256 \
	    > build/debug/qemu.log 2>&1 & echo $$! > /tmp/qemu.pid)
	@timeout=0; \
	while ! nc -z localhost 1234 2>/dev/null; do \
	    sleep 0.1; \
	    timeout=$$((timeout + 1)); \
	    if [ $$timeout -gt 300 ]; then \
	        echo "❌ Timeout waiting for QEMU"; \
	        tail -n 10 build/debug/qemu.log; \
	        kill $$(cat /tmp/qemu.pid) 2>/dev/null || true; \
	        exit 1; \
	    fi; \
	done
	@echo "✅ QEMU up, attach GDB to :1234"

clean:
	@echo "🧹 Cleaning..."
	$(RM) $(BUILD_DIR) build/debug

# ============================================================
#  Каталоги
# ============================================================

$(BUILD_BOOT_DIR) $(BUILD_KERNEL_DIR) $(BUILD_ACPI_DIR) $(BUILD_USER_BIN):
	@mkdir -p $@

# ============================================================
#  Загрузчики
# ============================================================

$(BUILD_BOOT_DIR)/%.bin: $(BOOT_SRC_DIR)/%.asm | $(BUILD_BOOT_DIR)
	@echo "  [BOOT]  $<"
	$(NASM) -f bin -o $@ $<

# ============================================================
#  Ядро
# ============================================================

$(BUILD_KERNEL_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_KERNEL_DIR)
	@echo "  [CC]    $<"
	$(CC) $(KERNEL_CFLAGS) -c -o $@ $<

$(BUILD_KERNEL_DIR)/%.o: $(SRC_DIR)/%.asm | $(BUILD_KERNEL_DIR)
	@echo "  [ASM]   $<"
	$(NASM) $(KERNEL_NASM) -o $@ $<

$(BUILD_ACPI_DIR)/%.o: $(ACPICA_DIR)/%.c | $(BUILD_ACPI_DIR)
	@mkdir -p $(dir $@)
	@echo "  [ACPI]  $<"
	$(CC) $(KERNEL_CFLAGS) $(ACPICA_CFLAGS) -c -o $@ $<

$(BUILD_KERNEL_DIR)/kernel.elf: $(KERNEL_OBJECTS) $(SRC_DIR)/link.ld | $(BUILD_KERNEL_DIR)
	@echo "  [LD]    kernel.elf"
	$(LD) $(KERNEL_LDFLAGS) -o $@ $(KERNEL_OBJECTS)

$(BUILD_KERNEL_DIR)/kernel.bin: $(BUILD_KERNEL_DIR)/kernel.elf
	@echo "  [BIN]   kernel.bin"
	$(OBJCOPY) -O binary -S $< $@
	@echo "          size: $$(wc -c < $@) bytes"

# ============================================================
#  Пользовательские программы
# ============================================================

$(BUILD_USER_BIN)/%.o: $(USER_BIN_DIR)/%.c | $(BUILD_USER_BIN)
	@echo "  [UCC]   $<"
	$(CC) $(USER_CFLAGS) -c -o $@ $<

$(BUILD_USER_BIN)/%.elf: $(BUILD_USER_BIN)/%.o $(USER_DIR)/link.ld
	@echo "  [ULD]   $@"
	$(LD) $(USER_LDFLAGS) -o $@ $<

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
# -include (с минусом) не падает, если .d ещё не сгенерированы
# (первая сборка, после clean).
#
# Что внутри .d:
#   build/kernel/task.o: src/task.c src/task.h src/heap.h src/paging.h ...
#
# А также (благодаря -MP) заголовки-призраки:
#   src/task.h:
# чтобы удаление заголовка не ломало сборку.
# ============================================================

-include $(KERNEL_DEPS)
-include $(USER_DEPS)

# Не удалять промежуточные .o при ошибке — иначе .d теряются
.PRECIOUS: $(KERNEL_C_OBJECTS) $(KERNEL_ASM_OBJECTS) $(ACPICA_OBJECTS) $(USER_OBJECTS)