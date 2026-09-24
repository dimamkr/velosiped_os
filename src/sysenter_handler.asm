global sysenter_handler_entry
global sysenter_carefully

extern sysenter_dispatcher
extern kernel_page_dict_phys
extern kernel_sysenter_stack

section .text

align 4096
sysenter_handler_entry:
    push edx ; сохраняем eip
    push ecx ; сохраняем esp
    push ebx
    push ebp

    mov ebp, esp
    mov ecx, cr3

    mov ebx, [kernel_page_dict_phys]
    mov cr3, ebx; меняем cr3 на ядерные страницы
    mov esp, [kernel_sysenter_stack] ; меняем esp на стек обработки sysenter

    push ebp ; сохраняем стек
    push ecx ; сохраняем старое cr3

    ; вызываем настоящий обработчик sysenter
    push eax

    mov eax, sysenter_dispatcher
    call eax

    add esp, 4

    ; возвращаем все
    ; не проверяем адреса, так как page fault возникнет в ring3

    pop ecx
    pop esp
    mov cr3, ecx

    pop ebp
    pop ebx
    pop ecx
    pop edx

    sti
    sysexit

sysenter_carefully:
    mov ecx, esp
    lea edx, [.after_sysenter]
    sysenter
.after_sysenter:

    ret