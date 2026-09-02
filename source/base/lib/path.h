#ifndef MONARCH_BASE_LIB_PATH_H
#define MONARCH_BASE_LIB_PATH_H 1

/**
 * @file path.h
 * @brief Small path and PATH-list helpers shared by kernel and userspace.
 *
 * This is not a full POSIX pathname library.  It intentionally implements the
 * tiny operations Monarch currently repeats in the VFS, kernel debug shell and
 * userspace shell: slash detection, joining, normalisation, PATH iteration and
 * completion-friendly splitting.
 */

#include "base/api/monarch.h"

#define MONARCH_PATH_MAX 256u

/** Return non-zero when `path` starts at the filesystem root. */
int path_is_absolute(const char *path);

/** Return non-zero when `path` contains at least one slash. */
int path_has_slash(const char *path);

/** Join directory and name with exactly one slash when needed. */
int path_join(const char *dir, const char *name, char *out, size_t size);

/** Join directory, name and extension.  `ext` may be nil or empty. */
int path_join_ext(const char *dir, const char *name, const char *ext, char *out, size_t size);

/** Normalise `path` against `cwd`, handling '.', '..' and repeated slashes. */
int path_normalize(const char *cwd, const char *path, char *out, size_t size);

/**
 * Split a partially typed path for completion.
 *
 * Example: `/initrd/etc/pa` becomes:
 * - dir:   `/initrd/etc`
 * - shown: `/initrd/etc/`
 * - base:  `pa`
 */
void path_split_parent(const char *prefix, char *dir, size_t dir_size, char *shown, size_t shown_size, char *base, size_t base_size);

/** If `name` ends in `suffix`, copy the stem before it into `out`. */
int path_stem_suffix(const char *name, const char *suffix, char *out, size_t size);

/**
 * Read the next entry from a PATH-like list.
 *
 * Monarch accepts ':', newlines, carriage returns and ASCII whitespace as
 * separators so `/initrd/etc/path` can stay human-readable.
 */
int path_next_entry(const char **cursor, char *out, size_t size);

#endif /* MONARCH_BASE_LIB_PATH_H */
