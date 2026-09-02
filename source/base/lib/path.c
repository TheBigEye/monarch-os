/**
 * @file path.c
 * @brief Small path normalization and PATH-list helpers.
 */

#include "base/lib/path.h"

static int copy_text(char *out, size_t size, const char *text) {
    if (!out || !size || !text) {
        return 0;
    }
    if (strlen(text) + 1u > size) {
        out[0] = '\0';
        return 0;
    }
    strcpy(out, text);
    return 1;
}

static int copy_slice(char *out, size_t size, const char *text, size_t count) {
    if (!out || !size || (!text && count)) {
        return 0;
    }
    if (count + 1u > size) {
        out[0] = '\0';
        return 0;
    }
    if (count) {
        memcpy(out, text, count);
    }
    out[count] = '\0';
    return 1;
}

int path_is_absolute(const char *path) {
    return path && path[0] == '/';
}

int path_has_slash(const char *path) {
    return strchr(path, '/') != nil;
}

int path_join(const char *dir, const char *name, char *out, size_t size) {
    size_t used;

    if (!out || !size || !name) {
        return 0;
    }
    if (!dir || !*dir) {
        dir = ".";
    }

    if (path_is_absolute(name)) {
        return copy_text(out, size, name);
    }

    used = strlen(dir);
    if (used == 0 || strcmp(dir, ".") == 0) {
        return copy_text(out, size, name);
    }

    if (used + 1u + strlen(name) + 1u > size) {
        out[0] = '\0';
        return 0;
    }

    strcpy(out, dir);
    if (used > 1u || dir[0] != '/') {
        strcat(out, "/");
    }
    strcat(out, name);
    return 1;
}

int path_join_ext(const char *dir, const char *name, const char *ext, char *out, size_t size) {
    size_t used;

    if (!path_join(dir, name, out, size)) {
        return 0;
    }

    if (!ext || !*ext) {
        return 1;
    }

    used = strlen(out);
    if (used + strlen(ext) + 1u > size) {
        out[0] = '\0';
        return 0;
    }
    strcat(out, ext);
    return 1;
}

static int append_component(char *out, size_t size, const char *part, size_t length) {
    size_t used = strlen(out);
    int root = strcmp(out, "/") == 0;

    if (length == 0) {
        return 1;
    }
    if (used + (root ? 0u : 1u) + length + 1u > size) {
        return 0;
    }
    if (!root) {
        out[used++] = '/';
        out[used] = '\0';
    }
    memcpy(out + used, part, length);
    out[used + length] = '\0';
    return 1;
}

static void pop_component(char *out) {
    char *slash;

    if (!out || strcmp(out, "/") == 0) {
        return;
    }

    slash = strrchr(out, '/');
    if (!slash || slash == out) {
        strcpy(out, "/");
    } else {
        *slash = '\0';
    }
}

int path_normalize(const char *cwd, const char *path, char *out, size_t size) {
    char absolute[MONARCH_PATH_MAX];
    const char *p;

    if (!out || !size) {
        return 0;
    }
    if (!cwd || !*cwd) {
        cwd = "/";
    }
    if (!path || !*path) {
        path = ".";
    }

    if (path_is_absolute(path)) {
        if (!copy_text(absolute, sizeof(absolute), path)) {
            return 0;
        }
    } else if (strcmp(cwd, "/") == 0) {
        if (!path_join("/", path, absolute, sizeof(absolute))) {
            return 0;
        }
    } else if (!path_join(cwd, path, absolute, sizeof(absolute))) {
        return 0;
    }

    if (!copy_text(out, size, "/")) {
        return 0;
    }

    p = absolute;
    while (*p) {
        const char *part;
        size_t length;

        while (*p == '/') {
            p++;
        }
        if (!*p) {
            break;
        }

        part = p;
        while (*p && *p != '/') {
            p++;
        }
        length = (size_t)(p - part);

        if (length == 1u && part[0] == '.') {
            continue;
        }
        if (length == 2u && part[0] == '.' && part[1] == '.') {
            pop_component(out);
            continue;
        }
        if (!append_component(out, size, part, length)) {
            return 0;
        }
    }

    return 1;
}

void path_split_parent(const char *prefix, char *dir, size_t dir_size, char *shown, size_t shown_size, char *base, size_t base_size) {
    char *slash;
    size_t dir_len;

    if (!prefix || !*prefix) {
        (void)copy_text(dir, dir_size, ".");
        (void)copy_text(shown, shown_size, "");
        (void)copy_text(base, base_size, "");
        return;
    }

    slash = strrchr(prefix, '/');
    if (!slash) {
        (void)copy_text(dir, dir_size, ".");
        (void)copy_text(shown, shown_size, "");
        (void)copy_text(base, base_size, prefix);
        return;
    }

    dir_len = (size_t)(slash - prefix);
    if (dir_len == 0) {
        (void)copy_text(dir, dir_size, "/");
    } else {
        (void)copy_slice(dir, dir_size, prefix, dir_len);
    }

    (void)copy_slice(shown, shown_size, prefix, dir_len + 1u);
    (void)copy_text(base, base_size, slash + 1);
}

int path_stem_suffix(const char *name, const char *suffix, char *out, size_t size) {
    size_t name_len;
    size_t suffix_len;

    if (!name || !suffix || !out || !size) {
        return 0;
    }

    name_len = strlen(name);
    suffix_len = strlen(suffix);
    if (name_len <= suffix_len || strcmp(name + name_len - suffix_len, suffix) != 0) {
        out[0] = '\0';
        return 0;
    }

    return copy_slice(out, size, name, name_len - suffix_len);
}

int path_next_entry(const char **cursor, char *out, size_t size) {
    const char *p;
    size_t used = 0;

    if (!cursor || !*cursor || !out || !size) {
        return 0;
    }

    p = *cursor;
    while (*p == ':' || *p == '\n' || *p == '\r' || space(*p)) {
        p++;
    }
    if (!*p) {
        *cursor = p;
        out[0] = '\0';
        return 0;
    }

    while (*p && *p != ':' && *p != '\n' && *p != '\r' && !space(*p)) {
        if (used + 1u < size) {
            out[used++] = *p;
        }
        p++;
    }
    out[used] = '\0';
    *cursor = p;
    return used > 0;
}
