; ============================================================
;  SebaOS - stage 2 (real mode)
;   1) zkopiruje 8x16 font z BIOSu na 0x9000
;   2) najde a zapne VESA grafiku 32bpp s linearnim framebufferem
;      (info o modu zustane na 0x600)
;   3) zapne A20, GDT, prepne do 32bitoveho protected modu
;   4) skoci do jadra na 0x10000
; ============================================================
[bits 16]
[org 0x7E00]

%define VBEINFO   0x6000
%define MODEINFO  0x6400
%define BOOTINFO  0x600
%define FONTDST   0x9000

stage2:
    xor ax, ax
    mov ds, ax
    mov es, ax

    ; --- 0) velikost RAM v KB -> 0x5F0 (INT 15h AX=E801)
    mov dword [0x5F0], 65536
    xor cx, cx
    xor dx, dx
    mov ax, 0xE801
    int 0x15
    jc .noram
    cmp ah, 0x86
    je .noram
    cmp ah, 0x80
    je .noram
    test ax, ax
    jnz .ramok
    mov ax, cx
    mov bx, dx
.ramok:
    movzx eax, ax
    movzx ebx, bx
    shl ebx, 6
    add eax, ebx
    add eax, 1024
    mov [0x5F0], eax
.noram:
    xor ax, ax
    mov es, ax

    ; --- 1) BIOS font 8x16
    mov ax, 0x1130
    mov bh, 6
    int 0x10                  ; ES:BP -> font
    push ds
    mov ax, es
    mov ds, ax
    mov si, bp
    xor ax, ax
    mov es, ax
    mov di, FONTDST
    mov cx, 2048
    cld
    rep movsw
    pop ds
    xor ax, ax
    mov es, ax

    ; --- 2) VBE
    mov dword [VBEINFO], 'VBE2'
    mov ax, 0x4F00
    mov di, VBEINFO
    int 0x10
    cmp ax, 0x004F
    jne vbe_fail
    mov ax, [VBEINFO + 0x10]
    mov [mlseg], ax
    mov ax, [VBEINFO + 0x0E]
    mov [mloff], ax
    mov word [tp], targets
.target:
    mov bx, [tp]
    mov ax, [bx]
    test ax, ax
    jz vbe_fail
    mov ax, [mloff]
    mov [mp], ax
.mode:
    mov ax, [mlseg]
    mov fs, ax
    mov bx, [mp]
    mov cx, [fs:bx]
    cmp cx, 0xFFFF
    je .nexttarget
    add word [mp], 2
    mov [curmode], cx
    mov ax, 0x4F01
    mov di, MODEINFO
    int 0x10
    cmp ax, 0x004F
    jne .mode
    mov al, [MODEINFO]
    and al, 0x81              ; podporovany + LFB
    cmp al, 0x81
    jne .mode
    mov bx, [tp]
    mov ax, [bx]
    cmp ax, [MODEINFO + 0x12]
    jne .mode
    mov ax, [bx + 2]
    cmp ax, [MODEINFO + 0x14]
    jne .mode
    cmp byte [MODEINFO + 0x19], 32
    jne .mode
    jmp .found
.nexttarget:
    add word [tp], 4
    jmp .target
.found:
    mov ax, 0x4F02
    mov bx, [curmode]
    or bx, 0x4000             ; linearni framebuffer
    int 0x10
    cmp ax, 0x004F
    jne vbe_fail
    mov si, MODEINFO
    mov di, BOOTINFO
    mov cx, 128
    cld
    rep movsw

    ; --- 3) A20 + GDT + PM
    mov ax, 0x2401
    int 0x15                  ; BIOS A20 (muze selhat)
    in al, 0x92
    or al, 2
    and al, 0xFE
    out 0x92, al

    cli
    lgdt [gdt_desc]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    jmp 0x08:pm32

vbe_fail:
    mov si, msg_vbe
.p: lodsb
    test al, al
    jz .h
    mov ah, 0x0E
    xor bx, bx
    int 0x10
    jmp .p
.h: hlt
    jmp .h

[bits 32]
pm32:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x3F0000
    mov eax, 0x10000
    jmp eax

[bits 16]
msg_vbe db 'SebaOS: VESA grafika 32bpp neni k dispozici.', 0
mlseg dw 0
mloff dw 0
mp    dw 0
tp    dw 0
curmode dw 0
targets:
    dw 800, 600
    dw 1024, 768
    dw 640, 480
    dw 0, 0

align 8
gdt:
    dq 0
    dq 0x00CF9A000000FFFF     ; 0x08 kod
    dq 0x00CF92000000FFFF     ; 0x10 data
gdt_end:
gdt_desc:
    dw gdt_end - gdt - 1
    dd gdt

times 2048-($-$$) db 0
