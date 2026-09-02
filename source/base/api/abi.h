#ifndef MONARCH_BASE_API_ABI_H
#define MONARCH_BASE_API_ABI_H 1

/**
 * @file abi.h
 * @brief Public Monarch ABI constants shared by kernel and userspace.
 *
 * This header contains numeric values that must stay identical on both sides of
 * the syscall boundary: syscall numbers, open flags, fcntl commands and file
 * descriptor flags.  Keeping them in `base/api` prevents the kernel dispatcher
 * and the userspace runtime from silently drifting apart.
 *
 * Monarch intentionally implements only a tiny Linux-like subset.  The syscall
 * numbers follow Linux i386 where a close educational equivalent exists, but the
 * semantics are often simplified.  `SYS_SLEEP`, for example, takes a millisecond
 * count instead of a Linux `timespec` pointer.
 */

/* open() flags. */
#define OREAD   0x01u
#define OWRITE  0x02u
#define OCREATE 0x04u
#define OAPPEND 0x08u
#define OTRUNC  0x10u
#define O_NONBLOCK 0x20u


/* fcntl()/ioctl() subset. */
#define F_DUPFD 0u
#define F_GETFD 1u
#define F_SETFD 2u
#define F_GETFL 3u
#define F_SETFL 4u
#define TCGETS  0x5401u
#define TIOCGWINSZ 0x5413u
#define TIOCSWINSZ 0x5414u
#define TCSETS 0x5402u

#define TERMIOS_ICANON 0x0001u
#define TERMIOS_ECHO   0x0002u
#define TERMIOS_ISIG   0x0004u

struct termios {
    uint32_t lflag;
};
#define AUDIO_GETINFO 0xA001u
#define KBD_GETEVENT  0xA101u
#define MOUSE_GETINFO 0xA102u
#define MOUSE_GETEVENT 0xA103u
#define FB_GETMAP     0xA104u
#define FB_BLIT       0xA105u

struct kbd_event {
    uint32_t code;
    uint32_t ascii;
    uint32_t pressed;
};

struct mouseinfo {
    int32_t x;
    int32_t y;
    int32_t z;
    uint32_t buttons;
};

/* One movement/button transition, consumed exactly once by MOUSE_GETEVENT. */
struct fb_blit {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t pitch;
    uintptr_t pixels;
};

struct fbmap {
    uint32_t address;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint32_t bytes;
    uint32_t bpp;
    uint32_t red_pos;
    uint32_t red_size;
    uint32_t green_pos;
    uint32_t green_size;
    uint32_t blue_pos;
    uint32_t blue_size;
};

struct mouse_event {
    int32_t x;
    int32_t y;
    int32_t dx;
    int32_t dy;
    int32_t dz;
    uint32_t buttons;
};

/* /dev/audio sample format identifiers. */
#define AUDIO_FMT_S16LE 1u

/* lseek() origins. */
#define SEEK_SET 0u
#define SEEK_CUR 1u
#define SEEK_END 2u

/* Descriptor flags. */
#define FD_CLOEXEC 0x01u

/* Readiness flags shared by poll/select-style userspace APIs. */
#define POLLIN   0x0001u
#define POLLOUT  0x0004u
#define POLLERR  0x0008u
#define POLLHUP  0x0010u
#define POLLNVAL 0x0020u

struct pollfd {
    int32_t fd;
    uint32_t events;
    uint32_t revents;
};

#define MONARCH_FD_SETSIZE 64u
typedef struct fd_set {
    uint64_t bits;
} fd_set;

struct winsize {
    uint16_t rows;
    uint16_t cols;
    uint16_t xpixel;
    uint16_t ypixel;
};

struct timeval {
    int32_t seconds;
    int32_t microseconds;
};

struct select_args {
    int32_t nfds;
    uintptr_t readfds;
    uintptr_t writefds;
    uintptr_t exceptfds;
    uintptr_t timeout;
};

#define FD_ZERO(set) ((set)->bits = 0u)
#define FD_SET(fd, set) ((set)->bits |= ((fd) >= 0 && (fd) < (int)MONARCH_FD_SETSIZE ? (1ull << (fd)) : 0u))
#define FD_CLR(fd, set) ((set)->bits &= ((fd) >= 0 && (fd) < (int)MONARCH_FD_SETSIZE ? ~(1ull << (fd)) : ~0ull))
#define FD_ISSET(fd, set) ((fd) >= 0 && (fd) < (int)MONARCH_FD_SETSIZE && (((set)->bits & (1ull << (fd))) != 0u))

/* Linux-like i386 syscall numbers plus Monarch extensions. */
#define SYS_EXIT      1u    /* Linux i386 exit */
#define SYS_READ      3u    /* Linux i386 read */
#define SYS_WRITE     4u    /* Linux i386 write */
#define SYS_OPEN      5u    /* Linux i386 open */
#define SYS_CLOSE     6u    /* Linux i386 close */
#define SYS_WAITPID   7u    /* Linux i386 waitpid, simplified */
#define SYS_CREATE    8u    /* Linux i386 creat */
#define SYS_UNLINK   10u    /* Linux i386 unlink */
#define SYS_EXECVE   11u    /* Linux i386 execve, simplified today */
#define SYS_CHDIR    12u    /* Linux i386 chdir */
#define SYS_TIME     13u    /* Linux i386 time, simplified */
#define SYS_LSEEK    19u    /* Linux i386 lseek, simplified */
#define SYS_GETPID   20u    /* Linux i386 getpid */
#define SYS_MOUNT    21u    /* Linux i386 mount, simplified */
#define SYS_UMOUNT   22u    /* Linux i386 umount, simplified */
#define SYS_MKDIR    39u    /* Linux i386 mkdir */
#define SYS_RMDIR    40u    /* Linux i386 rmdir */
#define SYS_DUP      41u    /* Linux i386 dup */
#define SYS_PIPE     42u    /* Linux i386 pipe */
#define SYS_IOCTL    54u    /* Linux i386 ioctl, tiny terminal subset */
#define SYS_FCNTL    55u    /* Linux i386 fcntl, tiny subset */
#define SYS_DUP2     63u    /* Linux i386 dup2 */
#define SYS_STAT    106u    /* Linux i386 newstat-like */
#define SYS_UNAME   122u    /* Linux i386 uname */
#define SYS_GETDENTS 141u   /* Linux i386 getdents, simplified */
#define SYS_YIELD   158u    /* Linux i386 sched_yield */
#define SYS_SLEEP   162u    /* Linux i386 nanosleep-like, simplified */
#define SYS_CWD     183u    /* Linux i386 getcwd */
#define SYS_SPAWN   400u    /* Monarch extension: spawn child process */
#define SYS_MOUNTS  401u    /* Monarch extension: copy mount table */
#define SYS_POLL    402u    /* Monarch extension: readiness wait */
#define SYS_SELECT  403u    /* Monarch extension: descriptor-set wait */
#define SYS_MKFIFO  404u    /* Monarch extension: named pipe endpoint */
#define SYS_SOCKETPAIR 405u /* Monarch extension: local socketpair */
#define SYS_SOCKET 406u
#define SYS_BIND 407u
#define SYS_LISTEN 408u
#define SYS_ACCEPT 409u
#define SYS_CONNECT 410u
#define SYS_PTYPAIR 411u
#define SYS_BRK 412u

#define AF_UNIX 1u
#define SOCK_STREAM 1u
#define UNIX_PATH_MAX 108u

struct sockaddr_un {
    uint32_t family;
    char path[UNIX_PATH_MAX];
};

struct socketpair_args {
    uint32_t domain;
    uint32_t type;
    uint32_t protocol;
    uintptr_t fds;
};


#define MOUNT_PATH 256u
#define MOUNT_TYPE 16u

struct mountent {
    char path[MOUNT_PATH];
    char type[MOUNT_TYPE];
};

/* Metadata copied out by ioctl(fd, AUDIO_GETINFO, &info). */
struct audioinfo {
    uint32_t ready;           /* Non-zero when the audio sink can accept PCM. */
    uint32_t format;          /* AUDIO_FMT_* value. */
    uint32_t sample_rate;     /* Frames per second expected by /dev/audio. */
    uint32_t channels;        /* Number of interleaved channels. */
    uint32_t bits_per_sample; /* Bits per channel sample. */
    uint32_t buffer_bytes;    /* Driver-side PCM buffering capacity. */
    uint32_t queued;          /* Driver-specific queued buffer count. */
    uint32_t running;         /* Non-zero when playback hardware is running. */
    uint32_t completed;       /* Completed buffer counter. */
    uint32_t underruns;       /* Buffer underrun counter. */
    uint32_t errors;          /* Hardware/driver error counter. */
};

#endif /* MONARCH_BASE_API_ABI_H */
