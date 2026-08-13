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
                 -fno-pic -mgeneral-regs-only -Os

# Флаги для отладочной сборки
CFLAGS_DEBUG   = -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
                 -fno-pic -mgeneral-regs-only -g -O0 -fno-omit-frame-pointer

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
ASM_SOURCES = $(filter-out $(SRC_DIR)/boot.asm, $(ALL_ASM))

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
	qemu-system-x86_64 -drive format=raw,file=$(BUILD_DIR)/myos.img,if=ide \
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
	@(nohup qemu-system-x86_64 -drive format=raw,file=$(BUILD_DIR)/myos.img,if=ide \
	    -monitor stdio -no-reboot -display sdl -vga std -s -S -m 256 \
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

# Компиляция ассемблерных файлов (кроме boot.asm) в объектные
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm | $(BUILD_DIR)
	@echo "1.1. Assembling $<..."
	$(NASM) $(NASMFLAGS) -o $@ $<

# Сборка загрузчика (boot.asm) – бинарный файл
$(BUILD_DIR)/boot.bin: $(SRC_DIR)/boot.asm | $(BUILD_DIR)
	@echo "5. Building bootloader..."
	$(NASM) -f bin -o $@ $<

# Линковка ядра в ELF
$(BUILD_DIR)/kernel.elf: $(OBJECTS) $(SRC_DIR)/link.ld | $(BUILD_DIR)
	@echo "2. Linking kernel..."
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# Получение плоского бинарника из ELF (убираем отладочную информацию)
$(BUILD_DIR)/kernel.bin: $(BUILD_DIR)/kernel.elf | $(BUILD_DIR)
	@echo "3. Creating binary..."
	$(OBJCOPY) -O binary -S $< $@
	@echo "   Kernel size: $$(wc -c < $@) bytes"

# Создание образа дискеты (IMG) из загрузчика и ядра
$(BUILD_DIR)/myos.img: $(BUILD_DIR)/boot.bin $(BUILD_DIR)/kernel.bin | $(BUILD_DIR)
	@echo "6. Creating disk image..."
	dd if=/dev/zero of=$@ bs=512 count=2880 2>/dev/null
	dd if=$(BUILD_DIR)/boot.bin of=$@ conv=notrunc 2>/dev/null
	dd if=$(BUILD_DIR)/kernel.bin of=$@ bs=512 seek=1 conv=notrunc 2>/dev/null
	@echo "   Image created: $@"

# Фантомные цели (не файлы)
.PHONY: all run build-debug run-debug clean debug-internal