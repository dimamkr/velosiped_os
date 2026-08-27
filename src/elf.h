#ifndef ELF_H
#define ELF_H

#include "types.h"
#include "task.h"

/* ---- ELF-заголовок: магическое число ---- */
#define ELF_MAGIC 0x464C457F /* 0x7F + 'E','L','F' */

/* Индексы в массиве e_ident */
#define EI_MAG0 0
#define EI_MAG1 1
#define EI_MAG2 2
#define EI_MAG3 3
#define EI_CLASS 4
#define EI_DATA 5
#define EI_VERSION 6
#define EI_OSABI 7
#define EI_ABIVERSION 8
#define EI_PAD 9
#define EI_NIDENT 16

/* ---- Класс (EI_CLASS) ---- */
#define ELFCLASS32 1
#define ELFCLASS64 2

/* ---- Порядок байт (EI_DATA) ---- */
#define ELFDATA2LSB 1 /* little-endian */
#define ELFDATA2MSB 2 /* big-endian */

/* ---- Версия (e_version) ---- */
#define EV_CURRENT 1

/* ---- Типы файлов (e_type) ---- */
#define ET_NONE 0 /* неизвестный */
#define ET_REL 1  /* объектный файл (.o) */
#define ET_EXEC 2 /* исполняемый файл */
#define ET_DYN 3  /* динамически линкуемый (PIE) */
#define ET_CORE 4 /* core-файл */

/* ---- Машинные архитектуры (e_machine) ---- */
#define EM_NONE 0
#define EM_386 3      /* Intel 80386 */
#define EM_ARM 40     /* ARM */
#define EM_X86_64 62  /* AMD64 */
#define EM_AVR 0x1F   /* AVR */
#define EM_RISCV 0xF3 /* RISC-V */
/* можно добавить другие по необходимости */

/* ---- Некоторые вспомогательные макросы для проверок ---- */
#define ELF_IS_32BIT(ehdr) ((ehdr)->e_ident[EI_CLASS] == ELFCLASS32)
#define ELF_IS_LITTLE_ENDIAN(ehdr) ((ehdr)->e_ident[EI_DATA] == ELFDATA2LSB)
#define ELF_IS_EXEC(ehdr) ((ehdr)->e_type == ET_EXEC || (ehdr)->e_type == ET_DYN)
#define ELF_IS_LOADABLE(phdr) ((phdr)->p_type == PT_LOAD)

typedef uint32_t elf32_addr;
typedef uint32_t elf32_off;
typedef uint16_t elf32_half;
typedef uint32_t elf32_word;

// Заголовок ELF (Ehdr) – 52 байта
typedef struct
{
    uint8_t e_ident[16];  // магическое число и информация
    elf32_half e_type;    // ET_EXEC (2), ET_DYN (3), ET_REL (1)
    elf32_half e_machine; // интересует лишь EM_386 (3)
    elf32_word e_version; // 1 (EV_CURRENT)
    elf32_addr e_entry;   // точка входа (виртуальный адрес)
    elf32_off e_phoff;    // смещение до таблицы program headers
    elf32_off e_shoff;    // смещение до section headers
    elf32_word e_flags;
    elf32_half e_ehsize;    // размер заголовка (52)
    elf32_half e_phentsize; // размер одного program header (32)
    elf32_half e_phnum;     // количество program headers
    elf32_half e_shentsize; // размер section header
    elf32_half e_shnum;     // количество section headers
    elf32_half e_shstrndx;  // индекс строковой таблицы секций
} elf32_ehdr_t;

// Program header (Phdr) – 32 байта
typedef struct
{
    elf32_word p_type;   // PT_LOAD (1), PT_INTERP (3), PT_NULL (0)...
    elf32_off p_offset;  // смещение в файле
    elf32_addr p_vaddr;  // виртуальный адрес загрузки
    elf32_addr p_paddr;  // физический (обычно не используется)
    elf32_word p_filesz; // размер в файле
    elf32_word p_memsz;  // размер в памяти (может быть больше из-за BSS)
    elf32_word p_flags;  // права (PF_R, PF_W, PF_X)
    elf32_word p_align;  // выравнивание (обычно 0x1000 для страниц)
} elf32_phdr_t;

bool elf_check(void *file_buff);
void elf_print_info(void *file_buff);
bool elf_try_exec(void *file_buff, task_t **new_task);
void elf_test();

#endif