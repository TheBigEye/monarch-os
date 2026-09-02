#include "kernel/core/elf.h"
#include "arch/x86/paging.h"
#include "kernel/memory/physical.h"

#define EI_NIDENT 16
#define ET_EXEC 2
#define EM_386 3
#define EV_CURRENT 1
#define PT_LOAD 1

struct ehdr {
    unsigned char ident[EI_NIDENT];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
    uint16_t shentsize;
    uint16_t shnum;
    uint16_t shstrndx;
} __attribute__((packed));

struct phdr {
    uint32_t type;
    uint32_t offset;
    uint32_t vaddr;
    uint32_t paddr;
    uint32_t filesz;
    uint32_t memsz;
    uint32_t flags;
    uint32_t align;
} __attribute__((packed));

static const char *last_error = "ok";

static void fail(const char *message) {
    last_error = message;
}

const char *elferror(void) {
    return last_error;
}

static int header(const void *data, size_t size, const struct ehdr **out) {
    const struct ehdr *elf = data;

    if (!data || size < sizeof(*elf)) {
        fail("file too small");
        return 0;
    }

    if (elf->ident[0] != 0x7F || elf->ident[1] != 'E' || elf->ident[2] != 'L' || elf->ident[3] != 'F') {
        fail("bad magic");
        return 0;
    }

    if (elf->ident[4] != 1) {
        fail("not ELF32");
        return 0;
    }

    if (elf->ident[5] != 1) {
        fail("not little-endian");
        return 0;
    }

    if (elf->ident[6] != EV_CURRENT) {
        fail("unsupported ELF version");
        return 0;
    }

    if (elf->type != ET_EXEC || elf->machine != EM_386 || elf->version != EV_CURRENT) {
        fail("not i386 executable");
        return 0;
    }

    if (elf->phentsize != sizeof(struct phdr)) {
        fail("bad program header table");
        return 0;
    }

    /*
     * Validate phoff and the table size separately, in that order, so
     * neither addition nor multiplication can wrap size_t and slip an
     * out-of-range program header table past this check.
     */
    if ((size_t)elf->phoff > size ||
        (size_t)elf->phnum * (size_t)elf->phentsize > size - (size_t)elf->phoff) {
        fail("bad program header table");
        return 0;
    }

    *out = elf;
    fail("ok");
    return 1;
}

static int scan(const void *data, size_t size, const struct ehdr *elf, uintptr_t *low, uintptr_t *high) {
    const uint8_t *bytes = data;
    uintptr_t start = 0xFFFFFFFFu;
    uintptr_t end = 0;

    for (uint16_t i = 0; i < elf->phnum; i++) {
        const struct phdr *ph = (const struct phdr *)(bytes + elf->phoff + (size_t)i * elf->phentsize);
        uintptr_t segstart;
        uintptr_t segend;

        if (ph->type != PT_LOAD) {
            continue;
        }

        if (ph->memsz < ph->filesz) {
            fail("bad load segment");
            return 0;
        }

        /*
         * A pure BSS PT_LOAD has p_filesz == 0.  Some linkers give such a
         * segment an offset that is at, or even beyond, the end of the file.
         * That is fine: no bytes are read from the file for that segment, only
         * zero-filled memory is mapped.  Validate p_offset only when there are
         * actual file bytes to copy.
         */
        if (ph->filesz && (ph->offset > size || ph->filesz > size - ph->offset)) {
            fail("bad load segment");
            return 0;
        }

        segstart = ph->vaddr;
        segend = ph->vaddr + ph->memsz;
        if (segend <= segstart) {
            fail("empty or wrapped segment");
            return 0;
        }

        if (segstart < start) {
            start = segstart;
        }
        if (segend > end) {
            end = segend;
        }
    }

    if (!end) {
        fail("no loadable segments");
        return 0;
    }

    if (start < PAGE_USER_BASE || end >= PAGE_USER_TOP || end <= start) {
        fail("unsafe virtual range");
        return 0;
    }

    if (elf->entry < start || elf->entry >= end) {
        fail("entry outside image");
        return 0;
    }

    *low = start;
    *high = end;
    fail("ok");
    return 1;
}

int elfcheck(const void *data, size_t size) {
    const struct ehdr *elf;
    return header(data, size, &elf);
}

int elfinfo(const void *data, size_t size, struct elfimage *image) {
    const struct ehdr *elf;
    uintptr_t low;
    uintptr_t high;

    if (!image || !header(data, size, &elf) || !scan(data, size, elf, &low, &high)) {
        return 0;
    }

    image->entry = elf->entry;
    image->start = low;
    image->end = high;
    image->phnum = elf->phnum;
    fail("ok");
    return 1;
}

static void unmaprange(uintptr_t directory, uintptr_t start, uintptr_t end) {
    start &= ~(uintptr_t)(PAGE_SIZE - 1u);
    end = alignup(end, PAGE_SIZE);

    for (uintptr_t v = start; v < end; v += PAGE_SIZE) {
        uintptr_t p = pagegetin(directory, v);
        if (p) {
            pageunmapin(directory, v);
            pmmfree(p & ~(uintptr_t)(PAGE_SIZE - 1u));
        }
    }
}

static int unusedrange(uintptr_t directory, uintptr_t start, uintptr_t end) {
    start &= ~(uintptr_t)(PAGE_SIZE - 1u);
    end = alignup(end, PAGE_SIZE);

    for (uintptr_t v = start; v < end; v += PAGE_SIZE) {
        if (pagegetin(directory, v)) {
            fail("virtual address already mapped");
            return 0;
        }
    }

    return 1;
}

static int maprange(uintptr_t directory, uintptr_t start, uintptr_t end) {
    uintptr_t first = start & ~(uintptr_t)(PAGE_SIZE - 1u);
    uintptr_t last = alignup(end, PAGE_SIZE);

    for (uintptr_t v = first; v < last; v += PAGE_SIZE) {
        uintptr_t p;

        if (pagegetin(directory, v)) {
            continue;
        }

        p = pmmalloc();
        if (!p) {
            unmaprange(directory, first, v);
            fail("out of physical pages");
            return 0;
        }

        memset((void *)p, 0, PAGE_SIZE);
        if (!pagemapin(directory, v, p, PAGE_WRITE | PAGE_USER)) {
            pmmfree(p);
            unmaprange(directory, first, v);
            fail("cannot map segment");
            return 0;
        }
    }

    return 1;
}

static int copyto(uintptr_t directory, uintptr_t dst, const uint8_t *src, size_t size) {
    while (size) {
        uintptr_t p = pagegetin(directory, dst);
        size_t chunk;

        if (!p) {
            fail("segment page missing");
            return 0;
        }

        chunk = PAGE_SIZE - (dst & (PAGE_SIZE - 1u));
        if (chunk > size) {
            chunk = size;
        }

        memcpy((void *)p, src, chunk);
        dst += chunk;
        src += chunk;
        size -= chunk;
    }

    return 1;
}

int elfloadat(const void *data, size_t size, struct elfimage *image, uintptr_t directory) {
    const struct ehdr *elf;
    const uint8_t *bytes = data;
    uintptr_t low;
    uintptr_t high;

    if (!image) {
        fail("missing output image");
        return 0;
    }

    if (!directory) {
        directory = pagedirectory();
    }

    if (!header(data, size, &elf) || !scan(data, size, elf, &low, &high)) {
        return 0;
    }

    if (!unusedrange(directory, low, high)) {
        return 0;
    }

    for (uint16_t i = 0; i < elf->phnum; i++) {
        const struct phdr *ph = (const struct phdr *)(bytes + elf->phoff + (size_t)i * elf->phentsize);

        if (ph->type != PT_LOAD) {
            continue;
        }

        if (!maprange(directory, ph->vaddr, ph->vaddr + ph->memsz)) {
            unmaprange(directory, low, high);
            return 0;
        }

        if (ph->filesz && !copyto(directory, ph->vaddr, bytes + ph->offset, ph->filesz)) {
            unmaprange(directory, low, high);
            return 0;
        }
    }

    image->entry = elf->entry;
    image->start = low;
    image->end = high;
    image->phnum = elf->phnum;
    fail("ok");
    return 1;
}

int elfload(const void *data, size_t size, struct elfimage *image) {
    return elfloadat(data, size, image, pagedirectory());
}

void elfunloadat(const struct elfimage *image, uintptr_t directory) {
    if (!image || !image->start || !image->end || image->end <= image->start) {
        return;
    }
    unmaprange(directory ? directory : pagedirectory(), image->start, image->end);
}

void elfunload(const struct elfimage *image) {
    elfunloadat(image, pagedirectory());
}
