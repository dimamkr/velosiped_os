#include "elf.h"
#include "konsole.h"

#include "elf.h"
#include "konsole.h"

/* Массивы строк для преобразования числовых значений */
static const char *elf_class_str[] = {
    "NONE",  /* 0 */
    "ELF32", /* 1 */
    "ELF64"  /* 2 */
};

static const char *elf_data_str[] = {
    "NONE",         /* 0 */
    "LSB (little)", /* 1 */
    "MSB (big)"     /* 2 */
};

static const char *elf_type_str[] = {
    "NONE", /* 0 */
    "REL",  /* 1 */
    "EXEC", /* 2 */
    "DYN",  /* 3 */
    "CORE"  /* 4 */
};

static const char *elf_machine_to_str(uint16_t machine)
{
    switch (machine)
    {
    case EM_NONE:
        return "No machine";
    case EM_386:
        return "Intel 80386";
    case EM_ARM:
        return "ARM";
    case EM_X86_64:
        return "AMD x86-64";
    default:
        return "Unknown";
    }
}

/* Функция вывода информации об ELF-файле */
void elf_print_info(void *file_buff)
{
    elf32_ehdr_t *ehdr = (elf32_ehdr_t *)file_buff;

    if (!ehdr)
    {
        konsole_println("ELF header is NULL");
        return;
    }

    konsole_println("========== ELF Information ==========");

    /* Магическое число (проверяем, что это ELF) */
    if (*(uint32_t *)(ehdr->e_ident) == ELF_MAGIC)
    {
        konsole_println("Magic: ELF");
    }
    else
    {
        konsole_println("Magic: INVALID");
    }

    /* Класс */
    uint8_t class = ehdr->e_ident[EI_CLASS];
    if (class <= 2)
        konsole_printf("Class: %s\n", elf_class_str[class]);
    else
        konsole_printf("Class: %x (unknown)\n", class);

    /* Порядок байт */
    uint8_t data = ehdr->e_ident[EI_DATA];
    if (data <= 2)
        konsole_printf("Data: %s\n", elf_data_str[data]);
    else
        konsole_printf("Data: %x (unknown)\n", data);

    /* Версия ELF */
    konsole_printf("Version: %d\n", ehdr->e_ident[EI_VERSION]);

    /* OS/ABI */
    konsole_printf("OS/ABI: %d\n", ehdr->e_ident[EI_OSABI]);

    /* Тип файла */
    uint16_t type = ehdr->e_type;
    if (type <= 4)
        konsole_printf("Type: %s\n", elf_type_str[type]);
    else
        konsole_printf("Type: %x (unknown)\n", type);

    /* Архитектура */
    uint16_t machine = ehdr->e_machine;
    konsole_printf("Machine: %s (%x)\n", elf_machine_to_str(machine), machine);

    /* Версия (поле e_version) */
    konsole_printf("ELF version: %d\n", ehdr->e_version);

    /* Точка входа */
    konsole_printf("Entry point: %x\n", ehdr->e_entry);

    /* Program header table */
    konsole_printf("Program header offset: %x\n", ehdr->e_phoff);
    konsole_printf("Program header entries: %d\n", ehdr->e_phnum);
    konsole_printf("Program header entry size: %d\n", ehdr->e_phentsize);

    /* Section header table */
    konsole_printf("Section header offset: %x\n", ehdr->e_shoff);
    konsole_printf("Section header entries: %d\n", ehdr->e_shnum);
    konsole_printf("Section header entry size: %d\n", ehdr->e_shentsize);

    /* Флаги (зависят от архитектуры) */
    konsole_printf("Flags: %x\n", ehdr->e_flags);

    /* Размер ELF-заголовка */
    konsole_printf("Header size: %d bytes\n", ehdr->e_ehsize);

    konsole_println("======================================");
}

bool elf_check(void *file_buff)
{
    elf32_ehdr_t *ehdr = (elf32_ehdr_t *)file_buff;

    if (*(uint32_t *)(ehdr->e_ident) != ELF_MAGIC)
        return false;
    if (!ELF_IS_32BIT(ehdr))
        return false;
    if (!ELF_IS_LITTLE_ENDIAN(ehdr))
        return false;
    if (ehdr->e_machine != EM_386)
        return false;
    if (!ELF_IS_EXEC(ehdr))
        return false;

    return true;
}