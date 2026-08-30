[Global paging_load_directory]
[Global enable_paging]       

; принимает указатель на директорию
paging_load_directory:                
    mov eax, [esp+4]            ; Load the address of the page directory into EAX. (+4 из-за адреса возврата)
    mov cr3, eax                ; Load the page directory base address into CR3.                   
    ret                         


; устанавливает 31 бит в cr0 для включения paging
enable_paging:
    mov eax, cr0                
    or eax, 0x80000000          ; Set the paging bit (bit 31) in EAX.
    mov cr0, eax                               
    ret                         