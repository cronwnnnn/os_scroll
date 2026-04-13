[GLOBAL cpu_idle]
[GLOBAL context_switch]
[GLOBAL switch_to_user_mode]
[GLOBAL resume_thread]

cpu_idle:
    hlt
    ret

context_switch:
    push eax
    push ecx
    push edx
    push ebx
    push ebp
    push esi
    push edi
    ; return address + 7 reg are 32bytes, hence old thread address is esp+32
    ; save old thread's esp
    mov eax, [esp + 32]
    mov [eax], esp

    ; get new thread's address and set esp to jump new thread's esp
    mov eax, [esp + 36]
    mov esp, [eax]

resume_thread:
    ;将当前stack的寄存器弹出，并使用ret跳转至当前thread的运行函数
    pop edi
    pop esi
    pop ebp
    pop ebx
    pop edx
    pop ecx
    pop eax
    
    sti
    ret