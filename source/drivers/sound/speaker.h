#ifndef MONARCH_DRIVERS_SOUND_SPEAKER_H
#define MONARCH_DRIVERS_SOUND_SPEAKER_H 1

#include "base/api/monarch.h"

struct speaker {
    void (*beep)(struct speaker *self, uint32_t frequency, uint32_t duration);
};

void speaker(struct speaker *self);

#endif /* MONARCH_DRIVERS_SOUND_SPEAKER_H */
