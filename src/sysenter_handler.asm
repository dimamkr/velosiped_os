global sysenter_handler_entry

extern sysenter_dispatcher

section .text

align 4096
sysenter_handler_entry:
    push edx ; сохраняем eip
    push ecx ; сохраняем esp

    ; вызываем обработчик sysenter
    ; аргументы передаются через пользовательский стек

    push eax

    mov eax, sysenter_dispatcher

    sti ; здесь уже разрешаем прерывания
    call eax

    add esp, 4

    ; возвращаем все
    ; не проверяем адреса, так как page fault возникнет в ring3

    pop ecx
    pop edx

    sysexit