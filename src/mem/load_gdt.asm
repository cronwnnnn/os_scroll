[GLOBAL load_gdt]
load_gdt:
    mov eax, [esp + 4]
    lgdt[eax]
    ; 段描述符的机制是gdt基址寄存器 + 描述符的值（偏移地址） = gdt的地址
    ; ds:50  并使用得到的gdt的基地址+ 偏移(50)  来访问虚拟地址 
    mov ax, 0x10           ; 选择内核数据段,8字节是一个段描述符,所以0x10就是第三个段描述符，第一个为空描述符
    mov ds, ax
    mov es, ax
    mov fs, ax  
    mov ss, ax   

    mov ax, 0x18
    mov gs, ax

    jmp 0x08:.flush     ;set code segment
.flush:
    ret