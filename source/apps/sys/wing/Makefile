# Wing service module manifest.
# The root Makefile owns the shared freestanding toolchain and linker flags;
# this file keeps Wing's source ownership visible and offers a focused target.
ROOT := ../../../..

WING_SOURCES := client.c context_menu.c server.c wing.c main.c

.PHONY: all build clean list

all: build

build:
	$(MAKE) -C $(ROOT) initrd/sys/bin/wing.elf

clean:
	$(MAKE) -C $(ROOT) clean

list:
	@printf '%s\n' $(WING_SOURCES)
