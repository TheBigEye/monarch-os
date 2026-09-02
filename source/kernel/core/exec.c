/**
 * @file exec.c
 * @brief High-level ELF process execution and exec replacement helpers.
 *
 */

#include "kernel/core/exec.h"
#include "arch/x86/context.h"
#include "arch/x86/paging.h"
#include "kernel/core/elf.h"
#include "kernel/core/debug.h"
#include "kernel/memory/heap.h"
#include "kernel/scheduler/thread.h"

static const char *last_error = "ok";

static void fail(const char *message) {
    last_error = message;
}

const char *execerror(void) {
    return last_error;
}

static void enter(void *arg) {
    struct process *proc = processcurrent();
    uintptr_t entry = (uintptr_t)arg;

    if (entry && proc && processstack(proc)) {
        userenter(processstack(proc), entry);
    }

    processfinish(proc, 127);
    threadexit();
}

static char *readfile(struct vfs *fs, struct process *owner, const char *path, size_t *out_size) {
    struct vstat st;
    struct vfile *file;
    char *buffer;
    size_t done = 0;
    char oldcwd[VFS_PATH];
    int restore = 0;

    if (!fs || !path || !out_size) {
        fail("bad exec arguments");
        return nil;
    }

    if (owner && fs->cwd(fs, oldcwd, sizeof(oldcwd))) {
        restore = 1;
        fs->chdir(fs, processcwd(owner));
    }

    if (!fs->stat(fs, path, &st) || st.type != VFS_FILE) {
        if (restore) {
            fs->chdir(fs, oldcwd);
        }
        fail("executable not found");
        return nil;
    }

    buffer = kmalloc(st.size + 1u);
    if (!buffer) {
        if (restore) {
            fs->chdir(fs, oldcwd);
        }
        fail("out of memory reading executable");
        return nil;
    }

    file = fs->open(fs, path, "r");
    if (!file) {
        kfree(buffer);
        if (restore) {
            fs->chdir(fs, oldcwd);
        }
        fail("cannot open executable");
        return nil;
    }

    while (done < st.size) {
        int count = file->read(file, buffer + done, st.size - done);
        if (count < 0) {
            file->close(file);
            kfree(buffer);
            if (restore) {
                fs->chdir(fs, oldcwd);
            }
            fail("cannot read executable");
            return nil;
        }
        if (count == 0) {
            KLOG("exec", "short read path=%s done=%u expected=%u", path, (unsigned)done, (unsigned)st.size);
            break;
        }
        done += (size_t)count;
    }

    file->close(file);
    if (restore) {
        fs->chdir(fs, oldcwd);
    }

    buffer[done] = '\0';
    *out_size = done;
    fail("ok");
    return buffer;
}

static int firstword(const char *command, char *path, size_t size) {
    size_t used = 0;

    if (!command || !path || !size) {
        return 0;
    }

    while (space(*command)) {
        command++;
    }
    if (!*command) {
        return 0;
    }

    while (*command && !space(*command)) {
        if (used + 1u < size) {
            path[used++] = *command;
        }
        command++;
    }
    path[used] = '\0';
    return used > 0;
}

/**
 * Spawn a new ELF process with inherited descriptors and an optional env block.
 *
 * This function is the educational equivalent of the small part of fork+exec
 * that Monarch currently supports: create a process, create an address space,
 * load the ELF, build argv/envp, then start a scheduler thread for it.
 */
int execspawnenv(struct vfs *fs, const char *path, const char *envblock, size_t envsize, struct process *parent, struct execresult *out) {
    struct elfimage image;
    struct process *child;
    struct thread *thread;
    char executable[VFS_PATH];
    char *buffer;
    size_t size;

    if (out) {
        memset(out, 0, sizeof(*out));
    }

    KLOG("exec", "spawn command=%s parent=%u", path ? path : "(null)", parent ? parent->pid : 0);
    if (!firstword(path, executable, sizeof(executable))) {
        fail("missing executable path");
        return 0;
    }

    buffer = readfile(fs, parent, executable, &size);
    if (!buffer) {
        return 0;
    }

    if (!elfinfo(buffer, size, &image)) {
        KLOG("exec", "elfinfo failed executable=%s bytes=%u error=%s", executable, (unsigned)size, elferror());
        fail(elferror());
        kfree(buffer);
        return 0;
    }

    child = processspawn(executable);
    if (!child) {
        kfree(buffer);
        fail("process table full");
        return 0;
    }

    if (parent) {
        child->ppid = parent->pid;
        if (!processinherit(child, parent, fs)) {
            processreap(child);
            kfree(buffer);
            fail("cannot inherit process state");
            return 0;
        }
    }

    if (!processnewspace(child)) {
        processreap(child);
        kfree(buffer);
        fail("cannot create address space");
        KLOG("exec", "spawn %s failed: address space", executable);
        return 0;
    }

    KLOG("exec", "pid=%u space=%p load %s", child->pid, (void *)processspace(child), executable);
    if (!elfloadat(buffer, size, &image, processspace(child))) {
        fail(elferror());
        processreap(child);
        kfree(buffer);
        return 0;
    }

    if (envblock) {
        if (!processsetenvblock(child, envblock, envsize)) {
            processreap(child);
            kfree(buffer);
            fail("cannot build process environment");
            return 0;
        }
    } else if (!processcopyenv(child, parent)) {
        processreap(child);
        kfree(buffer);
        fail("cannot inherit process environment");
        return 0;
    }

    if (!processsetargs(child, path) || !processbuildstack(child)) {
        processreap(child);
        kfree(buffer);
        fail("cannot build process stack");
        return 0;
    }

    processsetimage(child, image.start, image.end, image.entry);
    thread = threadspawnfor(child, executable, enter, (void *)image.entry);
    if (!thread) {
        processreap(child);
        kfree(buffer);
        fail("thread table full");
        return 0;
    }

    if (out) {
        out->pid = child->pid;
        out->tid = thread->tid;
        out->entry = image.entry;
        out->start = image.start;
        out->end = image.end;
    }

    KLOG("exec", "spawn ok pid=%u entry=%p image=%p..%p stack=%p", child->pid, (void *)image.entry, (void *)image.start, (void *)image.end, (void *)processstack(child));
    kfree(buffer);
    fail("ok");
    return 1;
}

int execspawn(struct vfs *fs, const char *path, struct process *parent, struct execresult *out) {
    return execspawnenv(fs, path, nil, 0, parent, out);
}



long execreplace(struct vfs *fs, const char *path) {
    struct process *proc = processcurrent();
    struct elfimage image;
    struct elfimage info;
    char command[VFS_PATH];
    char executable[VFS_PATH];
    char *buffer;
    size_t size;

    if (!proc || !fs || !path) {
        fail("bad exec arguments");
        return -1;
    }

    KLOG("exec", "replace pid=%u command=%s", proc->pid, path);
    strncpy(command, path, sizeof(command));
    if (!firstword(command, executable, sizeof(executable))) {
        fail("missing executable path");
        return -1;
    }

    buffer = readfile(fs, proc, executable, &size);
    if (!buffer) {
        return -1;
    }

    if (!elfinfo(buffer, size, &info)) {
        fail(elferror());
        kfree(buffer);
        return -1;
    }

    processcloexec(proc);

    if (processspace(proc) == pagekernel()) {
        if (!processnewspace(proc)) {
            kfree(buffer);
            processfinish(proc, 127);
            threadexit();
        }
    } else {
        processclearimage(proc);
    }

    if (!elfloadat(buffer, size, &image, processspace(proc))) {
        fail(elferror());
        kfree(buffer);
        processfinish(proc, 127);
        threadexit();
    }

    if (!processsetargs(proc, command) || !processbuildstack(proc)) {
        elfunloadat(&image, processspace(proc));
        kfree(buffer);
        processfinish(proc, 127);
        threadexit();
    }

    processsetname(proc, executable);
    processsetimage(proc, image.start, image.end, image.entry);
    kfree(buffer);

    pageswitch(processspace(proc));
    userenter(processstack(proc), image.entry);

    processfinish(proc, 127);
    threadexit();
}
