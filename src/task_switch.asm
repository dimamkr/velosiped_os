[GLOBAL task_switch_from_isr]
[GLOBAL goto_current_task]

[EXTERN current_task]
[EXTERN tasks]
[EXTERN to_destroy_accumulator]

[EXTERN task_get_next]
[EXTERN task_switch_prepare]
[EXTERN task_destroy_from_accumulator]

; --- Переключение на задачу без обработки предыдущей ---
goto_current_task:
    mov eax, [current_task] ; указатель на задачу
    mov esp, [eax + 4]

    mov eax, dword [to_destroy_accumulator]
    test eax, eax
    jnz .destroy

    jmp .goto_current_task_end

.destroy:
    call task_destroy_from_accumulator
    jmp .goto_current_task_end

.goto_current_task_end:
    pop ds                        ; восстанавливаем ds
    popa
    add esp, 8 ; пропускаем int_no/err_code
    iret

; --- Вытесняющее / кооперативное переключение (из прерывания или int 0x30) ---
task_switch_from_isr:
    ; Сохраняем текущий ESP (указывает на DS на стеке)
    mov eax, [current_task]
    mov [eax + 4], esp

    ; Выбираем следующую задачу
    call task_get_next
    test eax, eax
    jz .no_task

    ; Подготовка переключения (обновляет current_task)
    push eax
    push dword [current_task]
    call task_switch_prepare
    add esp, 8

    ; Загружаем ESP новой задачи
    mov eax, [current_task]
    mov esp, [eax + 4]

    ; Восстанавливаем контекст новой задачи
    pop ds
    popa
    add esp, 8 ; пропускаем int_no/err_code
    iret

.no_task:
    iret