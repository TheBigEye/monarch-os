#ifndef MONARCH_KERNEL_DEBUG_FDINFO_H
#define MONARCH_KERNEL_DEBUG_FDINFO_H 1

/**
 * @file fdinfo.h
 * @brief Kernel-debug helpers for inspecting the current process descriptor table.
 *
 * These helpers are not syscalls and they are not part of the architecture ABI.
 * They exist so the kernel/debug shell can print useful descriptor state without
 * reaching into the syscall dispatcher.  Keeping them here makes the ownership
 * clear: fdinfo is kernel diagnostics code built on top of the process object.
 */

#include "base/api/monarch.h"

/** Return non-zero when `fd` is open in the current process. */
int fdinfo_used(int fd);

/** Return non-zero when `fd` refers to an open directory handle. */
int fdinfo_is_dir(int fd);

/** Return open flags such as OREAD/OWRITE/OAPPEND for `fd`. */
uint32_t fdinfo_flags(int fd);

/** Return descriptor flags such as FD_CLOEXEC for `fd`. */
uint32_t fdinfo_fdflags(int fd);

/** Return the shared file/directory object reference count behind `fd`. */
unsigned fdinfo_refs(int fd);

/** Return the remembered path for `fd`, or nil when no path is available. */
const char *fdinfo_path(int fd);

#endif /* MONARCH_KERNEL_DEBUG_FDINFO_H */
