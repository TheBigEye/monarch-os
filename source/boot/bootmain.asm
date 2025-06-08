; Desc: Kernel entry point, stack, and Interrupt Service Routines.
;
; Notes: No warranty expressed or implied. Use at own risk.
;
; This is the kernel's entry point. We could either call main here,
; or we can use this to setup the stack or other nice stuff, like
; perhaps setting up the GDT and segments. Please note that interrupts
; are disabled at this point: More on interrupts later!

[BITS 32]

global bootmain
bootmain:
    mov esp, _sys_stack     ; This points the stack to our new stack area
    jmp stublet


; This part MUST be 4byte aligned, so we solve that issue using 'ALIGN 4', LOL
ALIGN 4
multiboot:

    ; Multiboot macros to make a few lines later more readable
    MULTIBOOT_PAGE_ALIGN	  equ 1 << 0
    MULTIBOOT_MEMORY_INFO	  equ 1 << 1
    MULTIBOOT_GRAPH_MODE      equ 1 << 2
    MULTIBOOT_HEADER_MAGIC	  equ 0x1BADB002
    MULTIBOOT_HEADER_FLAGS	  equ MULTIBOOT_PAGE_ALIGN | MULTIBOOT_MEMORY_INFO | MULTIBOOT_GRAPH_MODE
    MULTIBOOT_CHECKSUM	      equ -(MULTIBOOT_HEADER_MAGIC + MULTIBOOT_HEADER_FLAGS)
    EXTERN code, bss, end

    ; This is the GRUB Multiboot header. A boot signature
    dd MULTIBOOT_HEADER_MAGIC
    dd MULTIBOOT_HEADER_FLAGS
    dd MULTIBOOT_CHECKSUM

    ; Note: While we could enable 800x600 graphics mode here,
    ; current graphics code needs improvement first. Currently,
    ; we only support basic VGA modes without VESA features.
    ; This limits mode switching between text and 640x480,
    ; as switching back to 800x600 VESA mode is not yet implemented.

    ; Framebuffer, mode 787!
    ; dd 0, 0, 0, 0, 0
    ; dd 0 ; 0 = set graphics mode
    ; dd 800, 600, 32 ; Width, height, depth

; This is an endless loop here. Make a note of this: Later on, we
; will insert an 'extern _main', followed by 'call _main', right
; before the 'jmp $'.
stublet:
    push esp

    ; Push the incoming mulitboot headers
	push eax ; Header mutiboot magic
	push ebx ; Header pointer to info structure

    extern butterfly
    call butterfly
    jmp $


;-----------------------------------------------------------------------------;
;                          Butterfly Kernel GDT Stuff                         ;
;-----------------------------------------------------------------------------;

; This will set up our new segment registers. We need to do
; something special in order to set CS. We do what is called a
; far jump. A jump that includes a segment as well as an offset.
; This is declared in C as 'extern void gdtDoFlush();'
global gdtDoFlush
[extern descriptor_pointer]
gdtDoFlush:
    lgdt [descriptor_pointer]
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    jmp 0x08:gdt_end
gdt_end:
    ret

;-----------------------------------------------------------------------------;
;                    Butterfly Kernel IRQ and ISR Handlers                    ;
;-----------------------------------------------------------------------------;

; Defined in ISR.c
[extern ISR_handler]
[extern IRQ_handler]

;─────────────────────────────────────;
;           Common ISR Code           ;
;─────────────────────────────────────;
ISR_common_stub:
    ; 1. Save CPU state
	pusha ; Pushes edi,esi,ebp,esp,ebx,edx,ecx,eax
	mov ax, ds ; Lower 16-bits of eax = ds.
	push eax ; save the data segment descriptor
	mov ax, 0x10 ; kernel data segment descriptor
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	push esp ; registers_t *r

    ; 2. Call C handler
    cld ; C code following the sysV ABI requires DF to be clear on function entry
	call ISR_handler

    ; 3. Restore state
	pop eax
    pop eax
	mov ds, ax
	mov es, ax
	mov fs, ax
	mov gs, ax
	popa
	add esp, 8 ; Cleans up the pushed error code and pushed ISR number
	iret ; pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP

;─────────────────────────────────────;
;          Common IRQ Code            ;
;─────────────────────────────────────;
IRQ_common_stub:
    ; Identical to ISR code except for the 'call' and the 'pop ebx'
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
    call IRQ_handler ; Different than the ISR code
    pop ebx  ; Different than the ISR code
    pop ebx
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    popa
    add esp, 8
    iret

; We don't get information about which interrupt was caller
; when the handler is run, so we will need to have a different handler
; for every interrupt.
; Furthermore, some interrupts push an error code onto the stack but others
; don't, so we will push a dummy error code for those which don't, so that
; we have a consistent stack for all of them.

; First make the ISRs global
global ISR_0
global ISR_1
global ISR_2
global ISR_3
global ISR_4
global ISR_5
global ISR_6
global ISR_7
global ISR_8
global ISR_9
global ISR_10
global ISR_11
global ISR_12
global ISR_13
global ISR_14
global ISR_15
global ISR_16
global ISR_17
global ISR_18
global ISR_19
global ISR_20
global ISR_21
global ISR_22
global ISR_23
global ISR_24
global ISR_25
global ISR_26
global ISR_27
global ISR_28
global ISR_29
global ISR_30
global ISR_31

; IRQs
global IRQ_0
global IRQ_1
global IRQ_2
global IRQ_3
global IRQ_4
global IRQ_5
global IRQ_6
global IRQ_7
global IRQ_8
global IRQ_9
global IRQ_10
global IRQ_11
global IRQ_12
global IRQ_13
global IRQ_14
global IRQ_15

;─────────────────────────────────────;
;      Exception Handlers (ISRs)      ;
;─────────────────────────────────────;

; 0: Divide By Zero Exception
ISR_0:
    push byte 0
    push byte 0
    jmp ISR_common_stub

; 1: Debug Exception
ISR_1:
    push byte 0
    push byte 1
    jmp ISR_common_stub

; 2: Non Maskable Interrupt Exception
ISR_2:
    push byte 0
    push byte 2
    jmp ISR_common_stub

; 3: Int 3 Exception
ISR_3:
    push byte 0
    push byte 3
    jmp ISR_common_stub

; 4: INTO Exception
ISR_4:
    push byte 0
    push byte 4
    jmp ISR_common_stub

; 5: Out of Bounds Exception
ISR_5:
    push byte 0
    push byte 5
    jmp ISR_common_stub

; 6: Invalid Opcode Exception
ISR_6:
    push byte 0
    push byte 6
    jmp ISR_common_stub

; 7: Coprocessor Not Available Exception
ISR_7:
    push byte 0
    push byte 7
    jmp ISR_common_stub

; 8: Double Fault Exception (With Error Code!)
ISR_8:
    push byte 8
    jmp ISR_common_stub

; 9: Coprocessor Segment Overrun Exception
ISR_9:
    push byte 0
    push byte 9
    jmp ISR_common_stub

; 10: Bad TSS Exception (With Error Code!)
ISR_10:
    push byte 10
    jmp ISR_common_stub

; 11: Segment Not Present Exception (With Error Code!)
ISR_11:
    push byte 11
    jmp ISR_common_stub

; 12: Stack Fault Exception (With Error Code!)
ISR_12:
    push byte 12
    jmp ISR_common_stub

; 13: General Protection Fault Exception (With Error Code!)
ISR_13:
    push byte 13
    jmp ISR_common_stub

; 14: Page Fault Exception (With Error Code!)
ISR_14:
    push byte 14
    jmp ISR_common_stub

; 15: Reserved Exception
ISR_15:
    push byte 0
    push byte 15
    jmp ISR_common_stub

; 16: Floating Point Exception
ISR_16:
    push byte 0
    push byte 16
    jmp ISR_common_stub

; 17: Alignment Check Exception
ISR_17:
    push byte 0
    push byte 17
    jmp ISR_common_stub

; 18: Machine Check Exception
ISR_18:
    push byte 0
    push byte 18
    jmp ISR_common_stub

; 19: Reserved
ISR_19:
    push byte 0
    push byte 19
    jmp ISR_common_stub

; 20: Reserved
ISR_20:
    push byte 0
    push byte 20
    jmp ISR_common_stub

; 21: Reserved
ISR_21:
    push byte 0
    push byte 21
    jmp ISR_common_stub

; 22: Reserved
ISR_22:
    push byte 0
    push byte 22
    jmp ISR_common_stub

; 23: Reserved
ISR_23:
    push byte 0
    push byte 23
    jmp ISR_common_stub

; 24: Reserved
ISR_24:
    push byte 0
    push byte 24
    jmp ISR_common_stub

; 25: Reserved
ISR_25:
    push byte 0
    push byte 25
    jmp ISR_common_stub

; 26: Reserved
ISR_26:
    push byte 0
    push byte 26
    jmp ISR_common_stub

; 27: Reserved
ISR_27:
    push byte 0
    push byte 27
    jmp ISR_common_stub

; 28: Reserved
ISR_28:
    push byte 0
    push byte 28
    jmp ISR_common_stub

; 29: Reserved
ISR_29:
    push byte 0
    push byte 29
    jmp ISR_common_stub

; 30: Reserved
ISR_30:
    push byte 0
    push byte 30
    jmp ISR_common_stub

; 31: Reserved
ISR_31:
    push byte 0
    push byte 31
    jmp ISR_common_stub


;─────────────────────────────────────;
;             IRQ Handlers            ;
;─────────────────────────────────────;
IRQ_0:
	push byte 0
	push byte 32
	jmp IRQ_common_stub

IRQ_1:
	push byte 1
	push byte 33
	jmp IRQ_common_stub

IRQ_2:
	push byte 2
	push byte 34
	jmp IRQ_common_stub

IRQ_3:
	push byte 3
	push byte 35
	jmp IRQ_common_stub

IRQ_4:
	push byte 4
	push byte 36
	jmp IRQ_common_stub

IRQ_5:
	push byte 5
	push byte 37
	jmp IRQ_common_stub

IRQ_6:
	push byte 6
	push byte 38
	jmp IRQ_common_stub

IRQ_7:
	push byte 7
	push byte 39
	jmp IRQ_common_stub

IRQ_8:
	push byte 8
	push byte 40
	jmp IRQ_common_stub

IRQ_9:
	push byte 9
	push byte 41
	jmp IRQ_common_stub

IRQ_10:
	push byte 10
	push byte 42
	jmp IRQ_common_stub

IRQ_11:
	push byte 11
	push byte 43
	jmp IRQ_common_stub

IRQ_12:
	push byte 12
	push byte 44
	jmp IRQ_common_stub

IRQ_13:
	push byte 13
	push byte 45
	jmp IRQ_common_stub

IRQ_14:
	push byte 14
	push byte 46
	jmp IRQ_common_stub

IRQ_15:
	push byte 15
	push byte 47
	jmp IRQ_common_stub


; Here is the definition of our BSS section. Right now, we'll use
; it just to store the stack. Remember that a stack actually grows
; downwards, so we declare the size of the data before declaring
; the identifier '_sys_stack'
SECTION .bss
    resb 16384               ; This reserves 8KBytes of stack memory here
_sys_stack:
