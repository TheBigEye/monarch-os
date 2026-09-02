#ifndef MONARCH_DRIVERS_SOUND_AC97_H
#define MONARCH_DRIVERS_SOUND_AC97_H 1

/**
 * @file ac97.h
 * @brief Minimal Intel AC'97 PCI audio controller probe.
 *
 * This first AC'97 step only discovers and prepares the controller.  Real PCM
 * playback needs a bus-master buffer descriptor list, IRQ handling and a mixer
 * policy, which are added later in small testable chunks.
 */

#include "base/api/monarch.h"
#include "drivers/bus/pci.h"

struct ac97 {
    int (*play)(struct ac97 *self, const void *pcm, size_t size);
    int (*tone)(struct ac97 *self, uint32_t frequency, uint32_t duration);

    int present;
    int dma_ready;
    uint16_t mixer_base;
    uint16_t busmaster_base;
    uint8_t irq;
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t extended_id;
    uint16_t extended_status;
    uint32_t sample_rate;
    uintptr_t bdl_phys;
    void *bdl;
    uintptr_t buffer_phys[16];
    unsigned buffer_count;
    size_t buffer_bytes;
    volatile unsigned queued;
    uint8_t read_index;
    uint8_t write_index;
    int running;
    uint32_t completed;
    uint32_t underruns;
    uint32_t errors;
    struct pcidevice pci;
};

int ac97(struct ac97 *self);
const char *ac97_info(struct ac97 *self, char *buffer, size_t size);

#endif /* MONARCH_DRIVERS_SOUND_AC97_H */
