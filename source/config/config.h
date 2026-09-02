#ifndef MONARCH_CONFIG_CONFIG_H
#define MONARCH_CONFIG_CONFIG_H 1

#define MONARCH_MEMORY_END 0x01000000u
#define MONARCH_PHYSICAL_LIMIT 0x10000000u
#define MONARCH_PROMPT "@ "

/* Virtual kernel heap. The bootstrap heap still lives below MONARCH_MEMORY_END.
   After paging is enabled, kmalloc switches here and grows by mapping PMM pages. */
#define MONARCH_HEAP_BASE    0xD0000000u
#define MONARCH_HEAP_RESERVE 0x04000000u
#define MONARCH_HEAP_INITIAL 0x00100000u

#endif /* MONARCH_CONFIG_CONFIG_H */
