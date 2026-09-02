; Monarch OS bootstrap for Limine/Multiboot2.
; Limine enters here in 32-bit protected mode.

[BITS 32]

MULTIBOOT2_HEADER_MAGIC equ 0xE85250D6
MULTIBOOT2_ARCH_I386    equ 0

section .multiboot
align 8
header_start:
    dd MULTIBOOT2_HEADER_MAGIC
    dd MULTIBOOT2_ARCH_I386
    dd header_end - header_start
    dd -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT2_ARCH_I386 + (header_end - header_start))

align 8
    ; Optional framebuffer request tag: 800x600x32 linear framebuffer.
    ; The bootloader may choose a compatible fallback if unavailable.
    dw 5
    dw 0
    dd 20
    dd 800
    dd 600
    dd 32

align 8
    ; Required end tag: type = 0, flags = 0, size = 8
    dw 0
    dw 0
    dd 8
header_end:

section .text
global _start
extern kernel

_start:
    cli
    mov esp, stack_top
    xor ebp, ebp

    ; C ABI: kernel(uint32_t magic, uintptr_t info)
    push ebx
    push eax
    call kernel

.hang:
    hlt
    jmp .hang

; void gdtload(uint32_t descriptor_address)
global gdtload
gdtload:
    mov eax, [esp + 4]
    lgdt [eax]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:.flush
.flush:
    ret

; void idtload(uint32_t descriptor_address)
global idtload
idtload:
    mov eax, [esp + 4]
    lidt [eax]
    ret

; void tssload(uint32_t selector)
global tssload
tssload:
    mov ax, [esp + 4]
    ltr ax
    ret

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
