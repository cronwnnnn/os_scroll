; 系统调用号
SYSCALL_EXIT_NUM          equ  0
SYSCALL_FORK_NUM          equ  1
SYSCALL_EXEC_NUM          equ  2
SYSCALL_YIELD_NUM         equ  3
SYSCALL_READ_NUM          equ  4
SYSCALL_WRITE_NUM         equ  5
SYSCALL_STAT_NUM          equ  6
SYSCALL_LISTDIR_NUM       equ  7
SYSCALL_PRINT_NUM         equ  8
SYSCALL_WAIT_NUM          equ  9
SYSCALL_THREAD_EXIT_NUM   equ  10
SYSCALL_READ_CHAR_NUM     equ  11
SYSCALL_MOVE_CURSOR_NUM   equ  12


; =================这部分代码是在用户栈中执行的，还没进入内核栈，只有int 0x80触发中断后才会转至内核栈=======================
; 宏定义，传两个参数，%1用于替换第一个参数
; 第一个参数为系统调用名字，第二个为系统调用号
%macro DEFINE_SYSCALL_TRIGGER_0_PARAM 2
  [GLOBAL trigger_syscall_%1]
  trigger_syscall_%1:
    mov eax, %2
    int 0x80
    ret
%endmacro

%macro DEFINE_SYSCALL_TRIGGER_1_PARAM 2
  [GLOBAL trigger_syscall_%1]
  trigger_syscall_%1:
    ; eax 存调用号,ecx存第一个参数，后面依此类推
    ; 对于caller存储的ebx这种需要先push保存寄存器状态
    mov eax, %2
    mov ecx, [esp+4]
    int 0x80
    ret
%endmacro

%macro DEFINE_SYSCALL_TRIGGER_2_PARAM 2
  [GLOBAL trigger_syscall_%1]
  trigger_syscall_%1:
    mov eax, %2
    mov ecx, [esp+4]
    mov edx, [esp + 8]
    int 0x80
    ret
%endmacro

%macro DEFINE_SYSCALL_TRIGGER_3_PARAM 2
  [GLOBAL trigger_syscall_%1]
  trigger_syscall_%1:
    push ebx

    mov eax, %2
    mov ecx, [esp + 8]
    mov edx, [esp + 12]
    mov ebx, [esp + 16]
    int 0x80

    pop ebx
    ret
%endmacro

%macro DEFINE_SYSCALL_TRIGGER_4_PARAM 2
  [GLOBAL trigger_syscall_%1]
  trigger_syscall_%1:
    push ebx
    push esi

    mov eax, %2
    mov ecx, [esp + 12]
    mov edx, [esp + 16]
    mov ebx, [esp + 20]
    mov esi, [esp + 24]
    int 0x80

    pop esi
    pop ebx
    ret
%endmacro

%macro DEFINE_SYSCALL_TRIGGER_5_PARAM 2
  [GLOBAL trigger_syscall_%1]
  trigger_syscall_%1:
    push ebx
    push esi
    push edi

    mov eax, %2
    mov ecx, [esp + 16]
    mov edx, [esp + 20]
    mov ebx, [esp + 24]
    mov esi, [esp + 28]
    mov edi, [esp + 32]
    int 0x80

    pop edi
    pop esi
    pop ebx
    ret
%endmacro


DEFINE_SYSCALL_TRIGGER_1_PARAM exit, SYSCALL_EXIT_NUM
DEFINE_SYSCALL_TRIGGER_0_PARAM fork, SYSCALL_FORK_NUM
DEFINE_SYSCALL_TRIGGER_1_PARAM print, SYSCALL_PRINT_NUM