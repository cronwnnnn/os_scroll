[GLOBAL get_ebp]
get_esp:
    mov eax, esp
    ret

[GLOBAL get_eflags]
get_eflags:
    pushf
    pop eax
    ret

[GLOBAL set_eflags]
set_eflags:
    push dword [esp + 4]
    popf
    ret
