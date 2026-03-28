[GLOBAL reload_idt]
[GLOBAL interrupt_exit]
; 加载IDT表
reload_idt:
  mov eax, [esp + 4]  ; arg 1
  lidt [eax]
  ret

;声明所有中断服务程序的入口点，每个macro包裹的相当于一个函数，
;可以通过isr?来进行调用,每次使用DEFINE_ISR_NOERRCODE相当于复制了一遍里面的汇编代码
%macro DEFINE_ISR_ERRCODE 1
    [global isr%1]
    isr%1:
        cli ; disable interrupts
        push byte %1
        jmp isr_common_stub
%endmacro

%macro DEFINE_ISR_NO_ERRCODE 1
    [global isr%1]
    isr%1:
        cli ; disable interrupts
        push byte 0
        push byte %1
        jmp isr_common_stub
%endmacro

; ********************************* exceptions ************************************** ;
DEFINE_ISR_NO_ERRCODE  0
DEFINE_ISR_NO_ERRCODE  1
DEFINE_ISR_NO_ERRCODE  2
DEFINE_ISR_NO_ERRCODE  3
DEFINE_ISR_NO_ERRCODE  4
DEFINE_ISR_NO_ERRCODE  5
DEFINE_ISR_NO_ERRCODE  6
DEFINE_ISR_NO_ERRCODE  7
DEFINE_ISR_ERRCODE    8
DEFINE_ISR_NO_ERRCODE  9
DEFINE_ISR_ERRCODE    10
DEFINE_ISR_ERRCODE    11
DEFINE_ISR_ERRCODE    12
DEFINE_ISR_ERRCODE    13
DEFINE_ISR_ERRCODE    14
DEFINE_ISR_NO_ERRCODE  15
DEFINE_ISR_NO_ERRCODE  16
DEFINE_ISR_NO_ERRCODE  17
DEFINE_ISR_NO_ERRCODE  18
DEFINE_ISR_NO_ERRCODE  19
DEFINE_ISR_NO_ERRCODE  20
DEFINE_ISR_NO_ERRCODE  21
DEFINE_ISR_NO_ERRCODE  22
DEFINE_ISR_NO_ERRCODE  23
DEFINE_ISR_NO_ERRCODE  24
DEFINE_ISR_NO_ERRCODE  25
DEFINE_ISR_NO_ERRCODE  26
DEFINE_ISR_NO_ERRCODE  27
DEFINE_ISR_NO_ERRCODE  28
DEFINE_ISR_NO_ERRCODE  29
DEFINE_ISR_NO_ERRCODE  30
DEFINE_ISR_NO_ERRCODE  31

; ********************************* external interrupts ************************************** ;
DEFINE_ISR_NO_ERRCODE   32
DEFINE_ISR_NO_ERRCODE   33
DEFINE_ISR_NO_ERRCODE   34
DEFINE_ISR_NO_ERRCODE   35
DEFINE_ISR_NO_ERRCODE   36
DEFINE_ISR_NO_ERRCODE   37
DEFINE_ISR_NO_ERRCODE   38
DEFINE_ISR_NO_ERRCODE   39
DEFINE_ISR_NO_ERRCODE   40
DEFINE_ISR_NO_ERRCODE   41
DEFINE_ISR_NO_ERRCODE   42
DEFINE_ISR_NO_ERRCODE   43
DEFINE_ISR_NO_ERRCODE   44
DEFINE_ISR_NO_ERRCODE   45
DEFINE_ISR_NO_ERRCODE   46
DEFINE_ISR_NO_ERRCODE   47



[EXTERN isr_handler]
isr_common_stub:
    ; save common registers
    pusha

    ; save original data segment
    mov ax, ds
    push eax

    ; use kernel data selectcor
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call isr_handler

interrupt_exit:
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8 ; remove error code and interrupt number from stack
    sti
    iret    ; pop cs, eip, eflags, user_ss, and user_esp by processor