#include "drivers/sound/ac97.h"
#include "arch/x86/cpu.h"
#include "arch/x86/interrupt.h"
#include "kernel/memory/physical.h"
#include "kernel/scheduler/thread.h"

#define PCI_CLASS_MULTIMEDIA 0x04u
#define PCI_SUBCLASS_AUDIO   0x01u

#define AC97_RESET       0x00u
#define AC97_MASTER_VOL  0x02u
#define AC97_PCM_VOL     0x18u
#define AC97_EXT_ID      0x28u
#define AC97_EXT_STATUS  0x2Au
#define AC97_PCM_RATE    0x2Cu

#define AC97_BDL_ENTRIES 16u
#define AC97_PAGE_BYTES  4096u

#define PO_BDBAR 0x10u
#define PO_CIV   0x14u
#define PO_LVI   0x15u
#define PO_SR    0x16u
#define PO_PICB  0x18u
#define PO_PIV   0x1Au
#define PO_CR    0x1Bu

#define PO_SR_DCH   0x0001u
#define PO_SR_CELV  0x0002u
#define PO_SR_LVBCI 0x0004u
#define PO_SR_BCIS  0x0008u
#define PO_SR_FIFOE 0x0010u
#define PO_SR_CLEAR (PO_SR_LVBCI | PO_SR_BCIS | PO_SR_FIFOE)

#define PO_CR_RUN   0x01u
#define PO_CR_RESET 0x02u

#define BDL_IOC 0x80000000u

struct ac97_bdl_entry {
    uint32_t address;
    uint32_t control;
} __attribute__((packed));

static struct ac97 *active;

static uint16_t mixer_read(struct ac97 *self, uint16_t reg) {
    return inw((uint16_t)(self->mixer_base + reg));
}

static void mixer_write(struct ac97 *self, uint16_t reg, uint16_t value) {
    outw((uint16_t)(self->mixer_base + reg), value);
}

static uint16_t io_bar(uint32_t value) {
    return (uint16_t)(value & ~0x3u);
}

static uint16_t po_reg(struct ac97 *self, uint16_t reg) {
    return (uint16_t)(self->busmaster_base + reg);
}

static uint8_t next_index(struct ac97 *self, uint8_t index) {
    index++;
    if (index >= self->buffer_count) {
        index = 0;
    }
    return index;
}

static int alloc_dma(struct ac97 *self) {
    self->bdl_phys = pmmalloc();
    if (!self->bdl_phys) {
        return 0;
    }
    self->bdl = (void *)self->bdl_phys;
    memset(self->bdl, 0, AC97_PAGE_BYTES);

    for (unsigned i = 0; i < AC97_BDL_ENTRIES; i++) {
        uintptr_t page = pmmalloc();
        if (!page) {
            break;
        }
        self->buffer_phys[self->buffer_count++] = page;
        self->buffer_bytes += AC97_PAGE_BYTES;
        memset((void *)page, 0, AC97_PAGE_BYTES);
    }

    return self->buffer_count > 1;
}

static void reset_pcm_out(struct ac97 *self) {
    outb(po_reg(self, PO_CR), 0);
    outw(po_reg(self, PO_SR), PO_SR_CLEAR);
    outb(po_reg(self, PO_CR), PO_CR_RESET);
    for (unsigned i = 0; i < 1000u && (inb(po_reg(self, PO_CR)) & PO_CR_RESET); i++) {
        wait();
    }
    outw(po_reg(self, PO_SR), PO_SR_CLEAR);
}

static void pcm_service(struct ac97 *self) {
    uint16_t status;
    uint8_t civ;

    if (!self || !self->present || !self->dma_ready) {
        return;
    }

    status = inw(po_reg(self, PO_SR));
    civ = (uint8_t)(inb(po_reg(self, PO_CIV)) % self->buffer_count);

    if (self->running && self->queued) {
        while (self->queued && self->read_index != civ) {
            self->read_index = next_index(self, self->read_index);
            self->queued--;
            self->completed++;
        }

        if ((status & PO_SR_DCH) && self->queued) {
            self->underruns++;
            self->queued = 0;
            self->read_index = 0;
            self->write_index = 0;
        }
    }

    if (status & PO_SR_FIFOE) {
        self->errors++;
    }

    if (status & PO_SR_DCH) {
        self->running = 0;
        outb(po_reg(self, PO_CR), 0);
    }

    if (status & PO_SR_CLEAR) {
        outw(po_reg(self, PO_SR), status & PO_SR_CLEAR);
    }
}

static void irq_handler(struct registers *state) {
    unused(state);
    pcm_service(active);
}

static void prepare_idle_stream(struct ac97 *self) {
    if (self->running || self->queued) {
        return;
    }

    reset_pcm_out(self);
    memset(self->bdl, 0, AC97_PAGE_BYTES);
    self->read_index = 0;
    self->write_index = 0;
    outl(po_reg(self, PO_BDBAR), (uint32_t)self->bdl_phys);
}

static int wait_for_slot(struct ac97 *self) {
    while (self->queued >= self->buffer_count - 1u) {
        pcm_service(self);
        if (self->queued < self->buffer_count - 1u) {
            break;
        }
        threadsleep(1);
    }
    return self->queued < self->buffer_count - 1u;
}

static void commit_buffer(struct ac97 *self, uint8_t index, uint32_t bytes) {
    struct ac97_bdl_entry *bdl = (struct ac97_bdl_entry *)self->bdl;
    uint32_t samples = bytes / 2u; /* AC'97 length is in 16-bit samples. */

    bdl[index].address = (uint32_t)self->buffer_phys[index];
    bdl[index].control = (samples & 0xFFFFu) | BDL_IOC;
    outb(po_reg(self, PO_LVI), index);
    self->queued++;
    self->write_index = next_index(self, index);

    if (!self->running) {
        outw(po_reg(self, PO_SR), PO_SR_CLEAR);
        outb(po_reg(self, PO_CR), PO_CR_RUN);
        self->running = 1;
    }
}

static int enqueue_pcm(struct ac97 *self, const uint8_t *pcm, uint32_t bytes) {
    uint8_t index;
    uint32_t count;

    if (!wait_for_slot(self)) {
        return 0;
    }

    index = self->write_index;
    count = min(bytes, AC97_PAGE_BYTES);
    memcpy((void *)self->buffer_phys[index], pcm, count);
    if (count < AC97_PAGE_BYTES) {
        memset((uint8_t *)self->buffer_phys[index] + count, 0, AC97_PAGE_BYTES - count);
    }
    commit_buffer(self, index, count);
    return (int)count;
}

static int enqueue_tone(struct ac97 *self, uint32_t frequency, uint32_t frames, uint32_t *frame_cursor) {
    uint32_t rate = self->sample_rate ? self->sample_rate : 48000u;
    uint32_t period = frequency ? rate / frequency : rate / 440u;
    uint32_t page_frames = min(frames, AC97_PAGE_BYTES / 4u);
    uint8_t index;
    int16_t *samples;

    if (!wait_for_slot(self)) {
        return 0;
    }
    if (period < 2u) {
        period = 2u;
    }

    index = self->write_index;
    samples = (int16_t *)self->buffer_phys[index];
    for (uint32_t i = 0; i < page_frames; i++, (*frame_cursor)++) {
        int16_t sample = ((*frame_cursor % period) < (period / 2u)) ? 9000 : -9000;
        samples[i * 2u + 0u] = sample;
        samples[i * 2u + 1u] = sample;
    }
    if (page_frames * 4u < AC97_PAGE_BYTES) {
        memset((uint8_t *)samples + page_frames * 4u, 0, AC97_PAGE_BYTES - page_frames * 4u);
    }

    commit_buffer(self, index, page_frames * 4u);
    return (int)page_frames;
}

static size_t align_pcm_size(size_t size) {
    return size & ~(size_t)3u;
}

static int play_impl(struct ac97 *self, const void *pcm, size_t size) {
    const uint8_t *bytes = pcm;
    size_t total;
    size_t done = 0;

    if (!self || !self->present || !self->dma_ready || (!pcm && size)) {
        return 0;
    }

    total = align_pcm_size(size);
    if (!total) {
        return 0;
    }

    prepare_idle_stream(self);
    while (done < total) {
        int count;
        pcm_service(self);
        count = enqueue_pcm(self, bytes + done, (uint32_t)(total - done));
        if (count <= 0) {
            break;
        }
        done += (size_t)count;
    }

    return (int)done;
}

static int tone_impl(struct ac97 *self, uint32_t frequency, uint32_t duration) {
    uint32_t rate;
    uint32_t frames;
    uint32_t cursor = 0;
    uint32_t done = 0;

    if (!self || !self->present || !self->dma_ready) {
        return 0;
    }

    rate = self->sample_rate ? self->sample_rate : 48000u;
    frames = (rate * (duration ? duration : 200u)) / 1000u;
    if (!frames) {
        frames = 1;
    }

    prepare_idle_stream(self);
    while (done < frames) {
        int count;
        pcm_service(self);
        count = enqueue_tone(self, frequency ? frequency : 440u, frames - done, &cursor);
        if (count <= 0) {
            break;
        }
        done += (uint32_t)count;
    }

    return done == frames;
}

int ac97(struct ac97 *self) {
    struct pcidevice device;

    if (!self) {
        return 0;
    }

    memset(self, 0, sizeof(*self));
    self->play = play_impl;
    self->tone = tone_impl;
    self->sample_rate = 48000;

    if (!pci_find_class(PCI_CLASS_MULTIMEDIA, PCI_SUBCLASS_AUDIO, PCI_ANY, &device)) {
        return 0;
    }

    if ((device.bar[0] & 1u) == 0 || (device.bar[1] & 1u) == 0) {
        return 0;
    }

    pci_enable_io_busmaster(&device);

    self->present = 1;
    self->pci = device;
    self->vendor_id = device.vendor_id;
    self->device_id = device.device_id;
    self->mixer_base = io_bar(device.bar[0]);
    self->busmaster_base = io_bar(device.bar[1]);
    self->irq = device.irq_line;

    /* Reset and unmute the common output paths. */
    (void)mixer_read(self, AC97_RESET);
    mixer_write(self, AC97_MASTER_VOL, 0x0000);
    mixer_write(self, AC97_PCM_VOL, 0x0000);

    self->extended_id = mixer_read(self, AC97_EXT_ID);
    self->extended_status = mixer_read(self, AC97_EXT_STATUS);

    /* Variable Rate Audio is optional.  If present, ask for 48 kHz explicitly. */
    if (self->extended_id & 0x0001u) {
        mixer_write(self, AC97_EXT_STATUS, (uint16_t)(self->extended_status | 0x0001u));
        mixer_write(self, AC97_PCM_RATE, 48000u);
        self->sample_rate = mixer_read(self, AC97_PCM_RATE);
        self->extended_status = mixer_read(self, AC97_EXT_STATUS);
    }

    self->dma_ready = alloc_dma(self);
    reset_pcm_out(self);
    active = self;
    if (self->irq < 16u) {
        irq_register((uint8_t)(32u + self->irq), irq_handler);
        irq_enable((uint8_t)(32u + self->irq));
    }
    return 1;
}

const char *ac97_info(struct ac97 *self, char *buffer, size_t size) {
    if (!buffer || !size) {
        return "";
    }

    if (!self || !self->present) {
        snprintf(buffer, size, "ac97 ready=0\n");
    } else {
        pcm_service(self);
        snprintf(buffer, size,
            "ac97 ready=1 dma=%u pci=%u:%u.%u vendor=0x%x device=0x%x mixer=0x%x busmaster=0x%x irq=%u ext_id=0x%x ext_status=0x%x rate=%u channels=2 format=s16le buffer_bytes=%u queued=%u running=%u completed=%u underruns=%u errors=%u\n",
            self->dma_ready ? 1u : 0u,
            self->pci.bus,
            self->pci.slot,
            self->pci.function,
            self->vendor_id,
            self->device_id,
            self->mixer_base,
            self->busmaster_base,
            self->irq,
            self->extended_id,
            self->extended_status,
            self->sample_rate,
            (unsigned)self->buffer_bytes,
            self->queued,
            self->running ? 1u : 0u,
            self->completed,
            self->underruns,
            self->errors);
    }
    return buffer;
}
