[BITS 32]

global _start

; Small execve smoke-test for Monarch's tiny Linux-like syscall subset.
; It replaces this process image with /initrd/bin/hello.elf.  On success the
; syscall never returns; on failure we print a message and exit with 111.
%define SYS_EXIT   1
%define SYS_WRITE  4
%define SYS_EXECVE 11

_start:
    mov eax, SYS_WRITE
    mov ebx, 1
    mov ecx, before
    mov edx, before_len
    int 0x80

    mov eax, SYS_EXECVE
    mov ebx, path
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    int 0x80

    mov eax, SYS_WRITE
    mov ebx, 1
    mov ecx, failed
    mov edx, failed_len
    int 0x80

    mov eax, SYS_EXIT
    mov ebx, 111
    int 0x80

before: db "chain.elf: execve('/initrd/bin/hello.elf')", 10
before_len equ $ - before
failed: db "chain.elf: execve failed", 10
failed_len equ $ - failed
path: db "/initrd/bin/hello.elf", 0
