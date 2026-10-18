[GLOBAL _sysenter6]

; int _sysenter6(int syscall_num, int arg0, int arg1, int arg2, int arg3, int arg4, int arg5)
_sysenter6:
    add esp, 4 ; esp на syscall_num
    mov eax, [esp] ; eax = syscall_num;
    add esp, 4 ; esp теперь на начале массива аргументов
    mov ecx, esp ; usr esp для sysenter (на начале аргументов)
    mov edx, .after_sysenter ; адрес возврата для sysenter

    sysenter

.after_sysenter:
    sub esp, 8 ; восстанавливаем esp
    ret ; через eax возвращается значение