#!/bin/bash
echo "=== Keyboard Debug Test ==="
echo "==========================="

# Очистка
rm -rf build
mkdir -p build

echo "1. Compiling C files..."
gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-pic -mgeneral-regs-only -Os -c -o build/kernel.o kernel.c
gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-pic -mgeneral-regs-only -Os -c -o build/konsole.o konsole.c
gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-pic -mgeneral-regs-only -Os -c -o build/keyboard.o keyboard.c
gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-pic -mgeneral-regs-only -Os -c -o build/terminal.o terminal.c
gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-pic -mgeneral-regs-only -Os -c -o build/idt_initialiser.o idt_initialiser.c
gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-pic -mgeneral-regs-only -Os -c -o build/system.o system.c
gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-pic -mgeneral-regs-only -Os -c -o build/isr.o isr.c
gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-pic -mgeneral-regs-only -Os -c -o build/timer.o timer.c
gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-pic -mgeneral-regs-only -Os -c -o build/gdt_initialiser.o gdt_initialiser.c
gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-pic -mgeneral-regs-only -Os -c -o build/datetime.o datetime.c
gcc -m32 -ffreestanding -nostdlib -fno-builtin -fno-stack-protector \
    -fno-pic -mgeneral-regs-only -Os -c -o build/heap.o heap.c

#

#сброс если ошибки
if [ $? -ne 0 ]; then
    echo "✅ О НЕТ ВСЕ ПЛОХО!"
    exit 1
fi


echo "1.1. Compiling interrupts.asm"

nasm -f elf32 -o build/interrupts.o interrupts.asm

echo "2. Linking..."
ld -m elf_i386 -T link.ld -nostdlib -o build/kernel.elf \
    build/kernel.o build/konsole.o build/keyboard.o build/terminal.o \
    build/idt_initialiser.o build/isr.o build/interrupts.o build/system.o \
    build/timer.o build/gdt_initialiser.o build/datetime.o build/heap.o

echo "3. Creating binary..."
objcopy -O binary -S build/kernel.elf build/kernel.bin

echo "4. Checking kernel..."
echo "Entry point address:"
readelf -h build/kernel.elf
echo ""
echo "First 10 functions:"
nm build/kernel.elf | head -10

echo "5. Building bootloader..."
nasm -f bin -o build/boot.bin boot.asm


echo "6. Creating disk image..."
# Создаем пустой образ
dd if=/dev/zero of=build/myos.img bs=512 count=2880 2>/dev/null
# Записываем загрузчик
dd if=build/boot.bin of=build/myos.img conv=notrunc 2>/dev/null
# Записываем ядро
dd if=build/kernel.bin of=build/myos.img bs=512 seek=1 conv=notrunc 2>/dev/null

echo "6. Creating .iso image..."
#сделать .iso
mkisofs -R -b myos.img -no-emul-boot -boot-load-size 4 -o build/myos.iso build/
isohybrid build/myos.iso

echo "7. Kernel size: $(wc -c < build/kernel.bin) bytes"

echo ""
echo "========================================="
echo "Starting QEMU with PS/2 keyboard enabled"
echo "========================================="
echo "INSTRUCTIONS:"
echo "1. Wait for '=== KEYBOARD TEST ===' message"
echo "2. CLICK inside QEMU window with mouse"
echo "3. Press keys to see scancodes"
echo "4. Press ESC to exit test"
echo "========================================="

# Запускаем QEMU с правильными параметрами
qemu-system-x86_64 \
    -drive format=raw,file=build/myos.img,if=ide \
    -monitor stdio \
    -device isa-debug-exit,iobase=0xf4,iosize=0x04 \
    -no-reboot \
    -display sdl \
    -vga std \
    -d int \
    -D ./build/qemu_console.log \
    # -s -S \
    -m 256
