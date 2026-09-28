ROOT := ../../../..
SPARK_SOURCES := spark.c main.c
.PHONY: all build clean list
all: build
build:
	$(MAKE) -C $(ROOT) initrd/sys/bin/spark.elf
clean:
	$(MAKE) -C $(ROOT) clean
list:
	@printf '%s\n' $(SPARK_SOURCES)
