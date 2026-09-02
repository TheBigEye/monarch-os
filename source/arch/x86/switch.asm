[BITS 32]

extern threadsetsysstack

global contextswitch

; void contextswitch(uintptr_t *old_sp, uintptr_t new_sp)
; Saves callee-saved registers and the current stack pointer into *old_sp,
; loads new_sp, restores the next context and returns into it.
contextswitch:
    mov eax, [esp + 4] ; old_sp pointer
    mov edx, [esp + 8] ; new_sp value

    push ebp
    push ebx
    push esi
    push edi

    mov [eax], esp
    mov esp, edx

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

global usercall

; int usercall(uintptr_t stack, uintptr_t entry, int argc, char **argv)
; Runs an ELF entry point on a supplied ring-0 "user" stack for now.
; Later, ring-3 entry will replace this helper with iret-based transition code.
usercall:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi

    mov ebx, esp        ; save kernel/thread stack after callee-saved pushes
    push ebx
    call threadsetsysstack
    add esp, 4

    mov esp, [ebp + 8]  ; switch to process stack
    and esp, 0xfffffff0

    push dword [ebp + 20] ; argv
    push dword [ebp + 16] ; argc
    mov eax, [ebp + 12]   ; entry
    call eax

    mov esp, ebx        ; restore kernel/thread stack, keep eax return value
    push eax
    push dword 0
    call threadsetsysstack
    add esp, 4
    pop eax

    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

global userenter

; void userenter(uintptr_t stack, uintptr_t entry)
; Enters ring 3 through an iret frame.  The process stack must already contain
; the normal C call frame expected by _start: fake return, argc, argv.
userenter:
    mov eax, [esp + 4] ; user stack
    mov edx, [esp + 8] ; user entry

    mov cx, 0x23       ; user data selector (GDT_USER_DATA)
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    push dword 0x23    ; ss3
    push eax           ; esp3
    push dword 0x202   ; eflags: IF set
    push dword 0x1b    ; cs3 (GDT_USER_CODE)
    push edx           ; eip
    iretd

.hang:
    hlt
    jmp .hang
