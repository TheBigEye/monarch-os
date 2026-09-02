# -----------------------------------------------------------------------------
# Monarch OS build system
# -----------------------------------------------------------------------------
#
# This Makefile builds a small i686 freestanding kernel, packs an initrd with
# userspace ELF tools, and creates a Limine/Multiboot2 bootable ISO.
#
# The layout is intentionally explicit rather than clever.  Kernel code,
# userspace apps, generated test assets and QEMU disk images are separated into
# sections so new OSDev features can be added without turning the build into a
# mystery maze.
# -----------------------------------------------------------------------------

# -----------------------------------------------------------------------------
# Project paths
# -----------------------------------------------------------------------------

PROJECT      := Monarch
SOURCE_DIR   := source
APP_DIR      := $(SOURCE_DIR)/apps
ASSETS_DIR   := assets
SCRIPTS_DIR  := scripts
INITRD_DIR   := initrd

BUILD_DIR    := .build
OBJ_DIR      := $(BUILD_DIR)/obj
ISO_ROOT     := $(BUILD_DIR)/iso-root

KERNEL_ELF   := $(BUILD_DIR)/kernel.elf
INITRD_IMG   := $(BUILD_DIR)/initrd.img
ISO_IMAGE    := $(BUILD_DIR)/$(PROJECT).iso
PUBLIC_ISO   := public/OS.iso

FAT32_IMAGE  := $(BUILD_DIR)/fat32.img
EXT2_IMAGE   := $(BUILD_DIR)/ext2.img
FLOPPY_IMAGE := $(BUILD_DIR)/floppy.img
DISK_STAMP   := $(BUILD_DIR)/disk-images.stamp
AUDIO_STAMP  := $(INITRD_DIR)/share/audio.stamp

# -----------------------------------------------------------------------------
# Toolchain and build mode
# -----------------------------------------------------------------------------
#
# Normal OSDev builds use i686-elf-gcc/i686-elf-ld.  The Arena/Linux smoke-test
# path can override them with:
#
#   make iso CC=gcc LD=ld -j2
#
# DEBUG=1 switches object/kernel output paths so release and debug artifacts do
# not stomp each other.  TRACE_SCHED=1 is a separate debug flavor because the
# scheduler trace is intentionally noisy.
# -----------------------------------------------------------------------------

CC = i686-elf-gcc
LD = i686-elf-ld
AS = nasm

# Tool paths emitted by `make vsconfig`.
# In MSYS2/Cygwin, cygpath -m converts /mingw32/... to the complete native
# drive path without hardcoding the installation directory. Forward slashes
# keep the generated JSON valid on both Windows and Unix-like hosts.
define resolve_vscode_tool
$(shell path="$$(command -v $(1) 2>/dev/null || printf '%s' '$(1)')"; if command -v cygpath >/dev/null 2>&1; then cygpath -m "$$path"; else printf '%s' "$$path"; fi)
endef
VSCODE_CC := $(call resolve_vscode_tool,$(CC))
VSCODE_LD := $(call resolve_vscode_tool,$(LD))
AR = ar

DEBUG ?= 0
TRACE_SCHED ?= 0
GDB_DEBUG ?= 0
LOGWM ?= 0

ifeq ($(DEBUG),1)
    ifeq ($(TRACE_SCHED),1)
        OBJ_DIR    := $(BUILD_DIR)/obj-debug-sched
        KERNEL_ELF := $(BUILD_DIR)/kernel-debug-sched.elf
    else
        OBJ_DIR    := $(BUILD_DIR)/obj-debug
        KERNEL_ELF := $(BUILD_DIR)/kernel-debug.elf
    endif
endif

ifeq ($(GDB_DEBUG),1)
    OBJ_DIR    := $(BUILD_DIR)/obj-debug-gdb
    KERNEL_ELF := $(BUILD_DIR)/kernel-debug-gdb.elf
endif

# -----------------------------------------------------------------------------
# Terminal colors for build messages
# -----------------------------------------------------------------------------

BROWN = \033[0;33m
CYAN  = \033[0;36m
GREEN = \033[0;32m
RED   = \033[0;31m
RESET = \033[0m

# -----------------------------------------------------------------------------
# Compiler and linker flags
# -----------------------------------------------------------------------------
#
# Kernel and userspace are both freestanding 32-bit i686 C, but they define
# different build macros so headers can reject wrong-side includes.
#
# Userspace apps use function/data sections plus --gc-sections at link time.
# That keeps the current static-library approach small without requiring a
# dynamic linker yet.
# -----------------------------------------------------------------------------

COMMON_CFLAGS := $(strip                  \
    -std=gnu99                            \
    -m32                                  \
    -march=i686                           \
    -O2                                   \
    -ffreestanding                        \
    -fno-builtin                          \
    -fno-stack-protector                  \
    -fno-pic                              \
    -fno-pie                              \
    -fno-omit-frame-pointer               \
    -fno-asynchronous-unwind-tables       \
    -fno-unwind-tables                    \
    -fno-strict-aliasing                  \
    -Wall                                 \
    -Wextra                               \
    -Werror                               \
    -Wundef                               \
    -Wstrict-prototypes                   \
    -Wno-unused-parameter                 \
    -I$(SOURCE_DIR)                       \
)

CFLAGS := $(strip                         \
    $(COMMON_CFLAGS)                      \
    -DMONARCH_KERNEL_BUILD=1              \
    -MMD                                  \
    -MP                                   \
)

APP_CFLAGS := $(strip                     \
    $(COMMON_CFLAGS)                      \
    -DMONARCH_USER_BUILD=1                \
    -DMONARCH_LOGWM=$(LOGWM)              \
    -ffunction-sections                   \
    -fdata-sections                       \
)

ASFLAGS := -f elf32

ifeq ($(DEBUG),1)
    CFLAGS += -DMONARCH_DEBUG=1
else
    CFLAGS += -DMONARCH_DEBUG=0
endif

ifeq ($(TRACE_SCHED),1)
    CFLAGS += -DMONARCH_TRACE_SCHED=1
else
    CFLAGS += -DMONARCH_TRACE_SCHED=0
endif

ifeq ($(GDB_DEBUG),1)
    CFLAGS += -g3 -O0
endif

LDFLAGS := $(strip                        \
    -m elf_i386                           \
    -nostdlib                             \
    -T $(SOURCE_DIR)/arch/x86/linker.ld   \
    -Map=$(BUILD_DIR)/kernel.map          \
)

APP_LDFLAGS := $(strip                    \
    -m elf_i386                           \
    --gc-sections                         \
    --undefined=gfx_platform_alloc        \
    --undefined=gfx_platform_zero         \
    --undefined=gfx_platform_free         \
    -Ttext 0x40000000                     \
    -e _start                             \
)

# -----------------------------------------------------------------------------
# QEMU virtual machine configuration
# -----------------------------------------------------------------------------
#
# `make run` attaches two generated IDE disks:
#   index 0: portable FAT32 image
#   index 1: portable EXT2 image
#
# Audio defaults to a host backend when available, and QEMU exposes both the
# legacy PC speaker and an AC'97 PCI device so Monarch can test real PCM output.
# -----------------------------------------------------------------------------

OS_NAME := $(shell uname -s)
AUDIO_SYSTEM := dsound

ifeq ($(OS_NAME),Linux)
    HAVE_PIPEWIRE := $(shell command -v pw-top >/dev/null 2>&1 && echo 1 || echo 0)
    ifeq ($(HAVE_PIPEWIRE),1)
        AUDIO_SYSTEM := pipewire
    else
        HAVE_ALSA := $(shell command -v aplay >/dev/null 2>&1 && echo 1 || echo 0)
        ifeq ($(HAVE_ALSA),1)
            AUDIO_SYSTEM := alsa
        endif
    endif
endif

QEMU_ARGS := $(strip                \
    -boot d                         \
    -m 32M                          \
    -cpu max                        \
    -k en-us                        \
    -serial stdio                   \
    -display sdl,gl=off             \
    -device VGA,vgamem_mb=8         \
    -audiodev $(AUDIO_SYSTEM),id=0  \
    -device AC97,audiodev=0         \
    -machine pcspk-audiodev=0       \
    -machine pc                     \
    -rtc base=localtime,clock=host  \
    -no-reboot                      \
    -no-shutdown                    \
)

# -----------------------------------------------------------------------------
# Kernel source discovery
# -----------------------------------------------------------------------------
#
# `source/apps/bin` is userspace source.
# `base/usr`, `base/fbd` and `base/sfx` are userspace-only helper libraries.
# `base/lib/crt0.c` is the C userspace entry point.  None of those belong in the
# kernel link.
# -----------------------------------------------------------------------------

KERNEL_C_EXCLUDES :=                     \
    ! -path '$(APP_DIR)/bin/*'           \
    ! -path '$(APP_DIR)/sys/*'           \
    ! -path '$(APP_DIR)/win/*'           \
    ! -path '$(SOURCE_DIR)/base/win/*'   \
    ! -path '$(SOURCE_DIR)/base/usr/*'   \
    ! -path '$(SOURCE_DIR)/base/fbd/*'   \
    ! -path '$(SOURCE_DIR)/base/sfx/*'   \
    ! -path '$(SOURCE_DIR)/base/lib/crt0.c'

C_SOURCES := $(shell find $(SOURCE_DIR) -name '*.c' $(KERNEL_C_EXCLUDES) | sort)
ASM_SOURCES := $(shell find $(SOURCE_DIR) -name '*.asm' ! -path '$(APP_DIR)/*' | sort)

OBJECTS :=                               \
    $(patsubst %.c,$(OBJ_DIR)/%.o,$(C_SOURCES)) \
    $(patsubst %.asm,$(OBJ_DIR)/%.o,$(ASM_SOURCES))

DEPS := $(patsubst %.c,$(OBJ_DIR)/%.d,$(C_SOURCES))

# -----------------------------------------------------------------------------
# Userspace app and runtime discovery
# -----------------------------------------------------------------------------
#
# ASM apps are low-level syscall smoke tests and link directly.
# C apps link against small static archives rather than every runtime object.
# This is the simple pre-.so path: smaller apps and initrd without a dynamic
# linker or ET_DYN support yet.
# -----------------------------------------------------------------------------

APP_ASM := $(shell find $(APP_DIR) -maxdepth 1 -name '*.asm' | sort)
APP_C := $(shell find $(APP_DIR)/bin -maxdepth 1 -name '*.c' 2>/dev/null | sort)
APP_WIN_C := $(shell find $(APP_DIR)/win -maxdepth 1 -name '*.c' 2>/dev/null | sort)

APP_ASM_BINS := $(patsubst $(APP_DIR)/%.asm,$(INITRD_DIR)/bin/%.elf,$(APP_ASM))
APP_C_BINS := $(patsubst $(APP_DIR)/bin/%.c,$(INITRD_DIR)/bin/%.elf,$(APP_C))
APP_SYS_BINS := $(INITRD_DIR)/sys/bin/wing.elf $(INITRD_DIR)/sys/bin/spark.elf $(INITRD_DIR)/sys/bin/desk.elf
APP_WIN_BINS := $(INITRD_DIR)/win/bin/wasp.elf $(INITRD_DIR)/win/bin/wush.elf
APP_BINS := $(APP_ASM_BINS) $(APP_C_BINS) $(APP_SYS_BINS) $(APP_WIN_BINS)

APP_CRT0_OBJ := $(BUILD_DIR)/apps/base/lib/crt0.o
APP_GFX_ALLOC_OBJ := $(BUILD_DIR)/apps/base/usr/gfx_alloc.o

APP_BASE_C := $(shell find $(SOURCE_DIR)/base/lib -maxdepth 1 -name '*.c' ! -name 'crt0.c' 2>/dev/null | sort)
APP_USR_C  := $(shell find $(SOURCE_DIR)/base/usr -maxdepth 1 -name '*.c' 2>/dev/null | sort)
APP_FBD_C  := $(shell find $(SOURCE_DIR)/base/fbd -maxdepth 1 -name '*.c' 2>/dev/null | sort)
APP_SFX_C  := $(shell find $(SOURCE_DIR)/base/sfx -maxdepth 1 -name '*.c' 2>/dev/null | sort)
APP_GFX_C  := $(shell find $(SOURCE_DIR)/base/gfx -maxdepth 1 -name '*.c' 2>/dev/null | sort)

APP_BASE_OBJS := $(patsubst $(SOURCE_DIR)/base/%.c,$(BUILD_DIR)/apps/base/%.o,$(APP_BASE_C))
APP_USR_OBJS  := $(patsubst $(SOURCE_DIR)/base/%.c,$(BUILD_DIR)/apps/base/%.o,$(APP_USR_C))
APP_FBD_OBJS  := $(patsubst $(SOURCE_DIR)/base/%.c,$(BUILD_DIR)/apps/base/%.o,$(APP_FBD_C))
APP_SFX_OBJS  := $(patsubst $(SOURCE_DIR)/base/%.c,$(BUILD_DIR)/apps/base/%.o,$(APP_SFX_C))
APP_WIN_OBJS  := $(patsubst $(SOURCE_DIR)/apps/win/%.c,$(BUILD_DIR)/apps/win/%.o,$(APP_WIN_C))
APP_WIN_API_C := $(shell find $(SOURCE_DIR)/base/win -maxdepth 1 -name '*.c' 2>/dev/null | sort)
APP_WIN_API_OBJS := $(patsubst $(SOURCE_DIR)/base/%.c,$(BUILD_DIR)/apps/base/%.o,$(APP_WIN_API_C))
APP_GFX_OBJS  := $(patsubst $(SOURCE_DIR)/base/%.c,$(BUILD_DIR)/apps/base/%.o,$(APP_GFX_C))

APP_LIB_DIR := $(BUILD_DIR)/apps/lib
APP_LIBBASE := $(APP_LIB_DIR)/libbase.a
APP_LIBUSR  := $(APP_LIB_DIR)/libusr.a
APP_LIBFBD  := $(APP_LIB_DIR)/libfbd.a
APP_LIBSFX  := $(APP_LIB_DIR)/libsfx.a
APP_LIBGFX  := $(APP_LIB_DIR)/libgfx.a
APP_LIBWIN  := $(APP_LIB_DIR)/libwin.a

APP_RUNTIME_ARCHIVES :=                 \
    $(APP_LIBBASE)                       \
    $(APP_LIBGFX)                        \
    $(APP_LIBWIN)                        \
    $(APP_LIBUSR)                        \
    $(APP_LIBFBD)                        \
    $(APP_LIBSFX)

# -----------------------------------------------------------------------------
# Generated initrd content
# -----------------------------------------------------------------------------

AUDIO_FILES :=                           \
    $(INITRD_DIR)/share/tone.wav         \
    $(INITRD_DIR)/share/tone.au          \
    $(INITRD_DIR)/share/tone.msfx        \
    $(INITRD_DIR)/share/tone44100.wav    \
    $(INITRD_DIR)/share/tone22050.au     \
    $(INITRD_DIR)/share/tone11025.msfx

IMAGE_FILES :=                           \
    $(INITRD_DIR)/share/butter.mbi       \
    $(INITRD_DIR)/share/gradient.mbi

INITRD_INPUTS := $(shell find $(INITRD_DIR) -type f 2>/dev/null | sort)
INITRD_GENERATED := $(IMAGE_FILES) $(AUDIO_FILES) $(AUDIO_STAMP) $(APP_BINS)

# -----------------------------------------------------------------------------
# Limine bootloader configuration
# -----------------------------------------------------------------------------

LIMINE_URL = https://github.com/Limine-Bootloader/Limine/releases/latest/download/limine-binary.tar.gz

define LIMINE_CONF
timeout: 5
remember_last_entry: yes

wallpaper: boot():/wallpaper.png
wallpaper_style: stretched
interface_resolution: 800x600
editor_highlighting: yes
verbose: yes

/Monarch OS
    protocol: multiboot2
    path: boot():/boot/kernel.elf
    module_path: boot():/boot/initrd.img
    resolution: 800x600x32
    textmode: no
endef
export LIMINE_CONF

# -----------------------------------------------------------------------------
# Top-level targets
# -----------------------------------------------------------------------------

.PHONY: all kernel initrd iso disk-images run release web clean limine check-tools tree vsconfig

all: iso
release: iso web
kernel: $(KERNEL_ELF)
initrd: $(INITRD_IMG)
disk-images: $(DISK_STAMP)

# -----------------------------------------------------------------------------
# Tool bootstrap checks and Limine download
# -----------------------------------------------------------------------------

check-tools:
	@for tool in $(CC) $(LD) $(AS) $(AR) xorriso curl tar python3; do             \
		if ! command -v $$tool >/dev/null 2>&1; then                                \
			printf "$(RED)[!]$(RESET) Missing tool: $(BROWN)%s$(RESET)\n" $$tool;     \
			exit 1;                                                                 \
		fi;                                                                      \
	done

limine: check-tools
	@if [ ! -f limine/boot/limine-bios.sys ] ||                                   \
	   [ ! -f limine/boot/limine-bios-cd.bin ] ||                                 \
	   [ ! -f limine/boot/limine-uefi-cd.bin ] ||                                 \
	   [ ! -f limine/EFI/BOOT/BOOTX64.EFI ] ||                                    \
	   [ ! -f limine/EFI/BOOT/BOOTIA32.EFI ]; then                                \
		printf "$(GREEN)[-]$(RESET) Downloading Limine binaries...\n";            \
		$(RM) -rf limine/temp;                                                    \
		mkdir -p limine/temp;                                                     \
		curl -sS -L $(LIMINE_URL) -o limine/temp/limine.tar.gz;                   \
		tar -xzf limine/temp/limine.tar.gz -C limine/temp;                        \
		mkdir -p limine/boot limine/EFI/BOOT;                                     \
		find limine/temp -name limine-bios.sys    -exec cp {} limine/boot/ \;;    \
		find limine/temp -name limine-bios-cd.bin -exec cp {} limine/boot/ \;;    \
		find limine/temp -name limine-uefi-cd.bin -exec cp {} limine/boot/ \;;    \
		find limine/temp -name BOOTX64.EFI        -exec cp {} limine/EFI/BOOT/ \;;    \
		find limine/temp -name BOOTIA32.EFI       -exec cp {} limine/EFI/BOOT/ \;;    \
		cp $(ASSETS_DIR)/wallpaper.png limine/wallpaper.png;                      \
		printf '%s\n' "$$LIMINE_CONF" > limine/limine.conf;                       \
		$(RM) -rf limine/temp;                                                    \
		printf "$(GREEN)[-]$(RESET) Limine ready!\n";                             \
	fi

# -----------------------------------------------------------------------------
# VSCode configuration
# -----------------------------------------------------------------------------

# The file() function is expanded while Make parses recipes, so create the
# destination before vsconfig writes generated JSON.
$(shell mkdir -p .vscode)

define VSCODE_CPP_PROPERTIES
{
    "configurations": [
        {
            "name": "Monarch Kernel",
            "compilerPath": "$(VSCODE_CC)",
            "intelliSenseMode": "gcc-x86",
            "cStandard": "gnu99",
            "compilerArgs": [
                "-m32",
                "-march=i686",
                "-ffreestanding",
                "-fno-builtin",
                "-fno-pic",
                "-fno-pie"
            ],
            "defines": [
                "MONARCH_KERNEL_BUILD=1",
                "MONARCH_USER_BUILD=0",
                "MONARCH_DEBUG=0",
                "MONARCH_TRACE_SCHED=0"
            ],
            "includePath": [
                "$${workspaceFolder}/source",
                "$${workspaceFolder}/source/arch/x86",
                "$${workspaceFolder}/source/base/api",
                "$${workspaceFolder}/source/base/lib",
                "$${workspaceFolder}/source/base/sys",
                "$${workspaceFolder}/source/drivers",
                "$${workspaceFolder}/source/kernel"
            ],
            "browse": {
                "path": ["$${workspaceFolder}/source"],
                "limitSymbolsToIncludedHeaders": true
            }
        },
        {
            "name": "Monarch Userspace",
            "compilerPath": "$(VSCODE_CC)",
            "intelliSenseMode": "gcc-x86",
            "cStandard": "gnu99",
            "compilerArgs": [
                "-m32",
                "-march=i686",
                "-ffreestanding",
                "-fno-builtin",
                "-fno-pic",
                "-fno-pie",
                "-ffunction-sections",
                "-fdata-sections"
            ],
            "defines": [
                "MONARCH_USER_BUILD=1",
                "MONARCH_KERNEL_BUILD=0",
                "MONARCH_DEBUG=0",
                "MONARCH_TRACE_SCHED=0"
            ],
            "includePath": [
                "$${workspaceFolder}/source",
                "$${workspaceFolder}/source/base",
                "$${workspaceFolder}/source/base/api",
                "$${workspaceFolder}/source/base/lib",
                "$${workspaceFolder}/source/base/usr",
                "$${workspaceFolder}/source/base/gfx",
                "$${workspaceFolder}/source/base/fbd",
                "$${workspaceFolder}/source/base/sfx",
                "$${workspaceFolder}/source/base/win",
                "$${workspaceFolder}/source/apps",
                "$${workspaceFolder}/source/apps/bin",
                "$${workspaceFolder}/source/apps/sys",
                "$${workspaceFolder}/source/apps/win"
            ],
            "browse": {
                "path": [
                    "$${workspaceFolder}/source/base",
                    "$${workspaceFolder}/source/apps"
                ],
                "limitSymbolsToIncludedHeaders": true
            }
        }
    ],
    "version": 4
}
endef

define VSCODE_TASKS
{
    "version": "2.0.0",
    "tasks": [
        {
            "label": "Monarch: build ISO",
            "type": "shell",
            "command": "make iso CC=$(VSCODE_CC) LD=$(VSCODE_LD) -j2",
            "problemMatcher": "$$gcc",
            "group": {"kind": "build", "isDefault": true}
        },
        {
            "label": "Monarch: build debug GDB ISO",
            "type": "shell",
            "command": "make iso CC=$(VSCODE_CC) LD=$(VSCODE_LD) DEBUG=1 GDB_DEBUG=1 -j2",
            "problemMatcher": "$$gcc"
        },
        {
            "label": "Monarch: clean",
            "type": "shell",
            "command": "make clean",
            "problemMatcher": []
        }
    ]
}
endef

vsconfig:
	@mkdir -p .vscode
	@printf "$(GREEN)[-]$(RESET) Generating VSCode configuration with CC=$(BROWN)%s$(RESET) LD=$(BROWN)%s$(RESET)\\n" "$(VSCODE_CC)" "$(VSCODE_LD)"
	@$(file >.vscode/c_cpp_properties.json,$(VSCODE_CPP_PROPERTIES))
	@$(file >.vscode/tasks.json,$(VSCODE_TASKS))

# -----------------------------------------------------------------------------
# Kernel build rules
# -----------------------------------------------------------------------------

$(KERNEL_ELF): $(OBJECTS) Makefile
	@mkdir -p $(BUILD_DIR)
	@printf "$(GREEN)[-]$(RESET) Linking $(BROWN)%s$(RESET)\n" "$@"
	@$(LD) $(LDFLAGS) $(OBJECTS) -o $@

$(OBJ_DIR)/%.o: %.c Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) CC $(BROWN)%s$(RESET)\n" "$<"
	@$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR)/%.o: %.asm Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) AS $(BROWN)%s$(RESET)\n" "$<"
	@$(AS) $(ASFLAGS) $< -o $@

# -----------------------------------------------------------------------------
# Generated assets for initrd
# -----------------------------------------------------------------------------

$(INITRD_DIR)/share/butter.mbi: $(SCRIPTS_DIR)/mkmbi.py $(INITRD_DIR)/share/butter.bmp
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) MBI $(BROWN)%s$(RESET)\n" "$@"
	@python3 $(SCRIPTS_DIR)/mkmbi.py $(INITRD_DIR)/share/butter.bmp -o $@

$(INITRD_DIR)/share/gradient.mbi: $(SCRIPTS_DIR)/mkmbi.py $(INITRD_DIR)/share/gradient.ppm
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) MBI $(BROWN)%s$(RESET)\n" "$@"
	@python3 $(SCRIPTS_DIR)/mkmbi.py $(INITRD_DIR)/share/gradient.ppm -o $@

$(AUDIO_STAMP): $(SCRIPTS_DIR)/mkaudio.py Makefile
	@mkdir -p $(INITRD_DIR)/share
	@printf "$(CYAN)[i]$(RESET) AUDIO $(BROWN)%s$(RESET)\n" "$(INITRD_DIR)/share"
	@python3 $(SCRIPTS_DIR)/mkaudio.py --outdir $(INITRD_DIR)/share
	@touch $@

$(AUDIO_FILES): $(AUDIO_STAMP)

# -----------------------------------------------------------------------------
# Userspace build rules
# -----------------------------------------------------------------------------

$(BUILD_DIR)/apps/%.o: $(APP_DIR)/%.asm Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) UAS $(BROWN)%s$(RESET)\n" "$<"
	@$(AS) -f elf32 $< -o $@

$(BUILD_DIR)/apps/base/%.o: $(SOURCE_DIR)/base/%.c Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) UCC $(BROWN)%s$(RESET)\n" "$<"
	@$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD_DIR)/apps/bin/%.o: $(APP_DIR)/bin/%.c Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) UCC $(BROWN)%s$(RESET)\n" "$<"
	@$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD_DIR)/apps/sys/%.o: $(APP_DIR)/sys/%.c Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) UCC $(BROWN)%s$(RESET)\n" "$<"
	@$(CC) $(APP_CFLAGS) -c $< -o $@

$(BUILD_DIR)/apps/win/%.o: $(APP_DIR)/win/%.c Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) UCC $(BROWN)%s$(RESET)\n" "$<"
	@$(CC) $(APP_CFLAGS) -c $< -o $@


$(APP_LIBBASE): $(APP_BASE_OBJS) Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) UAR $(BROWN)%s$(RESET)\n" "$@"
	@$(RM) -f $@
	@$(AR) rcs $@ $(filter %.o,$^)

$(APP_LIBUSR): $(APP_USR_OBJS) Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) UAR $(BROWN)%s$(RESET)\n" "$@"
	@$(RM) -f $@
	@$(AR) rcs $@ $(filter %.o,$^)

$(APP_LIBFBD): $(APP_FBD_OBJS) Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) UAR $(BROWN)%s$(RESET)\n" "$@"
	@$(RM) -f $@
	@$(AR) rcs $@ $(filter %.o,$^)

$(APP_LIBSFX): $(APP_SFX_OBJS) Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) UAR $(BROWN)%s$(RESET)\n" "$@"
	@$(RM) -f $@
	@$(AR) rcs $@ $(filter %.o,$^)

$(APP_LIBGFX): $(APP_GFX_OBJS) Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) UAR $(BROWN)%s$(RESET)\n" "$@"
	@$(RM) -f $@
	@$(AR) rcs $@ $(filter %.o,$^)

$(APP_LIBWIN): $(APP_WIN_API_OBJS) Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) UAR $(BROWN)%s$(RESET)\n" "$@"
	@$(RM) -f $@
	@$(AR) rcs $@ $(filter %.o,$^)

$(INITRD_DIR)/bin/%.elf: $(BUILD_DIR)/apps/%.o Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) ULD $(BROWN)%s$(RESET)\n" "$@"
	@$(LD) -m elf_i386 -Ttext 0x40000000 -e _start $< -o $@

$(INITRD_DIR)/bin/%.elf: $(BUILD_DIR)/apps/bin/%.o $(APP_CRT0_OBJ) $(APP_GFX_ALLOC_OBJ) $(APP_RUNTIME_ARCHIVES) Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) ULD $(BROWN)%s$(RESET)\n" "$@"
	@$(LD) $(APP_LDFLAGS) $(APP_CRT0_OBJ) $(APP_GFX_ALLOC_OBJ) $< --start-group $(APP_RUNTIME_ARCHIVES) --end-group -o $@

$(INITRD_DIR)/sys/bin/%.elf: $(BUILD_DIR)/apps/sys/%.o $(APP_CRT0_OBJ) $(APP_GFX_ALLOC_OBJ) $(APP_RUNTIME_ARCHIVES) Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) ULD $(BROWN)%s$(RESET)\n" "$@"
	@$(LD) $(APP_LDFLAGS) $(APP_CRT0_OBJ) $(APP_GFX_ALLOC_OBJ) $< --start-group $(APP_RUNTIME_ARCHIVES) --end-group -o $@

$(INITRD_DIR)/win/bin/%.elf: $(BUILD_DIR)/apps/win/%.o $(APP_CRT0_OBJ) $(APP_RUNTIME_ARCHIVES) Makefile
	@mkdir -p $(dir $@)
	@printf "$(CYAN)[i]$(RESET) ULD $(BROWN)%s$(RESET)\n" "$@"
	@$(LD) $(APP_LDFLAGS) $(APP_CRT0_OBJ) $< --start-group $(APP_RUNTIME_ARCHIVES) --end-group -o $@

# -----------------------------------------------------------------------------
# Initrd, disk images, ISO and run targets
# -----------------------------------------------------------------------------

$(INITRD_IMG): $(SCRIPTS_DIR)/mkinitrd.py $(INITRD_INPUTS) $(INITRD_GENERATED)
	@mkdir -p $(BUILD_DIR)
	@printf "$(GREEN)[-]$(RESET) Packing initrd $(BROWN)%s$(RESET)\n" "$@"
	@python3 $(SCRIPTS_DIR)/mkinitrd.py $(INITRD_DIR) -o $@

$(DISK_STAMP): $(SCRIPTS_DIR)/mkdiskimages.py Makefile
	@mkdir -p $(BUILD_DIR)
	@printf "$(GREEN)[-]$(RESET) Creating test disks $(BROWN)%s$(RESET) $(BROWN)%s$(RESET) $(BROWN)%s$(RESET)\n" "$(FAT32_IMAGE)" "$(EXT2_IMAGE)" "$(FLOPPY_IMAGE)"
	@python3 $(SCRIPTS_DIR)/mkdiskimages.py --fat32 $(FAT32_IMAGE) --ext2 $(EXT2_IMAGE) --floppy $(FLOPPY_IMAGE)
	@touch $@

iso: limine $(KERNEL_ELF) $(INITRD_IMG)
	@printf "$(GREEN)[-]$(RESET) Generating ISO $(BROWN)%s$(RESET)\n" "$(ISO_IMAGE)"
	@$(RM) -rf $(ISO_ROOT)
	@mkdir -p $(ISO_ROOT)/boot $(ISO_ROOT)/EFI/BOOT $(BUILD_DIR)
	@cp limine/boot/limine-bios.sys $(ISO_ROOT)/boot/limine-bios.sys
	@cp limine/boot/limine-bios-cd.bin $(ISO_ROOT)/boot/limine-bios-cd.bin
	@cp limine/boot/limine-uefi-cd.bin $(ISO_ROOT)/boot/limine-uefi-cd.bin
	@cp limine/EFI/BOOT/BOOTX64.EFI $(ISO_ROOT)/EFI/BOOT/BOOTX64.EFI
	@cp limine/EFI/BOOT/BOOTIA32.EFI $(ISO_ROOT)/EFI/BOOT/BOOTIA32.EFI
	@cp limine/limine.conf $(ISO_ROOT)/limine.conf
	@cp limine/wallpaper.png $(ISO_ROOT)/wallpaper.png
	@cp $(KERNEL_ELF) $(ISO_ROOT)/boot/kernel.elf
	@cp $(INITRD_IMG) $(ISO_ROOT)/boot/initrd.img
	@xorriso -as mkisofs \
		-V MONARCH \
		-R -r -J \
		-b boot/limine-bios-cd.bin \
		-c boot.catalog \
		-no-emul-boot \
		-boot-load-size 4 \
		-boot-info-table \
		--efi-boot boot/limine-uefi-cd.bin \
		-efi-boot-part \
		--efi-boot-image \
		--protective-msdos-label \
		-o $(ISO_IMAGE) \
		$(ISO_ROOT)

web: iso
	@cp $(ISO_IMAGE) $(PUBLIC_ISO)
	@printf "$(GREEN)[-]$(RESET) Web ISO copied to $(BROWN)%s$(RESET)\n" "$(PUBLIC_ISO)"

run: iso disk-images
	@printf "$(GREEN)[-]$(RESET) Starting QEMU for $(BROWN)%s$(RESET)\n" "$(ISO_IMAGE)"
	@printf "$(CYAN)[i]$(RESET) Using audio backend: $(BROWN)$(AUDIO_SYSTEM)$(RESET)\n"
	@printf "$(CYAN)[i]$(RESET) Attaching disks: $(BROWN)$(FAT32_IMAGE)$(RESET), $(BROWN)$(EXT2_IMAGE)$(RESET), $(BROWN)$(FLOPPY_IMAGE)$(RESET)\n"
	@qemu-system-i386 -cdrom $(ISO_IMAGE) \
		-drive file=$(FAT32_IMAGE),format=raw,if=ide,index=0,media=disk \
		-drive file=$(EXT2_IMAGE),format=raw,if=ide,index=1,media=disk \
		-drive file=$(FLOPPY_IMAGE),format=raw,if=none,id=fda \
		-device floppy,drive=fda,unit=0,drive-type=144 \
		-global isa-fdc.fdtypeA=144 \
		$(QEMU_ARGS)

# -----------------------------------------------------------------------------
# Utility targets
# -----------------------------------------------------------------------------

tree:
	@find . -path './limine' -prune -o -path './$(BUILD_DIR)' -prune -o -print | sort

clean:
	@printf "$(GREEN)[-]$(RESET) Cleaning generated files ...\n"
	@$(RM) -rf $(BUILD_DIR) limine $(PUBLIC_ISO) $(INITRD_GENERATED)

-include $(DEPS)
