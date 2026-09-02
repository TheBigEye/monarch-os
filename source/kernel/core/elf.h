#ifndef MONARCH_KERNEL_CORE_ELF_H
#define MONARCH_KERNEL_CORE_ELF_H 1

#include "base/api/monarch.h"

#define ELF_LOAD_BASE_DEFAULT 0x40000000u

struct elfimage {
    uintptr_t entry;
    uintptr_t start;
    uintptr_t end;
    uint32_t phnum;
};

int elfcheck(const void *data, size_t size);
int elfinfo(const void *data, size_t size, struct elfimage *image);
int elfload(const void *data, size_t size, struct elfimage *image);
int elfloadat(const void *data, size_t size, struct elfimage *image, uintptr_t directory);
void elfunload(const struct elfimage *image);
void elfunloadat(const struct elfimage *image, uintptr_t directory);
const char *elferror(void);

#endif /* MONARCH_KERNEL_CORE_ELF_H */
