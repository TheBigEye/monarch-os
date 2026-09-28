ROOT := ../../../../
WASP_SOURCES := wasp.c main.c
.PHONY: all build clean list
all: build
build:
	$(MAKE) -C $(ROOT) initrd/win/bin/wasp.elf
clean:
	$(MAKE) -C $(ROOT) clean
list:
	@printf '%s\n' $(WASP_SOURCES)
