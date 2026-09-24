[BITS 16]
[ORG 0x7C00]


STAGE2_LOAD_SEGMENT equ 0x0000
STAGE2_LOAD_OFFSET equ 0x7E00
STAGE2_LBA_START equ 1 ; номер сектора, с которого начинается stage 2
STAGE2_SECTORS_COUNT equ 64 ; количество секторов под stage 2

; ------------------------------------------------------------
; Точка входа
; ------------------------------------------------------------
start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov [boot_drive], dl ; Сохраняем номер диска, с которого нас загрузили (из DL)

    mov ax, 0x0003
    int 0x10 ; переходим в текстовый режим 3

    mov si, msg_loading
    call print_string_16

    ; Проверяем, поддерживает ли BIOS расширения LBA
    ; Вызов: AH=0x41, BX=0x55AA, DL=диск
    mov ah, 0x41
    mov bx, 0x55AA
    mov dl, [boot_drive]
    int 0x13
    jc .no_lba               ; Если CF=1 – LBA не поддерживается
    cmp bx, 0xAA55           ; Расширение должно вернуть 0xAA55 в BX
    jne .no_lba

    ; читаем

    mov si, lba_packet       ; DS:SI указывает на структуру пакета
    mov ah, 0x42
    mov dl, [boot_drive]
    int 0x13
    jc disk_error
    
    jmp .lba_ok

.no_lba:
    ; Если LBA не поддерживается – падаем с сообщением
    mov si, msg_no_lba
    call print_string_16
    jmp $

.lba_ok:
    mov dl, [boot_drive]
    jmp STAGE2_LOAD_OFFSET ; передаем управление stage 2

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

disk_error:
    mov si, msg_disk_error
    call print_string_16
    jmp $                    ; Бесконечный цикл

; ------------------------------------------------------------
; Данные (в реальном режиме)
; ------------------------------------------------------------
boot_drive db 0
msg_loading db "Stage 1 loaded", 0x0D, 0x0A, 0
msg_no_lba db "E: LBA not supported", 0x0D, 0x0A, 0
msg_disk_error db "E: Disk read error", 0x0D, 0x0A, 0

; ------------------------------------------------------------
; Структура LBA-пакета для Int 0x13 AH=0x42
; ------------------------------------------------------------
lba_packet:
    db 0x10                  ; Размер пакета (16 байт)
    db 0x00                  ; Зарезервировано
    dw STAGE2_SECTORS_COUNT   ; Количество секторов для чтения
    dw STAGE2_LOAD_OFFSET    ; Смещение буфера
    dw STAGE2_LOAD_SEGMENT   ; Сегмент буфера
    dd STAGE2_LBA_START      ; Номер начального LBA (младшие 32 бита)
    dd 0x00000000            ; Старшие 32 бита LBA (не нужны)