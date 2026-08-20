# ============================================================
# Makefile для 32-битного ядра
# Исходники в папке src/, сборка в build/
# Цели:
#   all         - полная сборка (релиз) с очисткой
#   run         - собрать и запустить QEMU без отладки
#   build-debug - сборка с отладочной информацией (-g -O0)
#   run-debug   - собрать с отладкой и запустить QEMU в режиме ожидания GDB
#   clean       - удалить папку build
# ============================================================

# Компиляторы
CC      = gcc
NASM    = nasm
LD      = ld
OBJCOPY = objcopy
RM      = rm -rf

# Каталоги
SRC_DIR   = src
BUILD_DIR = build

# Флаги для релизной сборки
CFLAGS_RELEASE = -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
                 -fno-pic -mgeneral-regs-only -Os -I$(SRC_DIR)

# Флаги для отладочной сборки
CFLAGS_DEBUG   = -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
                 -fno-pic -mgeneral-regs-only -g -O0 -fno-omit-frame-pointer -I$(SRC_DIR)

# Флаги по умолчанию (релиз)
CFLAGS = $(CFLAGS_RELEASE)

# Флаги для линковки
LDFLAGS = -m elf_i386 -T $(SRC_DIR)/link.ld -nostdlib

# Флаги для NASM (объектный файл)
NASMFLAGS = -f elf32

# Список C-файлов
C_SOURCES = $(wildcard $(SRC_DIR)/*.c)

# Список ассемблерных файлов (исключаем boot.asm, он собирается отдельно в бинарник)
ALL_ASM = $(wildcard $(SRC_DIR)/*.asm)
ASM_SOURCES = $(filter-out $(SRC_DIR)/boot1.asm $(SRC_DIR)/boot2.asm, $(ALL_ASM))

# Объектные файлы (в каталоге build)
C_OBJECTS   = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS = $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(ASM_SOURCES))
OBJECTS     = $(C_OBJECTS) $(ASM_OBJECTS)

# ============================================================
# Основные цели
# ============================================================

# Сборка (релиз) – очистка + сборка образа
all: clean $(BUILD_DIR)/myos.img
	@echo "✅ Build completed (release)."

# Запуск QEMU без отладки (зависит от all)
run: all
	@echo "========================================="
	@echo "Starting QEMU (without debug)..."
	@echo "========================================="
	qemu-system-i386 -monitor stdio -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0 -drive format=raw,file=$(BUILD_DIR)/myos.img,if=none,id=disk \
	    -no-reboot -display sdl -vga std -m 256

# Сборка с отладочной информацией (очистка + отладочная сборка)
build-debug: clean
	@$(MAKE) debug-internal

# Внутренняя цель для отладочной сборки (переопределяем CFLAGS)
debug-internal: CFLAGS = $(CFLAGS_DEBUG)
debug-internal: $(BUILD_DIR)/myos.img
	@echo "✅ Debug build completed."

# Запуск QEMU с отладкой (для VS Code)
run-debug: build-debug
	@echo "========================================="
	@echo "Starting QEMU with GDB server (for VS Code)..."
	@echo "========================================="
	@(nohup qemu-system-i386 -monitor stdio -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0 -drive format=raw,file=$(BUILD_DIR)/myos.img,if=none,id=disk \
	    -no-reboot -display sdl -vga std -s -S -m 256 \
	    > $(BUILD_DIR)/qemu.log 2>&1 & echo $$! > /tmp/qemu.pid)
	@echo "Waiting for QEMU to open port 1234..."
	@timeout=0; \
	while ! nc -z localhost 1234 2>/dev/null; do \
	    sleep 0.1; \
	    timeout=$$((timeout + 1)); \
	    if [ $$timeout -gt 30 ]; then \
	        echo "❌ Timeout: QEMU did not open port 1234."; \
	        echo "Last lines of qemu.log:"; \
	        tail -n 10 $(BUILD_DIR)/qemu.log; \
	        kill $$(cat /tmp/qemu.pid) 2>/dev/null || true; \
	        exit 1; \
	    fi; \
	done
	@echo "✅ QEMU started and port 1234 is open. You can now attach GDB."

# Очистка
clean:
	@echo "🧹 Cleaning build directory..."
	$(RM) $(BUILD_DIR)

# ============================================================
# Правила сборки
# ============================================================

# Создание каталога build (если его нет)
$(BUILD_DIR):
	mkdir -p $@

# Компиляция C-файлов
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "1. Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<

# Сборка загрузчика stage 1 (boot1.asm) – бинарный файл
$(BUILD_DIR)/boot1.bin: $(SRC_DIR)/boot1.asm | $(BUILD_DIR)
	@echo "1.1 Building bootloader (stage 1)..."
	$(NASM) -f bin -o $@ $<

# Сборка загрузчика stage 2 (boot2.asm)
$(BUILD_DIR)/boot2.bin: $(SRC_DIR)/boot2.asm | $(BUILD_DIR)
	@echo "1.2 Building bootloader (stage 2)..."
	$(NASM) -f bin -o $@ $<

# Компиляция ассемблерных файлов (кроме boot.asm) в объектные
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm | $(BUILD_DIR)
	@echo "1.3 Assembling $<..."
	$(NASM) $(NASMFLAGS) -o $@ $<

# Линковка ядра в ELF
$(BUILD_DIR)/kernel.elf: $(OBJECTS) $(SRC_DIR)/link.ld | $(BUILD_DIR)
	@echo "2. Linking kernel..."
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# Получение плоского бинарника из ELF (убираем отладочную информацию)
$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf | $(BUILD_DIR)
	@echo "3. Creating binary..."
	$(OBJCOPY) -O binary -S $< $@
	@echo "   Kernel size: $$(wc -c < $@) bytes"

$(BUILD_DIR)/myos.img: $(BUILD_DIR)/boot1.bin $(BUILD_DIR)/boot2.bin $(BUILD_DIR)/kernel.bin | $(BUILD_DIR)
	@echo "Creating disk image with MBR and FAT32..."
	# 1. Пустой образ 64 МБ
	dd if=/dev/zero of=$@ bs=1M count=64 2>/dev/null
	# 2. MBR и FAT32-раздел (LBA)
	parted -s $@ mklabel msdos
	parted -s $@ mkpart primary fat32 1MiB 100%
	parted -s $@ set 1 boot on
	# 3. Создаём FAT32-раздел (129024 сектора ≈ 63 МБ)
	dd if=/dev/zero of=part.tmp bs=512 count=129024 2>/dev/null
	mkfs.vfat -F 32 -s 8 -R 32 -n "MYOS" part.tmp
	# 4. Копируем ядро в корень FAT32
	mcopy -i part.tmp $(BUILD_DIR)/kernel.bin ::kernel.bin
	# 5. Вшиваем FAT32 в образ (смещение 1MiB)
	dd if=part.tmp of=$@ bs=1M seek=1 conv=notrunc 2>/dev/null
	rm -f part.tmp
	# 6. Пишем boot1 в MBR (первые 446 байт)
	dd if=$(BUILD_DIR)/boot1.bin of=$@ conv=notrunc bs=446 count=1 2>/dev/null
	# 7. Пишем boot2 в сектор 1 (сразу после MBR)
	dd if=$(BUILD_DIR)/boot2.bin of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "Image created: $@"

# Фантомные цели (не файлы)
.PHONY: all run build-debug run-debug clean debug-internal