ROOT := ../../../../
WUSH_SOURCES := wush.c main.c
.PHONY: all build clean list
all: build
build:
	$(MAKE) -C $(ROOT) initrd/win/bin/wush.elf
clean:
	$(MAKE) -C $(ROOT) clean
list:
	@printf '%s\n' $(WUSH_SOURCES)
