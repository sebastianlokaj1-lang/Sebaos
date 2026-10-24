; ============================================================
;  SebaOS - stage 1 (boot sektor, 512 B)
;  Nacte stage 2 (na 0x7E00) a jadro (na 0x10000) pomoci INT 13h
;  rozsirene cteni (LBA) a skoci na stage 2.
; ============================================================
[bits 16]
[org 0x7C00]

%ifndef KSECT
%define KSECT 128
%endif
%define STAGE2_SECT 4

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti
    mov [bootdrv], dl

    mov si, msg_load
    call print

    ; --- stage 2 -> 0x0000:0x7E00 (LBA 1..4)
    mov si, dap_s2
    mov dl, [bootdrv]
    mov ah, 0x42
    int 0x13
    jc fail

    ; --- jadro -> 0x1000:0000 (fyzicky 0x10000), po 32 sektorech
    mov word [dap_seg], 0x1000
    mov word [dap_off], 0
    mov dword [dap_lba], 1 + STAGE2_SECT
    mov word [remaining], KSECT
.loop:
    mov ax, [remaining]
    test ax, ax
    jz .done
    cmp ax, 32
    jbe .n
    mov ax, 32
.n:
    mov [dap_cnt], ax
    mov si, dap
    mov dl, [bootdrv]
    mov ah, 0x42
    int 0x13
    jc fail
    mov ax, [dap_cnt]
    sub [remaining], ax
    add [dap_lba], ax
    shl ax, 5                 ; sektory * 512 / 16 = odstavce
    add [dap_seg], ax
    jmp .loop
.done:
    mov dl, [bootdrv]
    jmp 0x0000:0x7E00

fail:
    mov si, msg_fail
    call print
.h: hlt
    jmp .h

print:
    lodsb
    test al, al
    jz .r
    mov ah, 0x0E
    xor bx, bx
    int 0x10
    jmp print
.r: ret

msg_load db 'SebaOS: nacitam...', 13, 10, 0
msg_fail db 'Chyba cteni disku!', 0
bootdrv  db 0
remaining dw 0

dap_s2:
    db 0x10, 0
    dw STAGE2_SECT
    dw 0x7E00
    dw 0x0000
    dq 1

dap:
    db 0x10, 0
dap_cnt: dw 0
dap_off: dw 0
dap_seg: dw 0
dap_lba: dq 0

times 510-($-$$) db 0
dw 0xAA55
