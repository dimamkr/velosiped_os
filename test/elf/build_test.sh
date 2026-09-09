#!/bin/bash
# Компиляция тестовой программы в ELF-файл (32-битный, статический)
# Используйте i686-elf-gcc, если у вас кросс-компилятор, или gcc -m32

CC=i686-elf-gcc
if ! command -v $CC &> /dev/null; then
    CC=gcc
    echo "i686-elf-gcc not found, using gcc -m32"
fi

$CC -m32 -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -static -no-pie \
    -W -O0 -o test_elf.elf test_elf.c

# Проверка успешности
if [ $? -eq 0 ]; then
    echo "ELF создан: test_elf.elf"
else
    echo "Ошибка компиляции"
fi
