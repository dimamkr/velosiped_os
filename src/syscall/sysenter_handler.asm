global sysenter_handler_entry

extern sysenter_dispatcher
extern current_task
extern _sysenter_system_var

section .text

align 4096
sysenter_handler_entry:
    ; грузим стек
    ; TODO: подумать
    mov [_sysenter_system_var], eax
    mov eax, [current_task]
    mov esp, [eax + 16]
    add esp, [eax + 20]
    mov eax, [_sysenter_system_var]

    push edx ; сохраняем eip
    push ecx ; сохраняем esp

    ; вызываем обработчик sysenter
    ; аргументы передаются через пользовательский стек

    push eax

    sti ; здесь уже разрешаем прерывания

    mov eax, sysenter_dispatcher
    call eax

    add esp, 4

    ; возвращаем все
    ; не проверяем адреса, так как page fault возникнет в ring3

    pop ecx
    pop edx

    sysexit