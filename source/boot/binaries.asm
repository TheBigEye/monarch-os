[BITS 32]

; IMPORTANT!: When we modify some included binary files, we have to recompile all again

;;; WHY NOT OBJCOPY? LOL

%macro INCLUDE_BIN 2
    %define symbol %1
    %define path %2

    SECTION .rodata                       ; create a new section
    GLOBAL symbol                         ; make the symbol global

    ; define the symbol
    symbol:
        incbin path                       ; include the file
        db 0                              ; add a null terminator

        %1_size: dq %1_size - %1          ; define the size of the file
%endmacro

; include the ascii art
INCLUDE_BIN butterfly_logo, "source/binaries/butterfly.bin"


; include wallapapers :)
INCLUDE_BIN bigeye_480, "source/binaries/bitmaps/480_bigeye.bin"
INCLUDE_BIN myfall_480, "source/binaries/bitmaps/480_myfall.bin"
INCLUDE_BIN myfood_480, "source/binaries/bitmaps/480_myfood.bin"
INCLUDE_BIN mylamb_480, "source/binaries/bitmaps/480_mylamb.bin"
INCLUDE_BIN mylife_480, "source/binaries/bitmaps/480_mylife.bin"
INCLUDE_BIN mymind_480, "source/binaries/bitmaps/480_mymind.bin"
INCLUDE_BIN theman_480, "source/binaries/bitmaps/480_theman.bin"

INCLUDE_BIN candle_640, "source/binaries/bitmaps/640_candle.bin"
INCLUDE_BIN choice_640, "source/binaries/bitmaps/640_choice.bin"
INCLUDE_BIN clouds_640, "source/binaries/bitmaps/640_clouds.bin"
INCLUDE_BIN myhill_640, "source/binaries/bitmaps/640_myhill.bin"
INCLUDE_BIN mypain_640, "source/binaries/bitmaps/640_mypain.bin"
INCLUDE_BIN mypath_640, "source/binaries/bitmaps/640_mypath.bin"
INCLUDE_BIN myroad_640, "source/binaries/bitmaps/640_myroad.bin"
INCLUDE_BIN mywork_640, "source/binaries/bitmaps/640_mywork.bin"
INCLUDE_BIN wchess_640, "source/binaries/bitmaps/640_wchess.bin"


; include more files to be used, an example can be found in binaries.h
