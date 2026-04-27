[GLOBAL read_disk]

read_disk:
    push ebp
    mov ebp, esp
    push edi
    push esi
    push edx
    push ebx

    mov ebx, [ebp + 8]   ; buffer
    mov eax, [ebp + 12]  ; lba low/mid/high (32位)
    mov ecx, [ebp + 16]  ; sector count

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

.read_sectors:
    mov dx, 0x1f7
.hd_not_ready:
    nop
    in al, dx
    and al, 0x88  ; bit 7 (busy), bit 3 (data ready)
    cmp al, 0x08
    jnz .hd_not_ready

    mov ecx, 256
    mov dx, 0x1f0
.go_on_read_data:
    in ax, dx
    mov [ebx], ax
    add ebx, 2
    loop .go_on_read_data

    dec edi
    jnz .read_sectors

    pop ebx
    pop edx
    pop esi
    pop edi
    pop ebp
    ret

