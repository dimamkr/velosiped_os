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

# ACPICA
ACPICA_DIR = $(SRC_DIR)/acpica
ACPICA_INCLUDE = $(ACPICA_DIR)/include
ACPICA_SOURCES = $(wildcard $(ACPICA_DIR)/components/*/*.c)
ACPICA_SOURCES := $(filter-out $(ACPICA_DIR)/components/debugger/%, $(ACPICA_SOURCES))
ACPICA_SOURCES := $(filter-out $(ACPICA_DIR)/components/disassembler/%, $(ACPICA_SOURCES))
ACPICA_OBJECTS = $(patsubst $(ACPICA_DIR)/%.c, $(BUILD_DIR)/acpica/%.o, $(ACPICA_SOURCES))
ACPICA_SOURCES := $(filter-out $(ACPICA_DIR)/components/utilities/%, $(ACPICA_SOURCES))

# Флаги для релизной сборки
CFLAGS_RELEASE = -m32 -std=gnu11 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
                 -fno-pic -mgeneral-regs-only -O0 -I$(SRC_DIR) -Werror

# Флаги для отладочной сборки
CFLAGS_DEBUG   = -m32 -std=gnu11 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
                 -fno-pic -mgeneral-regs-only -g -O0 -fno-omit-frame-pointer -I$(SRC_DIR) -Werror

# Флаги для ACPICA (отключаем варнинги, которые мешают сборке)
# ACPICA
ACPICA_CFLAGS = -Wno-unused-but-set-variable -Wno-unused-parameter -Wno-sign-compare
ACPICA_CFLAGS += -ffreestanding -nostdlib -fno-builtin
ACPICA_CFLAGS += -DACPI_APPLICATION
ACPICA_CFLAGS += -DACPI_SYSTEM_HEADERS
ACPICA_CFLAGS += -DACPI_DEBUGGER=0
ACPICA_CFLAGS += -DACPI_DISASSEMBLER=0
ACPICA_CFLAGS += -U__linux__ -U__gnu_linux__

# Флаги для NASM (релиз и отладка)
NASMFLAGS_RELEASE = -f elf32
NASMFLAGS_DEBUG   = -f elf32 -g

# По умолчанию используем релизные флаги
CFLAGS    = $(CFLAGS_RELEASE)
NASMFLAGS = $(NASMFLAGS_RELEASE)

# Флаги для линковки
LDFLAGS = -m elf_i386 -T $(SRC_DIR)/link.ld -nostdlib

# Список C-файлов
C_SOURCES = $(wildcard $(SRC_DIR)/*.c)

# Список ассемблерных файлов (исключаем boot.asm, они собираются отдельно)
ALL_ASM = $(wildcard $(SRC_DIR)/*.asm)
ASM_SOURCES = $(filter-out $(SRC_DIR)/boot1.asm $(SRC_DIR)/boot2.asm, $(ALL_ASM))

# Объектные файлы
C_OBJECTS   = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(C_SOURCES))
ASM_OBJECTS = $(patsubst $(SRC_DIR)/%.asm, $(BUILD_DIR)/%.o, $(ASM_SOURCES))
OBJECTS = $(C_OBJECTS) $(ASM_OBJECTS) $(ACPICA_OBJECTS)

# ============================================================
# Основные цели
# ============================================================

# Сборка (релиз) – очистка + сборка образа
all: clean $(BUILD_DIR)/myos.img
	@echo "✅ Build completed (release)."

# Сборка с отладочной информацией
build-debug: clean
	@echo "Building with debug info..."
	$(MAKE) debug-internal

# Внутренняя цель для отладочной сборки (переопределяем флаги)
debug-internal: CFLAGS = $(CFLAGS_DEBUG)
debug-internal: NASMFLAGS = $(NASMFLAGS_DEBUG)
debug-internal: $(BUILD_DIR)/myos.img
	@echo "✅ Debug build completed."

# Запуск QEMU без отладки
run: all
	@echo "========================================="
	@echo "Starting QEMU (without debug)..."
	@echo "========================================="
	qemu-system-i386 -monitor stdio -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0 -drive format=raw,file=$(BUILD_DIR)/myos.img,if=none,id=disk \
	    -no-reboot -display sdl -vga std -m 256

# Запуск QEMU с отладкой (для VS Code)
run-debug: build-debug
	@echo "========================================="
	@echo "Starting QEMU with GDB server (for VS Code)..."
	@echo "========================================="
	@(nohup qemu-system-i386 -d int -D build/interrupts.log -monitor stdio -device ahci,id=ahci -device ide-hd,drive=disk,bus=ahci.0 -drive format=raw,file=$(BUILD_DIR)/myos.img,if=none,id=disk \
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

# Создание каталога build
$(BUILD_DIR):
	mkdir -p $@

# Компиляция C-файлов
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "1. Compiling $<..."
	$(CC) $(CFLAGS) -c -o $@ $<

# Сборка загрузчика stage 1 (boot1.asm) – бинарный файл (без отладки)
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
	@echo "Kernel data: "
	readelf -l $<
	@echo "kernel.bin file size: $$(wc -c < $@) bytes"

# Сборка ACPICA
$(BUILD_DIR)/acpica/%.o: $(ACPICA_DIR)/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	@echo "1.4 Compiling ACPICA $<..."
	$(CC) $(CFLAGS) $(ACPICA_CFLAGS) -I$(ACPICA_INCLUDE) -c -o $@ $<

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
	# 4. Создаём временную папку с контентом
	mkdir -p fat32_content
	# 4.1 Текстовые файлы
	echo "Hello, FAT32 World!" > fat32_content/hello.txt
	echo "This is a test file for FAT32 parser." > fat32_content/info.txt
	echo "Line 1" > fat32_content/multiline.txt
	echo "Line 2" >> fat32_content/multiline.txt
	echo "Line 3" >> fat32_content/multiline.txt
	# 4.2 Вложенные папки и файлы
	mkdir -p fat32_content/docs
	echo "Document 1 content" > fat32_content/docs/doc1.txt
	echo "Document 2 content" > fat32_content/docs/doc2.txt
	echo "Nested file" > fat32_content/docs/nested.txt
	mkdir -p fat32_content/data
	echo "Data content" > fat32_content/data/data1.bin
	dd if=/dev/urandom of=fat32_content/data/random.bin bs=512 count=1 2>/dev/null
	mkdir -p fat32_content/scripts
	echo "#!/bin/bash" > fat32_content/scripts/hello.sh
	echo "echo 'Hello from script!'" >> fat32_content/scripts/hello.sh
	echo "echo 'Another line'" >> fat32_content/scripts/hello.sh
	mkdir -p fat32_content/empty_folder
	# 4.3 Пустые файлы
	touch fat32_content/empty.txt
	touch fat32_content/docs/empty.doc
	# 4.4 Файл с пробелами в имени
	echo "File with spaces content" > "fat32_content/file with spaces.txt"
	# 4.5 Файл с длинным именем (проверка LFN)
	echo "This is a file with a very long filename that definitely exceeds the old 8.3 DOS naming convention limit" > "fat32_content/This_is_a_very_long_filename_that_exceeds_8_3_limit.txt"
	# 5. Копируем всё во временный FAT32-образ
	mcopy -s -i part.tmp fat32_content/* ::/
	# 6. Копируем ядро в корень FAT32
	mcopy -i part.tmp $(BUILD_DIR)/kernel.bin ::kernel.bin
	# 7. Вшиваем FAT32 в образ (смещение 1MiB)
	dd if=part.tmp of=$@ bs=1M seek=1 conv=notrunc 2>/dev/null
	rm -f part.tmp
	rm -rf fat32_content
	# 8. Пишем boot1 в MBR (первые 446 байт)
	dd if=$(BUILD_DIR)/boot1.bin of=$@ conv=notrunc bs=446 count=1 2>/dev/null
	# 9. Пишем boot2 в сектор 1 (сразу после MBR)
	dd if=$(BUILD_DIR)/boot2.bin of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "Image created: $@"

# Фантомные цели
.PHONY: all run build-debug run-debug clean debug-internal