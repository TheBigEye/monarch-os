# Desk service module manifest.
ROOT := ../../../..

DESK_SOURCES := desk.c button.c taskbar.c main.c

.PHONY: all build clean list

all: build

build:
	$(MAKE) -C $(ROOT) initrd/sys/bin/desk.elf

clean:
	$(MAKE) -C $(ROOT) clean

list:
	@printf '%s\n' $(DESK_SOURCES)
