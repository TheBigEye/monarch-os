[BITS 32]

global _start

; Monarch intentionally keeps syscall numbers close to Linux i386 for the small
; subset it implements.  The calling convention is the classic int 0x80 style:
; eax = number, ebx/ecx/edx/esi = arguments, eax = return value.
%define SYS_EXIT  1
%define SYS_WRITE 4
%define SYS_SLEEP 162

_start:
    ; Print a hello message through fd 1 (/dev/console inherited from bush).
    mov eax, SYS_WRITE
    mov ebx, 1
    mov ecx, hello
    mov edx, hello_len
    int 0x80

    ; Sleep a little.  This is a Monarch simplification of Linux nanosleep:
    ; ebx contains milliseconds rather than a pointer to struct timespec.
    mov eax, SYS_SLEEP
    mov ebx, 250
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    int 0x80

    mov eax, SYS_WRITE
    mov ebx, 1
    mov ecx, bye
    mov edx, bye_len
    int 0x80

    mov eax, SYS_EXIT
    xor ebx, ebx
    int 0x80

    ret

hello: db "hello from /initrd/bin/hello.elf via Linux-like int 0x80", 10
hello_len equ $ - hello
bye: db "hello.elf returning through SYS_EXIT", 10
bye_len equ $ - bye
