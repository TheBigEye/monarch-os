[org 0x7c00]                ; Set the origin of the program to 0x7c00

mov [BOOT_DISK], dl

; Setup the stack
mov bp, 0x7c00              ; Set the base pointer to 0x7c00
mov sp, bp                  ; Set the stack pointer to the base pointer

call cls                    ; Clear the BIOS screen

; if you want, disable thse two for normal text mode 80x25 (VGA_WIDTH and VGA_HEIGHT will be also modified on display.h)
call video_mode             ; essential for scalling
call text_mode              ; 80x50 display size, 8x8 font
;call disable_cursor        ; disable the console cursor

mov bx, MSG_bootloader_version
call Printf

mov bx, nl
call Printf

; -----------------------------------------------------------------------------------------------------------------------------

; Read the disk sectors
call Read_disk

; Access extended memory
jmp PROGRAM_SPACE

%include "src/boot/screen/print.asm"
%include "src/boot/screen/mode.asm"
%include "src/boot/disk/disk.asm"

MSG_bootloader_version:     db 'Butterfly bootloader v1', 0
nl: db 0x0A, 0x0D, 0

; -----------------------------------------------------------------------------------------------------------------------------

; Fill the rest of the sector with zeros
times 510-($-$$) db 0

; Magic number
dw 0xaa55 ; Set the last two bytes to 0xaa55
