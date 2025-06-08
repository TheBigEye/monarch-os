# ---------------------- Makefile for building Monarch OS ---------------------- #

# $@ = target file
# $< = first dependency
# $^ = all dependencies

# Source dir
SOURCE_DIR = source
ASSETS_DIR = assets
SCRIPTS_DIR = scripts
BITMAPS_DIR = $(ASSETS_DIR)/bitmaps
BINARIES_DIR = $(SOURCE_DIR)/binaries

BROWN =\033[0;33m
CYAN =\033[0;36m
GREEN =\033[0;32m
RESET =\033[0m

# Compiler and linker configuration
CC = i686-elf-gcc
LD = i686-elf-ld


# GCC compilation flags
CCFLAGS := $(strip                  \
    -pipe                           \
    -std=gnu99                      \
    -m32                            \
    -mfpmath=387                    \
    -O2                             \
    -g0                             \
    -fno-pie                        \
    -fno-ident                      \
    -fstrength-reduce               \
    -fomit-frame-pointer            \
    -finline-functions              \
    -fno-builtin                    \
    -ffreestanding                  \
    -fno-strict-aliasing            \
    -Wl,--file-alignment,16         \
    -Wl,--section-alignment,4096    \
)

# GCC warning flags
CWFLAGS := $(strip                  \
    -Wall                           \
    -Wextra                         \
    -Werror                         \
    -Wfloat-equal                   \
    -Wundef                         \
    -Winit-self                     \
    -Wno-int-conversion             \
    -Wno-unused-parameter           \
    -Wno-unused-function            \
    -Wstrict-prototypes             \
)

# Linker flags
LDFLAGS := $(strip                  \
    -m elf_i386                     \
    --allow-multiple-definition     \
    -nostdlib                       \
    -s                              \
    --gc-sections                   \
    -Map=kernel.map                 \
)

# Detect OS and audio system
OS_NAME := $(shell uname -s)
# Default audio system
AUDIO_SYSTEM := dsound

ifeq ($(OS_NAME),Linux)
    # Check for pipewire first, then alsa
    HAVE_PIPEWIRE := $(shell which pw-top >/dev/null 2>&1 && echo 1 || echo 0)
    ifeq ($(HAVE_PIPEWIRE),1)
        AUDIO_SYSTEM := pipewire
    else
        HAVE_ALSA := $(shell which aplay >/dev/null 2>&1 && echo 1 || echo 0)
        ifeq ($(HAVE_ALSA),1)
            AUDIO_SYSTEM := alsa
        endif
    endif
endif

# Qemu virtual machine config
QEMU_ARGS := $(strip                \
    -boot d                         \
    -m 32M                          \
    -cpu max                        \
    -k en-us                        \
    -serial stdio                   \
    -display sdl,gl=off             \
    -device VGA,vgamem_mb=8         \
    -audiodev $(AUDIO_SYSTEM),id=0  \
    -machine pcspk-audiodev=0       \
    -machine pc                     \
    -rtc base=localtime,clock=host  \
)


# C Source files
SOURCES = $(wildcard                       \
	$(SOURCE_DIR)/common/*.c               \
	$(SOURCE_DIR)/kernel/BFS/*.c           \
	$(SOURCE_DIR)/kernel/BGL/*.c           \
	$(SOURCE_DIR)/kernel/CPU/FPU/*.c       \
	$(SOURCE_DIR)/kernel/CPU/GDT/*.c       \
	$(SOURCE_DIR)/kernel/CPU/IDT/*.c       \
	$(SOURCE_DIR)/kernel/CPU/ISR/*.c       \
	$(SOURCE_DIR)/kernel/CPU/PIT/*.c       \
	$(SOURCE_DIR)/kernel/CPU/RTC/*.c       \
	$(SOURCE_DIR)/kernel/CPU/*.c           \
	$(SOURCE_DIR)/kernel/drivers/ATA/*.c   \
	$(SOURCE_DIR)/kernel/drivers/COM/*.c   \
	$(SOURCE_DIR)/kernel/drivers/FBC/*.c   \
	$(SOURCE_DIR)/kernel/drivers/TTY/*.c   \
	$(SOURCE_DIR)/kernel/drivers/VGA/*.c   \
	$(SOURCE_DIR)/kernel/drivers/*.c       \
	$(SOURCE_DIR)/kernel/memory/*.c        \
	$(SOURCE_DIR)/kernel/modules/*.c       \
	$(SOURCE_DIR)/kernel/*.c               \
)

# C Headers files
HEADERS = $(wildcard                       \
	$(SOURCE_DIR)/boot/*.h                 \
	$(SOURCE_DIR)/common/*.h               \
	$(SOURCE_DIR)/kernel/BFS/*.h           \
	$(SOURCE_DIR)/kernel/BGL/*.h           \
	$(SOURCE_DIR)/kernel/CPU/FPU/*.h       \
	$(SOURCE_DIR)/kernel/CPU/GDT/*.h       \
	$(SOURCE_DIR)/kernel/CPU/IDT/*.h       \
	$(SOURCE_DIR)/kernel/CPU/ISR/*.h       \
	$(SOURCE_DIR)/kernel/CPU/PIT/*.h       \
	$(SOURCE_DIR)/kernel/CPU/RTC/*.h       \
	$(SOURCE_DIR)/kernel/CPU/*.h           \
	$(SOURCE_DIR)/kernel/drivers/ATA/*.h   \
	$(SOURCE_DIR)/kernel/drivers/COM/*.h   \
	$(SOURCE_DIR)/kernel/drivers/FBC/*.h   \
	$(SOURCE_DIR)/kernel/drivers/TTY/*.h   \
	$(SOURCE_DIR)/kernel/drivers/VGA/*.h   \
	$(SOURCE_DIR)/kernel/drivers/*.h       \
	$(SOURCE_DIR)/kernel/memory/*.h        \
	$(SOURCE_DIR)/kernel/modules/*.h       \
	$(SOURCE_DIR)/kernel/*.h               \
)


# Boot ASM files (high priority!)
BOOT_ASM = $(wildcard                      \
    $(SOURCE_DIR)/boot/*.asm               \
)

# Object files
OBJECTS = $(BOOT_ASM:.asm=.o)              \
          $(SOURCES:.c=.o)                 \


# Check if we need to regenerate binaries
# Check if BINARIES_DIR exists and contains .bin files
# Returns 1 if we need to regenerate bitmaps, 0 otherwise
NEED_BITMAPS := $(shell                                             \
	if [ ! -d "$(BINARIES_DIR)" ] ||                                \
	   [ -z "$$(ls -A $(BINARIES_DIR)/*.bin 2>/dev/null)" ]; then   \
		echo "1";                                                   \
	else                                                            \
		echo "0";                                                   \
	fi                                                              \
)

# Check Python availability
CHECK_PYTHON := $(shell command -v python 2>/dev/null)


### First rule is the one executed when no parameters are fed to the Makefile
all: binaries kernel.elf run
release: binaries kernel.elf OS.iso


# We need python for bitmaps!
check-python:
	@if [ "$(CHECK_PYTHON)" = "" ]; then                            \
		echo -e "${BROWN}[!]${RESET} We need Python 3 please...";   \
		exit 1;                                                     \
	fi


# Convert bitmaps into binary files :)
binaries: check-python
	@if [ "$(NEED_BITMAPS)" = "1" ]; then                                               \
		echo -e "${GREEN}[-]${RESET} Processing bitmaps...";                            \
		mkdir -p $(BINARIES_DIR)/bitmaps;                                               \
		python $(SCRIPTS_DIR)/imgbin.py "$(BITMAPS_DIR)" -o "$(BINARIES_DIR)/bitmaps";  \
		echo -e "${GREEN}[-]${RESET} Processing strings...";                            \
		for file in $(ASSETS_DIR)/*.txt; do                                             \
			if [ -f "$$file" ]; then                                                    \
				filename=$$(basename "$$file");                                         \
				cp "$$file" "$(BINARIES_DIR)/$${filename%.txt}.bin";                    \
			fi;                                                                         \
		done;                                                                           \
		echo -e "${GREEN}[-]${RESET} Binaries processing done";                         \
	else                                                                                \
		echo -e "${GREEN}[-]${RESET} Binaries are up to date!";                         \
	fi


# Build the kernel binary
kernel.elf: ${OBJECTS}
	@echo -e "${GREEN}[-]${RESET} Linking kernel binary '${BROWN}./$@${RESET}' ..."
	@${LD} -T $(SOURCE_DIR)/linker.ld ${LDFLAGS} ${OBJECTS} -o $@



# We make the ISO image
OS.iso: kernel.elf
	@echo -e "${GREEN}[-]${RESET} Generating system ISO image at '${BROWN}./$@${RESET}' ..."
	@mkdir -p ./grub/temp/boot/grub
	@cp $< ./grub/temp/boot/kernel.elf
	@cp ./grub/menu.lst ./grub/temp/boot/grub/menu.lst
	@cp ./grub/stage2 ./grub/temp/boot/grub/stage2
	@xorriso -as mkisofs -no-pad -V Butterfly -R -b boot/grub/stage2 -no-emul-boot -quiet -boot-load-size 4 -boot-info-table -o $@ grub/temp/
	@$(RM) -rf ./grub/temp


# Run the OS image in QEMU
run: OS.iso
	@echo -e "${GREEN}[-]${RESET} Starting QEMU virtual machine for '${BROWN}./$^${RESET}' ..."
	@echo -e "${CYAN}[i]${RESET} Using audio backend: ${BROWN}$(AUDIO_SYSTEM)${RESET}"
	@qemu-system-i386 -cdrom $< $(QEMU_ARGS)


# Run the OS image in QEMU
ata: OS.iso
	@echo -e "${GREEN}[-]${RESET} Starting QEMU virtual machine for '${BROWN}./$^${RESET}' ..."
	@echo -e "${CYAN}[i]${RESET} Using audio backend: ${BROWN}$(AUDIO_SYSTEM)${RESET}"
	@qemu-system-i386 -cdrom $< $(QEMU_ARGS) -drive file=disk.img,index=0,if=ide,format=raw


# Display the contents of the OS image
echo: OS.iso
	@xxd $<


# Compile C source files
%.o: %.c ${HEADERS}
	@echo -e "${CYAN}[i]${RESET} Compiling component at '${BROWN}./$<${RESET}'"
	@${CC} ${CWFLAGS} ${CCFLAGS} -c $< -o $@


# Compile assembly source files
%.o: %.asm
	@echo -e "${CYAN}[i]${RESET} Assembling component at '${BROWN}./$<${RESET}'"
	@nasm $< -Ox -f elf -o $@


# Assemble binary files
%.bin: %.asm
	@echo -e "${CYAN}[i]${RESET} Generating binary file for '${BROWN}./$<${RESET}'"
	@nasm $< -f bin -o $@


# Disassemble binary files
%.dis: %.bin
	@ndisasm -b 32 $< > $@


# Clean the project folder
clean:
	@echo -e "${GREEN}[-]${RESET} Cleaning objects and output files ..."
	@$(RM) *.o *.dis *.elf *.iso *.map
	@$(RM) -rf ./grub/temp
	@$(RM) -rf $(BINARIES_DIR)/*
	@find $(SOURCE_DIR) -name '*.o' -type f -delete