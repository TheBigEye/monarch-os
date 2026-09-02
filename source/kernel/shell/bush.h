#ifndef MONARCH_KERNEL_SHELL_BUSH_H
#define MONARCH_KERNEL_SHELL_BUSH_H 1

#include "drivers/char/tty.h"
#include "drivers/sound/ac97.h"
#include "drivers/sound/speaker.h"
#include "drivers/block/floppy.h"
#include "kernel/fs/vfs.h"

#define BUSH_LINE_MAX 256u
#define BUSH_HISTORY_MAX 16u

struct bush {
    void (*run)(struct bush *self);

    struct tty *_tty;
    struct vfs *_fs;
    struct speaker *_speaker;
    struct ac97 *_ac97;
    struct floppy_controller *_floppy;
    int _status;

    char _history[BUSH_HISTORY_MAX][BUSH_LINE_MAX];
    char _history_draft[BUSH_LINE_MAX];
    unsigned _history_count;
    int _history_view;
};

void bush(struct bush *self, struct tty *term, struct vfs *fs, struct speaker *pc, struct ac97 *audio, struct floppy_controller *floppy);

#endif /* MONARCH_KERNEL_SHELL_BUSH_H */
