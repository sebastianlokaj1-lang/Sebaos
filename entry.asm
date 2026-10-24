[bits 32]
section .text.entry
global _start
extern kernel_main
extern __bss_start
extern __bss_end
_start:
    mov esp, 0x3F0000
    mov edi, __bss_start
    mov ecx, __bss_end
    sub ecx, edi
    shr ecx, 2
    xor eax, eax
    cld
    rep stosd
    fninit
    call kernel_main
.h: cli
    hlt
    jmp .h
