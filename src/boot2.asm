[BITS 16]
[ORG 0x7E00] ; stage 1 загружает нас по адресу 0x7E00


FAT_TEMP_BUFFER_SEGMENT equ 0xFE0 ; временный буффер, куда будет копироваться кусок таблицы fat
KERNEL_LOAD_SEGMENT equ 0x1000
KERNEL_LOAD_OFFSET equ 0x0000
KERNEL_LOAD_PHYS_HI equ 0x0001 ; старшее слово адреса загрузки ядра
KERNEL_LOAD_PHYS_LO equ 0x0000 ; младшее слово адреса загрузки ядра
STACK_TOP equ 0x90000 ; Верхушка стека в защищённом режиме

; сложение 32-битных чисел через переполнение
%macro add_32 4
    add %2, %4
    adc %1, %3
%endmacro

%macro read_cmos_register 2
    mov al, %1
    or al, 0x80
    mov dx, 0x70
    out dx, al
    mov dx, 0x71
    in al, dx
    mov %2, al
%endmacro

%macro read_disk 0
    push ax
    mov ax, 0x4200
    call disk_transfer
    pop ax
%endmacro

%macro write_disk 0
    push ax
    mov ax, 0x4300
    call disk_transfer
    pop ax
%endmacro

; ------------------------------------------------------------
; Точка входа
; ------------------------------------------------------------
start:
    ; Сохраняем номер диска, с которого нас загрузили
    mov [boot_drive], dl

    mov si, msg_loading
    call print_string_16

    ; читаем MBR

    read_disk

    mov ax, KERNEL_LOAD_SEGMENT
    mov es, ax

    ; сохраняем дату и время 

    xor dx, dx
    mov dl, 0x70

.cmos_wait_update:
    read_cmos_register 0x0A, al
    test al, 0x80
    jnz .cmos_wait_update

    read_cmos_register 0x00, byte [datetime]
    read_cmos_register 0x02, byte [datetime + 0x1]
    read_cmos_register 0x04, byte [datetime + 0x2]
    read_cmos_register 0x07, byte [datetime + 0x3]
    read_cmos_register 0x08, byte [datetime + 0x4]
    read_cmos_register 0x09, byte [datetime + 0x5]

    ; массовое копирование
    lea si, datetime
    mov di, 0x1B8
    mov cx, 3
    rep movsw

    write_disk ; сохраняем запись о дате и времени
    
    ; ищем сектор, помеченный как загрузочный (первый байт записи MBR - 0x80)
    mov cx, 4
    mov si, 0x1BE

.search_boot_partition:
    cmp byte [es:si], 0x80
    je .load_first_sector
    add si, 16
    loop .search_boot_partition
    jmp .no_boot_partition

.load_first_sector: ; загружаем смещение раздела в lba_packet
    mov ax, word [es:si + 0x8] ; младшее слово LBA
    mov word [lba_packet + 0x8], ax
    mov word [partition_offset], ax
    mov ax, word [es:si + 0xA] ; старшее слово LBA
    mov word [lba_packet + 0xA], ax
    mov word [partition_offset + 0x2], ax

    read_disk

    ; достаем смещения fat и области данных
    ; смещение таблицы fat = partition_offset + reserved_sectors
    ; смещение области данных = смещение fat + fat_size * fat_count

    ; смещение раздела
    mov ax, word [partition_offset]
    mov dx, word [partition_offset + 0x2]

    ; добавляем количество зарезервированных секторов
    mov cx, word [es:0x0E]
    xor bx, bx
    add_32 dx, ax, bx, cx

    ; сохраняем fat_offset
    mov word [fat_offset], ax
    mov word [fat_offset + 0x2], dx

    mov cx, word [es:0x24] ; размер fat в секторах
    mov bx, word [es:0x26]
    mov di, word [es:0x10] ; количество копий fat
    and di, 0x00FF

.make_data_offset:
    add_32 dx, ax, bx, cx
    dec di
    test di, di
    jnz .make_data_offset

    ; сохраняем data_offset
    mov word [data_offset], ax
    mov word [data_offset + 0x2], dx

    xor ah, ah
    mov al, byte [es:0x0D] ; sectors per cluster
    mov word [sectors_per_cluster], ax
    mov word [lba_packet + 0x2], ax ; сохраняем в lba_packet, чтобы читать по кластерам

    ; заносим номер корневого кластера
    mov ax, word [es:0x2C]
    mov dx, word [es:0x2E]

    call select_cluster
    read_disk

    ; ищем ядро в корневой директории 

    xor si, si
    mov cx, 128

.search_kernel:
    cmp byte [es:si + 0x00], 0x00 ; если пустая запись, то заканчиваем чтение
    je .no_kernel
    cmp byte [es:si + 0x0B], 0x0F ; если lfn-запись, то игнорируем
    je .search_kernel_1
    cmp byte [es:si + 0x0B], 0xE5 ; если удаленная запись, то игнорируем
    je .search_kernel_1
    test byte [es:si + 0x0B], 0x18 ; если метка тома или папка, то игнорируем
    jnz .search_kernel_1

    ; сравнение dos имени файла и расширения
    mov ax, word [kernel_dos_fullname + 0x00]
    cmp word [es:si + 0x00], ax
    jne .search_kernel_1
    mov ax, word [kernel_dos_fullname + 0x02]
    cmp word [es:si + 0x02], ax
    jne .search_kernel_1
    mov ax, word [kernel_dos_fullname + 0x04]
    cmp word [es:si + 0x04], ax
    jne .search_kernel_1
    mov ax, word [kernel_dos_fullname + 0x06]
    cmp word [es:si + 0x06], ax
    jne .search_kernel_1
    mov ax, word [kernel_dos_fullname + 0x08]
    cmp word [es:si + 0x08], ax
    jne .search_kernel_1
    mov al, byte [kernel_dos_fullname + 0x0A]
    cmp byte [es:si + 0x0A], al
    je .read_kernel

.search_kernel_1:
    add si, 32
    loop .search_kernel

.read_kernel:
    ; номер начального кластера файла
    mov ax, word [es:si + 0x1A]
    mov dx, word [es:si + 0x14]
    call select_cluster
    call fat_at
    
    mov si, KERNEL_LOAD_PHYS_HI
    mov di, KERNEL_LOAD_PHYS_LO

.kernel_reading:
    cmp dx, 0x0FFF
    jne .kernel_reading_1
    cmp ax, 0xFFF7
    je disk_error ; fat-записи 0x0FFFFFF7 (поврежденный кластер) в процессе чтения быть не должно
    jmp .kernel_reading_2
.kernel_reading_1:
    test dx, dx
    jnz .kernel_reading_2
    test ax, ax
    jz disk_error ; fat-записи 0x00000000 (пустой кластер) в процессе чтения быть не должно
.kernel_reading_2:
    call addr_32_to_16
    mov word [lba_packet + 0x4], cx
    mov word [lba_packet + 0x6], bx
    read_disk

    cmp dx, 0x0FFF
    jne .kernel_reading_3
    cmp ax, 0xFFF8
    jae .load_ok ; если fat-запись больше или равна 0x0FFFFFF8, значит это был последний кластер в цепочке
.kernel_reading_3:
    ; сдвигаем текущий указатель памяти на размер кластера
    mov cx, word [sectors_per_cluster]
.memory_offset:
    add_32 si, di, 0, 512
    loop .memory_offset

    call select_cluster
    ; переходим к следующей fat-записи
    call fat_at
    and dx, 0x0FFF

    jmp .kernel_reading

.load_ok:
    ; Успешно загрузили ядро. Теперь переходим в защищённый режим.
    cli
    lgdt [gdt_descriptor]    ; Загружаем таблицу GDT

    ; Включаем защищённый режим (устанавливаем бит PE в CR0)
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    ; Дальний прыжок для перезагрузки сегментных регистров
    ; После этого процессор работает в 32-битном режиме

    jmp CODE_SEG:protected_mode

; ------------------------------------------------------------
; Ошибки
; ------------------------------------------------------------

.no_boot_partition:
    mov si, msg_no_boot_partition
    call print_string_16
    jmp $

.no_kernel:
    mov si, msg_no_kernel
    call print_string_16
    jmp $

; ------------------------------------------------------------
; 16-битные подпрограммы
; ------------------------------------------------------------

; выводит в консоль строку по адресу из si
print_string_16:
    pusha
    mov ah, 0x0E             ; Функция BIOS: телетайпный вывод
.loop:
    lodsb                    ; Загружаем байт из [SI] в AL, увеличиваем SI
    test al, al              ; Проверяем, не конец строки (AL == 0)?
    jz .done
    int 0x10                 ; Вызов BIOS для печати символа
    jmp .loop
.done:
    popa
    ret

dbg:
    mov si, msg_dbg
    call print_string_16
    jmp $

; выбирает для чтения кластер из dx:ax
select_cluster:
    pusha

    sub ax, 2 ; вычитаем 2 из младшего слова
    sbb dx, 0 ; вычитаем cf из старшего слова

    mov di, word [sectors_per_cluster]
    mov cx, word [data_offset]
    mov bx, word [data_offset + 0x2]

.make_cluster_offset:
    add_32 bx, cx, dx, ax
    dec di
    test di, di
    jnz .make_cluster_offset

    mov word [lba_packet + 0x8], cx
    mov word [lba_packet + 0xA], bx

    popa
    ret

; грузит в dx:ax запись из fat по смещению dx:ax
fat_at:
    push es
    push di
    push bx
    push cx
    push word [lba_packet + 0x2]
    push word [lba_packet + 0x4]
    push word [lba_packet + 0x6]
    push word [lba_packet + 0x8]
    push word [lba_packet + 0xA]

    mov bx, FAT_TEMP_BUFFER_SEGMENT
    mov es, bx

    ; в 1 секторе убирается 128 fat-записей
    ; хотим, чтобы в di хранилось смещение записи, а в dx:ax номер сектора
    mov di, ax
    mov bx, dx

    ; делим dx:ax на 128
    shr ax, 7
    mov bx, dx
    shl bx, 5
    or ax, bx
    shr dx, 7

    and di, 0x7F ; берем остаток от деления на 128
    shl di, 2 ; умножаем на 4

    mov cx, word [fat_offset]
    mov bx, word [fat_offset + 0x2]
    add_32 dx, ax, bx, cx

    ; грузим соответствующий сектор
    mov word [lba_packet + 0x2], 1
    mov word [lba_packet + 0x4], 0
    mov word [lba_packet + 0x6], FAT_TEMP_BUFFER_SEGMENT
    mov word [lba_packet + 0x8], ax
    mov word [lba_packet + 0xA], dx

    read_disk

    ; загружаем прочитанную запись в dx:ax
    mov ax, word [es:di]
    mov dx, word [es:di + 0x2]

    pop word [lba_packet + 0xA]
    pop word [lba_packet + 0x8]
    pop word [lba_packet + 0x6]
    pop word [lba_packet + 0x4]
    pop word [lba_packet + 0x2]
    pop cx
    pop bx
    pop di
    pop es

    ret

; преобразует адрес в виде 32-битного числа si:di в адрес bx:cx в формате сегмент:смещение
addr_32_to_16:
    mov bx, di
    mov cx, si
    shl cx, 12
    shr bx, 4
    or bx, cx
    mov cx, di
    and cx, 0x000F

    ret

; читает/записывает диск в зависимости от ax с параметрами из lba_packet
disk_transfer:
    pusha
    push es

    mov es, ax
    mov si, lba_packet       ; es:SI указывает на структуру пакета
    mov dl, [boot_drive]
    int 0x13
    jc disk_error

    pop es
    popa
    ret

disk_error:
    mov si, msg_disk_error
    call print_string_16
    jmp $

; ------------------------------------------------------------
; Данные
; ------------------------------------------------------------
datetime db 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
partition_offset dd 0
boot_drive db 0
sectors_per_cluster dw 0
fat_offset dd 0
data_offset dd 0

kernel_dos_fullname db "KERNEL  BIN"

msg_loading db "Stage 2 loaded", 0x0D, 0x0A, 0
msg_disk_error db "E: Disk transfer error", 0x0D, 0x0A, 0
msg_no_boot_partition db "E: No boot partition", 0x0D, 0x0A, 0
msg_no_kernel db "E: Kernel not found", 0x0D, 0x0A, 0
msg_dbg db "!DEBUG!", 0x0D, 0x0A, 0

; ------------------------------------------------------------
; Структура LBA-пакета для Int 0x13 AH=0x42
; ------------------------------------------------------------
; Формат (16 байт):
;   Offset 0:  размер пакета (16)
;   Offset 2:  количество секторов для чтения
;   Offset 4:  смещение буфера (сегмент:смещение)
;   Offset 6:  сегмент буфера
;   Offset 8:  LBA (нижние 32 бита)
;   Offset 12: LBA (верхние 32 бита – для 64-битных LBA, обычно 0)
; ------------------------------------------------------------
lba_packet:
    db 0x10                  ; Размер пакета (16 байт)
    db 0x00                  ; Зарезервировано (0)
    dw 0x0001                ; Количество секторов для чтения
    dw KERNEL_LOAD_OFFSET       ; Смещение буфера
    dw KERNEL_LOAD_SEGMENT       ; Сегмент буфера
    dd 0x00000000            ; Номер начального LBA (младшие 32 бита)
    dd 0x00000000            ; Старшие 32 бита LBA (не нужны)

; ------------------------------------------------------------
; GDT (Global Descriptor Table) – минимальная для защищённого режима
; ------------------------------------------------------------
gdt_start:
    ; Нулевой дескриптор (обязателен)
    dd 0x00000000
    dd 0x00000000

; Дескриптор сегмента кода (32-битный, исполняемый, не системный)
gdt_code:
    dw 0xFFFF                ; Лимит (биты 0-15) – 4 ГБ
    dw 0x0000                ; База (биты 0-15)
    db 0x00                  ; База (биты 16-23)
    db 10011010b             ; Флаги доступа: Present, Privilege 0, Code, Readable
    db 11001111b             ; Флаги: 32-битный, гранулярность 4 КБ
    db 0x00                  ; База (биты 24-31)

; Дескриптор сегмента данных (32-битный, читаемый/записываемый)
gdt_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b             ; Present, Privilege 0, Data, Writable
    db 11001111b
    db 0x00

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1 ; Размер GDT (в байтах - 1)
    dd gdt_start               ; Адрес начала GDT

; Селекторы для загрузки в CS и другие сегментные регистры
CODE_SEG equ gdt_code - gdt_start
DATA_SEG equ gdt_data - gdt_start

; ------------------------------------------------------------
; 32-битный защищённый режим
; ------------------------------------------------------------
[BITS 32]

%macro debug_P 0
    ; --- ОТЛАДКА: выводим символ 'P' в левый верхний угол ---
    mov word [0xB8000], 0x0F50   ; 'P' белым по чёрному
%endmacro

protected_mode:

    ; Загружаем селекторы данных во все сегментные регистры
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Настраиваем стек (растёт вниз)
    mov ebp, STACK_TOP
    mov esp, ebp

    lea eax, datetime
    push eax ; передаем аргументом адрес сигнатуры

    ; настройка paging
    jmp setup_paging

; ------------------------------------------------------------
; Настройка paging достаточного для загрузки ядра
; ------------------------------------------------------------
setup_paging:
    ; Адреса структур
    ; (0x10000 это 64 кб) 
    %define PAGE_DIR_ADDR   0x60000
    %define FLAGS 0x83
    %define BLOCKS_COUNT 16
    ; смещение нуля
    %define VIRTUAL_START 0xC0000000

    ; ----- Включить поддержку 4 МБ страниц (PSE) -----
    mov eax, cr4
    or eax, 0x00000010          ; бит PSE (4-й бит)
    mov cr4, eax
    ; потом в ядре выключим поддержку 4 мб страниц

    ; Очистим каталог
    mov edi, PAGE_DIR_ADDR
    mov ecx, 1024 ; сколько uint32_t 
    xor eax, eax ; зануляем eax (чем заполняем)
    rep stosd

    ; Заполняем таблицу страниц: identity mapping для первых 64 МБ
    mov edi, PAGE_DIR_ADDR
    mov ecx, BLOCKS_COUNT
    mov eax, FLAGS   ; присутствует, чтение/запись, супервизор, без таблиц
.fill_table_low:
    stosd
    add eax, 0x400000       ; следующая страница (+4 мб)
    loop .fill_table_low ; пока ecx не ноль, jmp на метку

    ; аналогично
    ; +0xC0000000
    lea edi, [PAGE_DIR_ADDR+0x300*4]
    mov ecx, BLOCKS_COUNT
    mov eax, FLAGS
.fill_table_high:
    stosd
    add eax, 0x400000
    loop .fill_table_high

    ; Загружаем адрес каталога в CR3
    mov eax, PAGE_DIR_ADDR
    mov cr3, eax

    ; Включаем бит PG в CR0
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; готовим стек к полному переносу ядра наверх
    lea esp, [esp + VIRTUAL_START]

    ; Делаем прыжок на высокий адрес ядра
    jmp 0xC0010000

hang:
    hlt
    jmp hang