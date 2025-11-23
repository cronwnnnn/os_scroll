;loader------------
%include"boot/boot.inc"

SECTION loader vstart=LOADER_BASE_ADDR

loader_entry:
    jmp loader_start

;***************************** data section  **********************************
cursor_row: dd 0
cursor_col: dd 0

setup_protection_mode_message:
  db "setup protection mode ... "

setup_protection_mode_message_length equ $ - setup_protection_mode_message

setup_page_message:
  db "setup page ... "
  db 0

init_kernel_message:
  db "init kernel ... "
  db 0

panic_message:
  db "PANIC!"
  db 0

;gdt
GDT_BASE:
    dd 0x00000000
    dd 0x00000000

CODE_DESC:
    dd DESC_CODE_LOW_32
    dd DESC_CODE_HIGH_32

DATA_DESC:
    dd DESC_DATA_LOW_32
    dd DESC_DATA_HIGH_32

VEDIO_DESC:
    dd DESC_VIDEO_LOW_32
    dd DESC_VIDEO_HIGH_32

;dummy for 60 free gdt entry
times 60 dq 0 

GDT_SIZE equ $ - GDT_BASE
GDT_LIMIT equ GDT_SIZE - 1

gdt_ptr:
    dw GDT_LIMIT
    dd GDT_BASE

;*************************** 16-bits real mode ********************************;
loader_start:
    call clear_screen
    call setup_protection_mode

    jmp $

clear_screen:
    mov byte ah, 0x06
    mov byte al, 0x00
    mov byte bh, 0x07
    ; start (0, 0)
    mov byte cl, 0x00
    mov byte ch, 0x00
    ; end: (dl, dh) = (x:79, y:24)
    mov byte dl, 0x4f
    mov byte dh, 0x18

    int 0x10
    ret

    ; args:
    ;  - ax message
    ;  - cx length
    ;  - dx position
print_message_real_mode:
    mov bp, ax
    mov ah, 0x13  ; int num
    mov al, 0x01
    mov bh, 0x00  ; page number
    mov bl, 0x07
    int 0x10
    ret

setup_protection_mode:
    mov ax, setup_protection_mode_message
    mov cx, setup_protection_mode_message_length
    mov dx, 0x00
    call print_message_real_mode

    ;enable A20
    in al, 0x92
    or al, 0000_0010b
    out 0x92, al

    ;load gdt
    lgdt [gdt_ptr]

    ;set cr0 PE
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax

    ;set long jump
    jmp dword SELECTOR_CODE:protection_mode_entry

    
;-------------------32bits protection_mode -------------------
[bits 32]
protection_mode_entry:
  mov ax, SELECTOR_DATA
  mov ds, ax
  mov ss, ax
  mov es, ax

  ;set vedio segment
  mov ax, SELECTOR_VIDEO
  mov gs, ax

  push setup_protection_mode_message_length
  call mov_cursor
  add esp, 4

  call print_OK
  call print_newline

  ;set up page
  push setup_page_message
  call print_message
  add esp, 4

  call setup_page

  call print_OK
  call print_newline

  ;init kernel
  push init_kernel_message
  call print_message
  add esp, 4

  call init_kernel ; jump to kernel entry, nerver return!!

  push panic_message 
  call print_message
  add esp, 4

  jmp $

  ret


; args: address of string
print_message:
  push ebp
  mov ebp, esp
  push esi
  mov esi, [ebp+8]

  call get_current_cursor_addr
  mov ecx, eax ; current blank position
  mov eax, 0 ; string index

.print_start:
  mov byte dl, [esi + eax]
  cmp dl, 0
  je .print_end
  mov byte [gs:ecx], dl
  mov byte [gs:ecx+1], 0x7
  add eax, 1
  add ecx, 2
  jmp .print_start

.print_end:
  push eax
  call mov_cursor ; 移动光标到字符末尾
  add esp, 4

  pop esi
  pop ebp
  ret


print_OK:
  call get_current_cursor_addr
  mov byte [gs:eax], '['
  mov byte [gs:eax+1], 0x07
  mov byte [gs:eax+2], 'O'
  mov byte [gs:eax+3], 0010b
  mov byte [gs:eax+4], 'K'
  mov byte [gs:eax+5], 0010b
  mov byte [gs:eax+6], ']'
  mov byte [gs:eax+7], 0x07

  push 4 ; 移动4个字符 * 2字节/字符
  call mov_cursor
  add esp, 4
  ret


print_newline:
  mov ecx, [cursor_row]
  inc ecx
  mov dword [cursor_row], ecx
  mov dword [cursor_col], 0
  push 0
  call mov_cursor
  add esp, 4

  ret


; args: none, outputs: byte of cursor offset
get_current_cursor_addr:
  mov ecx, [cursor_row] ; 得到的是字符偏移
  mov edx, [cursor_col]

  mov eax, ecx
  shl eax, 4
  shl ecx, 6
  add eax, ecx
  add eax, edx ; 一行宽80个字符，因此为 80 * row + col

  shl eax, 1 ; *2,是真正的字节偏移
  ret


; args: string length in eax , not byte length
mov_cursor:
  push ebp
  mov ebp, esp

  mov eax, [ebp + 8] ; 从栈上获取参数
  mov ecx, [cursor_row]
  mov edx, [cursor_col]
  add edx, eax

.check_new_line:
  cmp edx, 80
  jle .do_mov
  inc ecx
  sub edx, 80
  jmp .check_new_line

.do_mov:
  mov dword [cursor_row], ecx
  mov dword [cursor_col], edx

  call get_current_cursor_addr
  shr eax, 1 ;转为字符单位
  mov ecx, eax

  ;进行写入操作
  mov al, 14
  mov dx, 0x3d4
  out dx, al
  mov al, ch
  mov dx, 0x3d5
  out dx, al

  mov al, 15
  mov dx, 0x3d4
  out dx, al
  mov al, cl
  mov dx, 0x3d5
  out dx, al

  pop ebp
  ret


 ;----------------------------- set page -------------------------------
setup_page:
  ;clear mem for page
  push dword PAGE_SIZE
  push dword PAGE_DIR_PHYSICAL_ADDR
  call clear_memory
  add esp, 8

; ************* create and fill page_dir **************
.create_pde:
  mov eax, PAGE_DIR_PHYSICAL_ADDR - PAGE_SIZE
  or eax, PG_P | PG_RW_W | PG_US_U
  ;for first 1MB physical memory
  mov [PAGE_DIR_PHYSICAL_ADDR], eax
  mov [PAGE_DIR_PHYSICAL_ADDR + 768 * 4], eax
  ;for pde
  mov eax, PAGE_DIR_PHYSICAL_ADDR
  or eax, PG_P | PG_RW_W | PG_US_U
  mov [PAGE_DIR_PHYSICAL_ADDR + 769 * 4], eax
  ;otehr kernel page
  mov eax, PAGE_DIR_PHYSICAL_ADDR + PAGE_SIZE
  or eax, PG_P | PG_RW_W | PG_US_U
  mov ecx, 254
  mov edx, PAGE_DIR_PHYSICAL_ADDR + 770 * 4
.create_kernel_pde:
  mov [edx], eax
  add eax, PAGE_SIZE
  add edx, 4
  loop .create_kernel_pde
; ************* create and fill page_dir **************

;************* create and first page table**************
  mov eax, 0
  or eax, PG_P | PG_RW_W | PG_US_U
  mov ecx, 256
  mov edx, PAGE_DIR_PHYSICAL_ADDR - PAGE_SIZE 
.create_pte:
  mov [edx], eax
  add eax, PAGE_SIZE
  add edx, 4
  loop .create_pte
;************* create and first page table**************


  call enable_page
  ret





enable_page:
  ; 将页目录表的物理地址加载到 cr3
  mov eax, PAGE_DIR_PHYSICAL_ADDR
  mov cr3, eax

  ; 开启分页 (CR0 寄存器的 PG 位置 1)
  mov eax, cr0
  or eax, 0x80000000
  mov cr0, eax

  ; 立即跳转到高地址继续执行。
  ; 这里的 jmp 会刷新指令预取队列，并使 EIP 指向高地址。
  ; high_addr_continue 标签的物理地址在低1MB，但由于高半核映射，
  ; 它的虚拟地址也在 0xC0000000 之上。
  jmp dword SELECTOR_CODE:high_addr_continue

high_addr_continue:
  ; 从这里开始，我们已经在虚拟地址空间 0xC0000000+ 运行了

  ; 更新 GDT 指针，使其指向虚拟地址
  add dword [gdt_ptr + 2], 0xC0000000
  lgdt [gdt_ptr]

  ; 更新段寄存器。注意：cs 已经在 jmp 时更新
  mov ax, SELECTOR_DATA
  mov ds, ax
  mov es, ax
  mov ss, ax

  ; 更新视频段选择子
  mov ax, SELECTOR_VIDEO
  mov gs, ax

  ret



; set contineous kernel pages mapping for virtual memory -> physical memory
;  - arg 1: virtual addr start (page aligned)
;  - arg 2: physical addr start (page aligned)
;  - arg 3: num of pages
;
; caller must make sure the physical memory starting from arg2 is free to use
set_page_mapping:
  push ebp
  mov ebp, esp
  push esi
  push edi
  push ebx


  mov esi, [ebp + 8] ; virtual addr start
  mov edi, [ebp + 12] ; physical addr start
  mov ecx, [ebp + 16] ; num of pages
  shr esi, 12
  or edi, PG_US_U | PG_RW_W | PG_P
.map_next_page:
  mov [PAGE_TAB_VIRTUAL_ADDR + esi * 4], edi
  add esi, 1
  add edi, PAGE_SIZE
  loop .map_next_page

  ; flush TLB lest the new mapping not take effect
  mov eax, cr3
  mov cr3, eax

  pop ebx
  pop edi
  pop esi
  pop ebp
  ret


; clear contineous kernel pages mapping for virtual memory
;  - arg 1: virtual addr start (page aligned)
;  - arg 2: num of pages
clear_page_mapping:
  push ebp
  mov ebp, esp
  push esi

  mov esi, [ebp + 8] ; virtual addr start
  mov ecx, [ebp + 12] ; num of pages
  shr esi, 12
.clear_next_page:
  mov dword [PAGE_TAB_VIRTUAL_ADDR + esi * 4], 0
  add esi, 1
  loop .clear_next_page

  mov eax, cr3
  mov cr3, eax

  pop esi
  pop ebp
  ret


;------------------------- init kernel -----------------------------
init_kernel:
  call allocate_pages_for_kernel
  call load_hd_kernel_image
  call do_load_kernel

  ; init floating point unit before entering the kernel
  finit

  ; move stack to 0xF0000000
  mov esp, KERNEL_STACK_TOP - 16
  mov ebp, esp

  ; let's jump to kernel entry :)
  jmp eax
  ret




allocate_pages_for_kernel:
  ;allocate pages for kernel_bin
  mov ecx, KERNEL_BIN_MAX_SIZE
  shr ecx, 12
  push ecx
  push KERNEL_BIN_LOAD_PHYSICAL_ADDR
  push KERNEL_BIN_LOAD_VIRTUAL_ADDR
  call set_page_mapping
  add esp, 12

  ;allocate pages for kernel section
  mov ecx, KERNEL_SIZE_MAX
  shr ecx, 12
  push ecx
  push KERNEL_PHYSICAL_ADDR_START
  push KERNEL_VIRTUAL_ADDR_START
  call set_page_mapping
  add esp, 12

  ;allocate pages for kernel stack
  mov ecx, 1
  push ecx
  push KERNEL_STACK_PHYSICAL_ADDR
  push KERNEL_STACK_TOP - PAGE_SIZE
  call set_page_mapping
  add esp, 12

  ret
  

load_hd_kernel_image:
  ; three paraments for disk_addr, mem_addr, read_length
    mov eax, KERNEL_START_SECTOR
    mov ebx, KERNEL_BIN_LOAD_VIRTUAL_ADDR
    mov cx, KERNEL_SECTORS
    call read_disk_32
    ret


; load kernel sections to memory
; return the kernel entry point address
do_load_kernel:
  xor eax, eax
  xor ebx, ebx
  xor ecx, ecx
  xor edx, edx

  mov dx, [KERNEL_BIN_LOAD_VIRTUAL_ADDR + 42]   ; e_phentsize
  mov ebx, [KERNEL_BIN_LOAD_VIRTUAL_ADDR + 28]  ; e_phoff
  add ebx, KERNEL_BIN_LOAD_VIRTUAL_ADDR
  mov cx, [KERNEL_BIN_LOAD_VIRTUAL_ADDR + 44]   ; e_phnum

.load_each_segment:
  ; only load loadable section
  cmp byte [ebx + 0], 1
  jne .next_program_header

  push dword [ebx + 16] ; p_filesz
  mov eax, [ebx + 4]  ; p_offset
  add eax, KERNEL_BIN_LOAD_VIRTUAL_ADDR
  push eax              ; source addr
  push dword [ebx + 8]  ; dst addr
  call memmory_copy
  add esp, 12

.next_program_header:
  add ebx, edx
  loop .load_each_segment

  ; return the kernel entry address
  mov dword eax, [KERNEL_BIN_LOAD_VIRTUAL_ADDR + 24]
  ret



;------------------------- init kernel -----------------------------

memmory_copy:
  push ebp
  mov ebp, esp
  push esi
  push edi
  push ecx

  mov edi, [ebp + 8]  ; dest addr
  mov esi, [ebp + 12] ; src addr
  mov ecx, [ebp + 16] ; length
  rep movsb

  pop ecx
  pop edi
  pop esi
  pop ebp
  ret

clear_memory:
  push ebp
  mov ebp, esp
  push ecx

  mov eax, [ebp + 8]   ; start addr
  mov ecx, [ebp + 12]  ; size
.clear_byte:
  mov byte [eax], 0
  add eax, 1
  loop .clear_byte

  pop ecx
  pop ebp
  ret


  ; read data from disk, in 32-bit mode
read_disk_32:
  mov esi, eax
  mov edi, ecx

  ; sector count
  mov dx, 0x01f2
  mov al, cl
  out dx, al

  mov eax, esi

  ; LBA low
  mov dx, 0x1f3
  out dx, al

  ; LBA mid
  shr eax, 8
  mov dx, 0x1f4
  out dx, al

  ; LBA high
  shr eax, 8
  mov dx, 0x1f5
  out dx, al

  ; device reg: LBA[24:28]
  shr eax, 8
  and al, 0x0f

  or al, 0xe0  ; 0x1110, LBA mode
  mov dx, 0x1f6
  out dx, al

  ; command reg: 0x2 read, start reading
  mov dx, 0x1f7
  mov al, 0x20
  out dx, al

.hd_not_ready:
  nop
  in al, dx
  and al, 0x88  ; bit 7 (busy), bit 3 (data ready)
  cmp al, 0x08
  jnz .hd_not_ready

  ; di = cx = sector count
  ; read 2 bytes time, so loop (sector count) * 512 / 2 times
  mov ecx, edi
  shl ecx, 8

  mov dx, 0x1f0

.go_on_read_data:
  in ax, dx
  mov [ebx], ax
  add ebx, 2
  loop .go_on_read_data

  ret

