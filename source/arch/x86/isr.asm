; x86 interrupt and IRQ stubs for Monarch.
; The common stub builds a `struct registers` frame and calls C.

[BITS 32]

extern interrupt_dispatch
extern syscallgate
extern threadsysstack

global syscallentry

global isr0
global isr1
global isr2
global isr3
global isr4
global isr5
global isr6
global isr7
global isr8
global isr9
global isr10
global isr11
global isr12
global isr13
global isr14
global isr15
global isr16
global isr17
global isr18
global isr19
global isr20
global isr21
global isr22
global isr23
global isr24
global isr25
global isr26
global isr27
global isr28
global isr29
global isr30
global isr31

global irq0
global irq1
global irq2
global irq3
global irq4
global irq5
global irq6
global irq7
global irq8
global irq9
global irq10
global irq11
global irq12
global irq13
global irq14
global irq15

; int 0x80 syscall ABI:
; eax = syscall number
; ebx, ecx, edx, esi = arguments 0..3
; eax = return value
syscallentry:
    push ds
    push es
    push fs
    push gs

    ; Preserve eax: it contains the syscall number.
    ; Loading segment selectors through ax would otherwise corrupt it.
    push eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    pop eax

    ; Preserve edi too.  C code often keeps local variables such as argv in
    ; callee-saved registers, and our syscall wrapper does not pass edi as an
    ; argument.  The syscall return value is eax; every other user register we
    ; touch here must survive the software interrupt.
    push edi

    ; Save syscall arguments on the interrupted stack.
    ; If the interrupted code was running on a process stack, we then copy
    ; these arguments to the thread's kernel syscall stack before entering C.
    push esi
    push edx
    push ecx
    push ebx
    push eax
    mov edi, esp

    cld
    call threadsysstack
    test eax, eax
    jz .same_stack

    ; Switch to the kernel syscall stack recorded by usercall().  This is a
    ; temporary ring-0 substitute for the future TSS ring-transition stack:
    ; blocking syscalls such as sleep/exit must not enter the scheduler while
    ; ESP points into a process-private user stack.
    mov esp, eax
    push dword [edi + 16]
    push dword [edi + 12]
    push dword [edi + 8]
    push dword [edi + 4]
    push dword [edi]
    call syscallgate
    add esp, 20

    mov esp, edi
    add esp, 20
    pop edi
    pop gs
    pop fs
    pop es
    pop ds
    iretd

.same_stack:
    call syscallgate
    add esp, 20
    pop edi

    pop gs
    pop fs
    pop es
    pop ds
    iretd


%macro ISR_NOERR 1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERR 1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

%macro IRQ 2
irq%1:
    push dword 0
    push dword %2
    jmp isr_common
%endmacro

ISR_NOERR 0
ISR_NOERR 1
ISR_NOERR 2
ISR_NOERR 3
ISR_NOERR 4
ISR_NOERR 5
ISR_NOERR 6
ISR_NOERR 7
ISR_ERR   8
ISR_NOERR 9
ISR_ERR   10
ISR_ERR   11
ISR_ERR   12
ISR_ERR   13
ISR_ERR   14
ISR_NOERR 15
ISR_NOERR 16
ISR_ERR   17
ISR_NOERR 18
ISR_NOERR 19
ISR_NOERR 20
ISR_NOERR 21
ISR_NOERR 22
ISR_NOERR 23
ISR_NOERR 24
ISR_NOERR 25
ISR_NOERR 26
ISR_NOERR 27
ISR_NOERR 28
ISR_NOERR 29
ISR_NOERR 30
ISR_NOERR 31

IRQ 0, 32
IRQ 1, 33
IRQ 2, 34
IRQ 3, 35
IRQ 4, 36
IRQ 5, 37
IRQ 6, 38
IRQ 7, 39
IRQ 8, 40
IRQ 9, 41
IRQ 10, 42
IRQ 11, 43
IRQ 12, 44
IRQ 13, 45
IRQ 14, 46
IRQ 15, 47

isr_common:
    pusha

    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    cld
    call interrupt_dispatch
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    iretd
