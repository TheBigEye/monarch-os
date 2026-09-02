/**
 * @file process.h
 * @brief Process object, descriptor table and process-lifetime API.
 */

#ifndef MONARCH_KERNEL_SCHEDULER_PROCESS_H
#define MONARCH_KERNEL_SCHEDULER_PROCESS_H 1

#include "base/api/monarch.h"
#include "kernel/fs/vfs.h"

#define PROCESS_MAX 64
#define PROCESS_FDS 64
#define PROCESS_NAME 32
#define PROCESS_ARGS 16
#define PROCESS_ENVS 16
#define PROCESS_ENV_TEXT 1024u
#define PROCESS_STACK_TOP  0xBFFFE000u
#define PROCESS_STACK_SIZE 16384u

enum processstate {
    PROCESS_UNUSED = 0,
    PROCESS_READY,
    PROCESS_RUNNING,
    PROCESS_SLEEPING,
    PROCESS_ZOMBIE
};

enum fdtype {
    FD_NONE = 0,
    FD_FILE,
    FD_DIR
};

struct fdslot {
    int used;
    uint32_t flags;
    uint32_t fdflags;
    enum fdtype type;
    struct vfile *file;
    struct vdir *dir;
    char path[VFS_PATH];
};

struct process {
    int (*bind)(struct process *self, int fd, struct vfile *file, uint32_t flags);
    int (*binddir)(struct process *self, int fd, struct vdir *dir, const char *path, uint32_t flags);
    int (*alloc)(struct process *self, struct vfile *file, uint32_t flags);
    int (*allocdir)(struct process *self, struct vdir *dir, const char *path, uint32_t flags);
    struct vfile *(*file)(struct process *self, int fd);
    struct vdir *(*dir)(struct process *self, int fd);
    int (*close)(struct process *self, int fd);
    int (*used)(struct process *self, int fd);
    int (*isdir)(struct process *self, int fd);
    const char *(*path)(struct process *self, int fd);
    uint32_t (*flags)(struct process *self, int fd);

    uint32_t pid;
    uint32_t ppid;
    int exit_code;
    enum processstate state;
    char name[PROCESS_NAME];
    char cwd[VFS_PATH];
    int argc;
    char *argv[PROCESS_ARGS + 1u];
    char *args;
    int envc;
    char *env[PROCESS_ENVS + 1u];
    char *envs;
    uintptr_t space;
    uintptr_t stack_base;
    uintptr_t stack_top;
    uintptr_t stack_pointer;
    uintptr_t user_argv;
    uintptr_t user_env;
    uintptr_t image_start;
    uintptr_t image_end;
    uintptr_t image_entry;
    uintptr_t heap_base;
    uintptr_t heap_break;
    uintptr_t heap_limit;
    struct fdslot fds[PROCESS_FDS];
};

void process(struct process *proc, uint32_t pid, uint32_t ppid, const char *name);
struct process *processinit(const char *name);
struct process *processspawn(const char *name);
int processinherit(struct process *child, struct process *parent, struct vfs *fs);
void processcloseall(struct process *proc);
void processcloexec(struct process *proc);
void processfinish(struct process *proc, int status);
void processreap(struct process *proc);
struct process *processfind(uint32_t pid);
long processwait(int pid, int *status);
struct process *processcurrent(void);
void processswitch(struct process *proc);
void processsetstate(struct process *proc, enum processstate state);
void processsetname(struct process *proc, const char *name);
void processsetcwd(struct process *proc, const char *cwd);
const char *processcwd(struct process *proc);
void processclearargs(struct process *proc);
int processsetargs(struct process *proc, const char *command);
void processclearenv(struct process *proc);
int processsetenvblock(struct process *proc, const char *block, size_t size);
int processcopyenv(struct process *dst, struct process *src);
int processargc(struct process *proc);
char **processargv(struct process *proc);
int processenvc(struct process *proc);
char **processenv(struct process *proc);
uintptr_t processspace(struct process *proc);
int processnewspace(struct process *proc);
void processsetspace(struct process *proc, uintptr_t space);
void processclearstack(struct process *proc);
int processbuildstack(struct process *proc);
uintptr_t processstack(struct process *proc);
char **processuserargv(struct process *proc);
char **processuserenv(struct process *proc);
void processsetimage(struct process *proc, uintptr_t start, uintptr_t end, uintptr_t entry);
void processclearimage(struct process *proc);
void processeach(void (*iter)(void *ctx, struct process *proc), void *ctx);
const char *processstate(enum processstate state);

#endif /* MONARCH_KERNEL_SCHEDULER_PROCESS_H */
