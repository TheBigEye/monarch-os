#include "kernel/debug/fdinfo.h"
#include "kernel/scheduler/process.h"

static struct process *owner(void) {
    return processcurrent();
}

static int valid(struct process *proc, int fd) {
    return proc && proc->used(proc, fd);
}

int fdinfo_used(int fd) {
    struct process *proc = owner();
    return valid(proc, fd);
}

int fdinfo_is_dir(int fd) {
    struct process *proc = owner();
    return valid(proc, fd) && proc->isdir(proc, fd);
}

uint32_t fdinfo_flags(int fd) {
    struct process *proc = owner();
    return valid(proc, fd) ? proc->flags(proc, fd) : 0;
}

uint32_t fdinfo_fdflags(int fd) {
    struct process *proc = owner();
    return valid(proc, fd) ? proc->fds[fd].fdflags : 0;
}

unsigned fdinfo_refs(int fd) {
    struct process *proc = owner();
    struct vfile *file;
    struct vdir *dir;

    if (!valid(proc, fd)) {
        return 0;
    }

    file = proc->file(proc, fd);
    if (file) {
        return file->_refs;
    }

    dir = proc->dir(proc, fd);
    return dir ? dir->_refs : 0;
}

const char *fdinfo_path(int fd) {
    struct process *proc = owner();
    return valid(proc, fd) ? proc->path(proc, fd) : nil;
}
