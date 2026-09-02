#include "kernel/core/init.h"
#include "arch/x86/cpu.h"
#include "arch/x86/gdt.h"
#include "arch/x86/idt.h"
#include "arch/x86/interrupt.h"
#include "arch/x86/paging.h"
#include "arch/x86/pit.h"
#include "arch/x86/syscall.h"
#include "config/config.h"
#include "kernel/memory/heap.h"
#include "kernel/memory/physical.h"
#include "kernel/scheduler/thread.h"
#include "kernel/core/panic.h"
#include "kernel/core/version.h"
#include "kernel/core/debug.h"
#include "base/lib/mbr.h"

extern char kernel_end;

static const struct mb2module *bootmodule(uintptr_t info) {
    return (const struct mb2module *)mb2find(info, MB2_TAG_MODULE);
}

static const struct mb2tag *nexttag(uintptr_t info, uintptr_t *offset) {
    uint32_t total;
    const struct mb2tag *tag;

    if (!info || !offset) {
        return nil;
    }

    total = *(uint32_t *)info;
    if (*offset + sizeof(struct mb2tag) > total) {
        return nil;
    }

    tag = (const struct mb2tag *)(info + *offset);
    if (tag->type == MB2_TAG_END || tag->size < sizeof(*tag)) {
        return nil;
    }

    *offset += alignup(tag->size, 8);
    return tag;
}

static uintptr_t modulesend(uintptr_t info) {
    uintptr_t offset = 8;
    uintptr_t end = 0;
    const struct mb2tag *tag;

    while ((tag = nexttag(info, &offset)) != nil) {
        if (tag->type == MB2_TAG_MODULE) {
            const struct mb2module *module = (const struct mb2module *)tag;
            if (module->mod_end > end) {
                end = module->mod_end;
            }
        }
    }

    return end;
}

static void reservemodules(uintptr_t info) {
    uintptr_t offset = 8;
    const struct mb2tag *tag;

    while ((tag = nexttag(info, &offset)) != nil) {
        if (tag->type == MB2_TAG_MODULE) {
            const struct mb2module *module = (const struct mb2module *)tag;
            if (module->mod_end > module->mod_start) {
                pmmreserve(module->mod_start, module->mod_end);
            }
        }
    }
}

static void attach_mbr_partitions(struct kernel *k, struct blockdevice *disk, unsigned slot_base) {
    uint8_t sector[MBR_SECTOR_SIZE];
    struct mbr_table table;

    if (!k || !disk || slot_base >= countof(k->partitions) || disk->read(disk, 0, sector, 1) != 1) {
        return;
    }
    if (!mbr_parse(sector, sizeof(sector), &table)) {
        KLOG("block", "%s has no valid MBR", disk->name ? disk->name : "disk");
        return;
    }

    for (unsigned i = 0; i < MBR_PARTITIONS && slot_base + i < countof(k->partitions); i++) {
        struct mbr_partition *entry = &table.part[i];
        struct partition *part = &k->partitions[slot_base + i];
        char name[16];

        if (!entry->type || !entry->sectors) {
            continue;
        }
        snprintf(name, sizeof(name), "%sp%u", disk->name ? disk->name : "disk", i + 1u);
        partition(part, name, disk, entry->first_lba, entry->sectors, disk->readonly);
        k->devfs.attach(&k->devfs, &part->device);
        KLOG("block", "partition %s type=0x%x %s start=%u sectors=%u",
            part->device.name,
            entry->type,
            mbr_type_name(entry->type),
            entry->first_lba,
            entry->sectors);
    }
}

static void attach_one_disk(struct kernel *k, unsigned index, uint8_t slave) {
    char name[8];

    if (!k || index >= countof(k->harddisks)) {
        return;
    }

    snprintf(name, sizeof(name), "hd%u", index);
    if (harddisk(&k->harddisks[index], name, 0x1F0u, 0x3F6u, slave)) {
        k->devfs.attach(&k->devfs, &k->harddisks[index].device);
        KLOG("block", "attached %s sectors=%u lba48=%u", k->harddisks[index].device.name, k->harddisks[index].device.sectors, k->harddisks[index].lba48);
        attach_mbr_partitions(k, &k->harddisks[index].device, index * MBR_PARTITIONS);
    } else {
        KLOG("block", "no ATA disk detected for hd%u", index);
    }
}

static void attach_storage(struct kernel *k) {
    attach_one_disk(k, 0, 0);
    attach_one_disk(k, 1, 1);
}

void init(struct kernel *k, uint32_t magic, uintptr_t info) {
    uintptr_t heap_start = alignup((uintptr_t)&kernel_end + 0x10000u, 4096u);
    uintptr_t heap_end = MONARCH_MEMORY_END;
    uintptr_t module_end = modulesend(info);

    /* Limine may place Multiboot2 modules right after the kernel. The
       bootstrap heap must start after those modules, otherwise early heap
       writes corrupt initrd bytes before the PMM can reserve them. */
    if (module_end && module_end + 0x10000u > heap_start) {
        heap_start = alignup(module_end + 0x10000u, 4096u);
    }

    disable();
    gdt();
    idt();
    interrupt();

    serial(&k->serial, COM1);
    debuginit(&k->serial);
    KLOG("boot", "serial ready magic=%x info=%p module_end=%p heap=%p..%p", magic, (void *)info, (void *)module_end, (void *)heap_start, (void *)heap_end);

    kheap(heap_start, heap_end);
    physical(&k->physical, info, heap_end);
    reservemodules(info);
    paging();
    KLOG("boot", "paging online cr3=%p", (void *)pagedirectory());
    framebuffer(&k->framebuffer, info);
    console(&k->console);
    keyboard(&k->keyboard);
    mouse(&k->mouse, k->framebuffer.ready(&k->framebuffer) ? (int)k->framebuffer.width : 640, k->framebuffer.ready(&k->framebuffer) ? (int)k->framebuffer.height : 480);
    tty(&k->tty, &k->console, &k->keyboard);
    speaker(&k->speaker);
    if (floppy_init(&k->floppy)) {
        KLOG("block", "floppy controller ready base=0x%x irq=%u dma=%u commands=%u", k->floppy.base, IRQ_FLOPPY, k->floppy.dma_ready, k->floppy.command_ready);
    } else {
        KLOG("block", "floppy controller unavailable");
    }
    if (ac97(&k->ac97)) {
        KLOG("sound", "ac97 pci=%u:%u.%u mixer=0x%x busmaster=0x%x irq=%u rate=%u",
            k->ac97.pci.bus,
            k->ac97.pci.slot,
            k->ac97.pci.function,
            k->ac97.mixer_base,
            k->ac97.busmaster_base,
            k->ac97.irq,
            k->ac97.sample_rate);
    } else {
        KLOG("sound", "no ac97 controller detected");
    }

    k->console.clear(&k->console);
    k->console.color(&k->console, VGA_LIGHT_GREY, VGA_BLACK);

    if (!kheapvirtual(MONARCH_HEAP_BASE, MONARCH_HEAP_RESERVE, MONARCH_HEAP_INITIAL)) {
        panic("virtual heap initialization failed");
    }
    pit(100);
    bfs(&k->rootfs);
    devfs(&k->devfs, &k->console, &k->serial, &k->keyboard, &k->mouse, &k->framebuffer, &k->ac97);
    if (k->floppy.present) {
        k->devfs.attach(&k->devfs, &k->floppy.device);
    }
    vfs(&k->fs, &k->rootfs);
    k->fs.mount(&k->fs, "/dev", "devfs", &k->devfs);
    attach_storage(k);
    if (k->fs.mkdir(&k->fs, "/tmp")) {
        KLOG("vfs", "created runtime directory /tmp");
    }
    {
        const struct mb2module *module = bootmodule(info);
        void *image = nil;
        size_t image_size = 0;

        if (module && module->mod_end > module->mod_start) {
            image = (void *)(uintptr_t)module->mod_start;
            image_size = (size_t)(module->mod_end - module->mod_start);
        }

        initrd(&k->initrd, image, image_size);
        KLOG("initrd", "module image=%p size=%u files=%u", image, (unsigned)image_size, k->initrd.files(&k->initrd));
        if (k->initrd.device(&k->initrd)) {
            k->devfs.attach(&k->devfs, k->initrd.device(&k->initrd));
            k->initrd.load(&k->initrd, &k->fs, "/initrd");
        }
    }
    {
        int fat_mounted = 0;
        int ext2_mounted = 0;
        for (unsigned i = 0; i < countof(k->partitions); i++) {
            if (!k->partitions[i].device.name) {
                continue;
            }
            if (!fat_mounted && fat32(&k->fatfs, &k->partitions[i].device)) {
                k->fs.mount(&k->fs, "/fat", "fat32", &k->fatfs);
                KLOG("fat32", "mounted %s at /fat root=%u clusters=%u",
                    k->partitions[i].device.name,
                    k->fatfs.info.root_cluster,
                    k->fatfs.info.total_clusters);
                fat_mounted = 1;
                continue;
            }
            if (!ext2_mounted && ext2(&k->ext2fs, &k->partitions[i].device)) {
                k->fs.mount(&k->fs, "/ext2", "ext2", &k->ext2fs);
                KLOG("ext2", "mounted %s at /ext2 blocks=%u block_size=%u",
                    k->partitions[i].device.name,
                    k->ext2fs.info.blocks_count,
                    k->ext2fs.info.block_size);
                ext2_mounted = 1;
            }
        }
    }
    syscall(&k->fs);
    threadinit("main");

    k->boot.magic = magic;
    k->boot.address = info;

    k->serial.printf(&k->serial, "[monarch] boot magic=%x info=%p module_end=%p bootstrap=%p..%p\n", magic, (void *)info, (void *)module_end, (void *)heap_start, (void *)heap_end);
    k->serial.printf(&k->serial, "[monarch] pmm limit=%p total=%u free=%u pages\n", (void *)pmmlimit(), (unsigned)pmmtotal(), (unsigned)pmmfreepages());
    k->serial.printf(&k->serial, "[monarch] paging=%s cr3=%p\n", pagingactive() ? "on" : "off", (void *)pagedirectory());
    if (k->framebuffer.ready(&k->framebuffer)) {
        k->serial.printf(&k->serial, "[monarch] framebuffer=%p %ux%u pitch=%u bpp=%u\n",
            (void *)k->framebuffer.physical,
            k->framebuffer.width,
            k->framebuffer.height,
            k->framebuffer.pitch,
            k->framebuffer.bpp);
    } else {
        k->serial.write(&k->serial, "[monarch] framebuffer unavailable\n");
    }
    k->serial.printf(&k->serial, "[monarch] heap %s start=%p break=%p mapped=%p end=%p\n",
        kheappaged() ? "virtual" : "bootstrap",
        (void *)kheapstart(), (void *)kheapbreak(), (void *)kheapmapped(), (void *)kheapend());
    k->serial.printf(&k->serial, "[monarch] initrd files=%u size=%u bytes\n", k->initrd.files(&k->initrd), (unsigned)k->initrd.size(&k->initrd));
    k->tty.printf(&k->tty, "\n%s %s (%s)\n", MONARCH_NAME, MONARCH_VERSION, MONARCH_CODENAME);
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        k->tty.printf(&k->tty, "warning: unexpected bootloader magic: 0x%x\n", magic);
    }
    k->tty.write(&k->tty, "type 'help' to list commands.\n\n");

    enable();
    k->serial.write(&k->serial, "[monarch] interrupts enabled\n");
}
