/**
 * @file ush.c
 * @brief Userspace shell: builtins, PATH lookup, local environment and child spawning.
 */

#include "base/usr/sys.h"
#include "base/lib/path.h"

#define LINE_MAX 256u
#define PATH_MAX 256u
#define ENV_MAX 16u
#define ENV_NAME 32u
#define ENV_VALUE 160u
#define HISTORY_MAX 16u

struct envvar {
    int used;
    char name[ENV_NAME];
    char value[ENV_VALUE];
};

static struct envvar envs[ENV_MAX];
static char history[HISTORY_MAX][LINE_MAX];
static char history_draft[LINE_MAX];
static unsigned history_count;

static int complete_line(char *line, size_t size, size_t *used, size_t *cursor);
static void prompt(void);

static void movecursor(int delta) {
    if (delta > 0) {
        puts("\033[");
        putu((uint32_t)delta);
        putch('C');
    } else if (delta < 0) {
        puts("\033[");
        putu((uint32_t)-delta);
        putch('D');
    }
}

static void redrawtail(char *line, size_t used, size_t cursor) {
    for (size_t i = cursor; i < used; i++) {
        putch(line[i]);
    }
    putch(' ');
    movecursor(-((int)(used - cursor) + 1));
}

static void redrawline(char *line, size_t used, size_t cursor, size_t old_used, size_t old_cursor) {
    size_t clear = max(used, old_used);

    movecursor(-(int)old_cursor);
    for (size_t i = 0; i < used; i++) {
        putch(line[i]);
    }
    for (size_t i = used; i <= clear; i++) {
        putch(' ');
    }
    movecursor(-((int)(clear - cursor + 1u)));
}

static int nonblank(const char *text) {
    while (text && *text) {
        if (!space(*text)) {
            return 1;
        }
        text++;
    }
    return 0;
}

static void historyadd(const char *line) {
    if (!nonblank(line)) {
        return;
    }
    if (history_count && strcmp(history[history_count - 1u], line) == 0) {
        return;
    }
    if (history_count == HISTORY_MAX) {
        memmove(history[0], history[1], sizeof(history[0]) * (HISTORY_MAX - 1u));
        history_count--;
    }
    strncpy(history[history_count++], line, LINE_MAX);
}

static int historypick(int direction, int *view, char *line, size_t size) {
    if (!view || !line || !size || !history_count) {
        return 0;
    }

    if (direction < 0) {
        if (*view < 0) {
            *view = (int)history_count;
            strncpy(history_draft, line, sizeof(history_draft));
        }
        if (*view > 0) {
            (*view)--;
            strncpy(line, history[*view], size);
            return 1;
        }
        return 0;
    }

    if (*view < 0) {
        return 0;
    }
    if ((unsigned)(*view + 1) < history_count) {
        (*view)++;
        strncpy(line, history[*view], size);
    } else {
        *view = -1;
        strncpy(line, history_draft, size);
    }
    return 1;
}

static int escapeinput(char *line, size_t size, size_t *used, size_t *cursor, int *history_view) {
    char a;
    char b;

    if (read(0, &a, 1) <= 0) {
        return 0;
    }
    if (a != '[') {
        return 1;
    }
    if (read(0, &b, 1) <= 0) {
        return 0;
    }

    if (b == 'A' || b == 'B') {
        size_t old_used = *used;
        size_t old_cursor = *cursor;
        if (historypick(b == 'A' ? -1 : 1, history_view, line, size)) {
            *used = strlen(line);
            *cursor = *used;
            redrawline(line, *used, *cursor, old_used, old_cursor);
        }
        return 1;
    }
    if (b == 'D') {
        if (*cursor > 0) {
            (*cursor)--;
            movecursor(-1);
        }
        return 1;
    }
    if (b == 'C') {
        if (*cursor < *used) {
            (*cursor)++;
            movecursor(1);
        }
        return 1;
    }
    if (b == 'H') {
        movecursor(-(int)*cursor);
        *cursor = 0;
        return 1;
    }
    if (b == 'F') {
        movecursor((int)(*used - *cursor));
        *cursor = *used;
        return 1;
    }
    if (b == '3') {
        char tilde;
        (void)read(0, &tilde, 1);
        if (*cursor < *used) {
            memmove(line + *cursor, line + *cursor + 1u, *used - *cursor - 1u);
            (*used)--;
            line[*used] = '\0';
            redrawtail(line, *used, *cursor);
        }
        return 1;
    }

    return 1;
}

static int readline(char *line, size_t size, void (*promptfn)(void)) {
    size_t used = 0;
    size_t cursor = 0;
    int history_view = -1;

    if (!size) {
        return 0;
    }

    line[0] = '\0';
    while (used + 1u < size) {
        char ch;
        long got = read(0, &ch, 1);
        if (got < 0) {
            perror("ush: read");
            return -1;
        }
        if (got == 0) {
            continue;
        }

        if ((uint8_t)ch == 0x1Bu) {
            (void)escapeinput(line, size, &used, &cursor, &history_view);
            continue;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            movecursor((int)(used - cursor));
            putch('\n');
            break;
        }
        if (ch == '\t') {
            size_t old_used = used;
            size_t old_cursor = cursor;
            int action = complete_line(line, size, &used, &cursor);
            if (action == 1) {
                redrawline(line, used, cursor, old_used, old_cursor);
            } else if (action == 2 && promptfn) {
                promptfn();
                puts(line);
                movecursor(-((int)(used - cursor)));
            }
            continue;
        }
        if (ch == '\b') {
            if (cursor) {
                cursor--;
                movecursor(-1);
                memmove(line + cursor, line + cursor + 1u, used - cursor - 1u);
                used--;
                line[used] = '\0';
                redrawtail(line, used, cursor);
            }
            continue;
        }
        if (ch >= ' ') {
            memmove(line + cursor + 1u, line + cursor, used - cursor);
            line[cursor++] = ch;
            used++;
            line[used] = '\0';
            putch(ch);
            if (cursor < used) {
                redrawtail(line, used, cursor);
            }
        }
    }

    line[used] = '\0';
    return (int)used;
}


static char *skip(char *text) {
    while (*text && space(*text)) {
        text++;
    }
    return text;
}

static char *nextword(char **cursor) {
    char *start;

    *cursor = skip(*cursor);
    if (!**cursor) {
        return nil;
    }

    start = *cursor;
    while (**cursor && !space(**cursor)) {
        (*cursor)++;
    }
    if (**cursor) {
        *(*cursor)++ = '\0';
    }
    return start;
}

static struct envvar *envslot(const char *name) {
    for (unsigned i = 0; i < ENV_MAX; i++) {
        if (envs[i].used && strcmp(envs[i].name, name) == 0) {
            return &envs[i];
        }
    }
    return nil;
}

static const char *envget(const char *name) {
    struct envvar *slot = envslot(name);
    return slot ? slot->value : nil;
}

static int setenv(const char *name, const char *value) {
    struct envvar *slot;

    if (!name || !*name || strchr(name, '=')) {
        return 0;
    }

    slot = envslot(name);
    if (!slot) {
        for (unsigned i = 0; i < ENV_MAX; i++) {
            if (!envs[i].used) {
                slot = &envs[i];
                slot->used = 1;
                strncpy(slot->name, name, sizeof(slot->name));
                break;
            }
        }
    }

    if (!slot) {
        return 0;
    }

    strncpy(slot->value, value ? value : "", sizeof(slot->value));
    return 1;
}

static int unsetenv(const char *name) {
    struct envvar *slot = envslot(name);
    if (!slot) {
        return 0;
    }
    memset(slot, 0, sizeof(*slot));
    return 1;
}

static void printenv(void) {
    for (unsigned i = 0; i < ENV_MAX; i++) {
        if (envs[i].used) {
            puts(envs[i].name);
            putch('=');
            puts(envs[i].value);
            putch('\n');
        }
    }
}

static int varchar(char ch) {
    return (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '_';
}

static int addchar(char *out, size_t size, size_t *used, char ch) {
    if (*used + 1u >= size) {
        return 0;
    }
    out[(*used)++] = ch;
    out[*used] = '\0';
    return 1;
}

static int addtext(char *out, size_t size, size_t *used, const char *text) {
    while (text && *text) {
        if (!addchar(out, size, used, *text++)) {
            return 0;
        }
    }
    return 1;
}

/**
 * Expand the tiny set of shell variables supported by ush.
 *
 * Supported forms are $NAME, ${NAME}, and $? for the last foreground status.
 * Single quotes suppress expansion and double quotes allow it.  This is enough
 * to feel Unix-like without pretending to be a full POSIX shell.
 */
static int expandvars(const char *in, char *out, size_t size, int last) {
    size_t used = 0;
    int single = 0;
    int escape = 0;
    char number[16];

    if (!in || !out || !size) {
        return 0;
    }

    out[0] = '\0';
    while (*in) {
        if (escape) {
            if (!addchar(out, size, &used, *in++)) {
                return 0;
            }
            escape = 0;
            continue;
        }

        if (*in == '\\') {
            if (!addchar(out, size, &used, *in++)) {
                return 0;
            }
            escape = 1;
            continue;
        }

        if (*in == '\'') {
            single = !single;
            if (!addchar(out, size, &used, *in++)) {
                return 0;
            }
            continue;
        }

        if (!single && *in == '$') {
            char name[ENV_NAME];
            size_t n = 0;
            const char *value;

            in++;
            if (*in == '?') {
                snprintf(number, sizeof(number), "%d", last);
                if (!addtext(out, size, &used, number)) {
                    return 0;
                }
                in++;
                continue;
            }

            if (*in == '{') {
                in++;
                while (*in && *in != '}' && n + 1u < sizeof(name)) {
                    name[n++] = *in++;
                }
                if (*in == '}') {
                    in++;
                }
            } else {
                while (*in && varchar(*in) && n + 1u < sizeof(name)) {
                    name[n++] = *in++;
                }
            }
            name[n] = '\0';

            if (n == 0) {
                if (!addchar(out, size, &used, '$')) {
                    return 0;
                }
                continue;
            }

            value = envget(name);
            if (value && !addtext(out, size, &used, value)) {
                return 0;
            }
            continue;
        }

        if (!addchar(out, size, &used, *in++)) {
            return 0;
        }
    }

    return 1;
}


static size_t envblock(char *buffer, size_t size) {
    size_t used = 0;

    if (!buffer || !size) {
        return 0;
    }

    for (unsigned i = 0; i < ENV_MAX; i++) {
        size_t need;
        if (!envs[i].used) {
            continue;
        }

        need = strlen(envs[i].name) + 1u + strlen(envs[i].value) + 1u;
        if (used + need + 1u > size) {
            break;
        }

        strcpy(buffer + used, envs[i].name);
        used += strlen(envs[i].name);
        buffer[used++] = '=';
        strcpy(buffer + used, envs[i].value);
        used += strlen(envs[i].value);
        buffer[used++] = '\0';
    }

    if (used < size) {
        buffer[used++] = '\0';
    }
    return used;
}

static void envinit(void) {
    char path[ENV_VALUE];
    long fd = open("/initrd/etc/path", OREAD);

    if (fd >= 0) {
        long got = read((int)fd, path, sizeof(path) - 1u);
        close((int)fd);
        if (got > 0) {
            path[got] = '\0';
            for (long i = 0; i < got; i++) {
                if (path[i] == '\n' || path[i] == '\r') {
                    path[i] = ':';
                }
            }
            setenv("PATH", path);
        }
    }

    if (!envget("PATH")) {
        setenv("PATH", "/initrd/bin");
    }
    setenv("HOME", "/");
    setenv("SHELL", "/initrd/bin/ush.elf");
    setenv("PS1", "$ ");
}

static int pathelf(const char *name, char *path, size_t size) {
    char list[ENV_VALUE];
    char dir[PATH_MAX];
    const char *p;
    const char *envpath;
    struct stat st;

    if (!name || !path || !size) {
        return 0;
    }

    if (path_has_slash(name)) {
        strncpy(path, name, size);
        return stat(path, &st) == 0 && st.type == VFS_FILE;
    }

    envpath = envget("PATH");
    strncpy(list, envpath ? envpath : "/initrd/bin", sizeof(list));

    p = list;
    while (path_next_entry(&p, dir, sizeof(dir))) {
        if (path_join_ext(dir, name, ".elf", path, size) &&
            stat(path, &st) == 0 && st.type == VFS_FILE) {
            return 1;
        }
    }

    return 0;
}


#define COMPLETE_SHOW_MAX 12u

struct completion {
    unsigned count;
    unsigned shown;
    int overflow;
    char common[PATH_MAX];
    char one[PATH_MAX];
    char item[COMPLETE_SHOW_MAX][PATH_MAX];
    char suffix;
};

static const char *const builtin_names[] = {
    "help", "exit", "cd", "pwd", "status", "env", "set", "unset", "path", "which"
};

static void completion_add(struct completion *out, const char *text, char suffix) {
    size_t n = 0;

    if (!out || !text || !*text) {
        return;
    }

    for (unsigned i = 0; i < out->shown; i++) {
        if (strcmp(out->item[i], text) == 0) {
            return;
        }
    }

    if (out->shown < COMPLETE_SHOW_MAX) {
        strncpy(out->item[out->shown++], text, sizeof(out->item[0]));
    } else {
        out->overflow = 1;
    }

    if (out->count == 0) {
        strncpy(out->common, text, sizeof(out->common));
        strncpy(out->one, text, sizeof(out->one));
        out->suffix = suffix;
        out->count = 1;
        return;
    }

    while (out->common[n] && text[n] && out->common[n] == text[n]) {
        n++;
    }
    out->common[n] = '\0';
    out->count++;
}

static int replace_token(char *line, size_t size, size_t *used, size_t *cursor, size_t start, size_t end, const char *text) {
    size_t len;
    size_t tail;

    if (!line || !used || !cursor || !text || start > end || end > *used) {
        return 0;
    }

    len = strlen(text);
    if (*used - (end - start) + len + 1u > size) {
        return 0;
    }

    tail = *used - end;
    memmove(line + start + len, line + end, tail + 1u);
    if (len) {
        memcpy(line + start, text, len);
    }
    *used = *used - (end - start) + len;
    *cursor = start + len;
    return 1;
}

static int completion_apply(struct completion *matches, char *line, size_t size, size_t *used, size_t *cursor, size_t start, size_t end) {
    char text[PATH_MAX];
    const char *base;

    if (!matches || matches->count == 0) {
        return 0;
    }

    base = matches->count == 1u ? matches->one : matches->common;
    strncpy(text, base, sizeof(text));
    if (matches->count == 1u && matches->suffix && strlen(text) + 1u < sizeof(text)) {
        size_t len = strlen(text);
        text[len] = matches->suffix;
        text[len + 1u] = '\0';
    }

    if (strlen(text) <= end - start) {
        return 0;
    }

    return replace_token(line, size, used, cursor, start, end, text);
}

static void complete_paths(const char *prefix, struct completion *matches) {
    char dir[PATH_MAX];
    char shown[PATH_MAX];
    char base[DIRENT_NAME];
    struct dirent entries[8];
    long fd;
    long count;

    path_split_parent(prefix, dir, sizeof(dir), shown, sizeof(shown), base, sizeof(base));
    fd = open(dir, OREAD);
    if (fd < 0) {
        return;
    }

    while ((count = getdents((int)fd, entries, countof(entries))) > 0) {
        for (long i = 0; i < count; i++) {
            char text[PATH_MAX];
            if (!starts(entries[i].name, base)) {
                continue;
            }
            snprintf(text, sizeof(text), "%s%s", shown, entries[i].name);
            completion_add(matches, text, entries[i].type == VFS_DIR ? '/' : ' ');
        }
    }
    close((int)fd);
}

static void complete_external_commands(const char *prefix, struct completion *matches) {
    char list[ENV_VALUE];
    char dir[PATH_MAX];
    const char *p;
    const char *envpath = envget("PATH");

    strncpy(list, envpath ? envpath : "/initrd/bin", sizeof(list));
    p = list;
    while (path_next_entry(&p, dir, sizeof(dir))) {
        long fd;
        long count;
        struct dirent entries[8];

        fd = open(dir, OREAD);
        if (fd < 0) {
            continue;
        }
        while ((count = getdents((int)fd, entries, countof(entries))) > 0) {
            for (long i = 0; i < count; i++) {
                char name[DIRENT_NAME];
                if (entries[i].type == VFS_FILE && path_stem_suffix(entries[i].name, ".elf", name, sizeof(name)) && starts(name, prefix)) {
                    completion_add(matches, name, ' ');
                }
            }
        }
        close((int)fd);
    }
}

static void complete_builtins(const char *prefix, struct completion *matches) {
    for (size_t i = 0; i < countof(builtin_names); i++) {
        if (starts(builtin_names[i], prefix)) {
            completion_add(matches, builtin_names[i], ' ');
        }
    }
}

static size_t current_token_start(const char *line, size_t cursor) {
    size_t start = cursor;
    while (start > 0 && !space(line[start - 1u]) && line[start - 1u] != '|' && line[start - 1u] != '<' && line[start - 1u] != '>') {
        start--;
    }
    return start;
}

static int command_position(const char *line, size_t start) {
    int command = 1;

    for (size_t i = 0; i < start; i++) {
        if (line[i] == '|') {
            command = 1;
        } else if (!space(line[i])) {
            command = 0;
        }
    }
    return command;
}

static int has_quotes(const char *text) {
    while (text && *text) {
        if (*text == '\'' || *text == '"') {
            return 1;
        }
        text++;
    }
    return 0;
}

static void completion_print(const struct completion *matches) {
    if (!matches || matches->count <= 1u) {
        return;
    }

    putch('\n');
    for (unsigned i = 0; i < matches->shown; i++) {
        puts(matches->item[i]);
        puts("  ");
        if ((i % 3u) == 2u) {
            putch('\n');
        }
    }
    if (matches->shown && (matches->shown % 3u) != 0) {
        putch('\n');
    }
    if (matches->overflow) {
        puts("...\n");
    }
}

static int complete_line(char *line, size_t size, size_t *used, size_t *cursor) {
    struct completion matches;
    char prefix[PATH_MAX];
    size_t start;
    size_t len;

    if (!line || !used || !cursor || *cursor > *used) {
        return 0;
    }

    start = current_token_start(line, *cursor);
    len = *cursor - start;
    if (len >= sizeof(prefix)) {
        return 0;
    }
    memcpy(prefix, line + start, len);
    prefix[len] = '\0';
    if (has_quotes(prefix)) {
        return 0;
    }

    memset(&matches, 0, sizeof(matches));
    if (path_has_slash(prefix) || !command_position(line, start)) {
        complete_paths(prefix, &matches);
    } else {
        complete_builtins(prefix, &matches);
        complete_external_commands(prefix, &matches);
    }

    if (completion_apply(&matches, line, size, used, cursor, start, *cursor)) {
        return 1;
    }
    if (matches.count > 1u) {
        completion_print(&matches);
        return 2;
    }
    return 0;
}

static int external(const char *line, char *command, size_t size) {
    char temp[LINE_MAX];
    char path[PATH_MAX];
    char *cursor;
    char *name;
    char *rest;

    strncpy(temp, line, sizeof(temp));
    cursor = temp;
    name = nextword(&cursor);
    if (!name) {
        return 0;
    }

    rest = skip(cursor);
    if (!pathelf(name, path, sizeof(path))) {
        return 0;
    }

    if (*rest) {
        snprintf(command, size, "%s %s", path, rest);
    } else {
        snprintf(command, size, "%s", path);
    }
    return 1;
}


struct redirect {
    char input[PATH_MAX];
    char output[PATH_MAX];
    char error[PATH_MAX];
    int append;
    int errappend;
};

struct redirect_state {
    long stdin_copy;
    long stdout_copy;
    long stderr_copy;
};

static const char *skip_const(const char *text) {
    while (text && space(*text)) {
        text++;
    }
    return text;
}

static const char *copy_token(const char *text, char *out, size_t size) {
    size_t used = 0;
    int quote = 0;
    int escape = 0;

    if (!text || !out || !size) {
        return nil;
    }

    text = skip_const(text);
    if (!*text) {
        return nil;
    }

    while (*text) {
        char ch = *text;

        if (escape) {
            if (used + 1u < size) {
                out[used++] = ch;
            }
            escape = 0;
            text++;
            continue;
        }

        if (ch == '\\') {
            escape = 1;
            text++;
            continue;
        }

        if (quote) {
            if (ch == quote) {
                quote = 0;
            } else if (used + 1u < size) {
                out[used++] = ch;
            }
            text++;
            continue;
        }

        if (ch == '\'' || ch == '"') {
            quote = ch;
            text++;
            continue;
        }

        if (space(ch) || ch == '<' || ch == '>' || ch == '|') {
            break;
        }

        if (used + 1u < size) {
            out[used++] = ch;
        }
        text++;
    }

    out[used] = '\0';
    return used ? text : nil;
}

static int copy_command_char(char *out, size_t size, char ch, size_t *used) {
    if (*used + 1u >= size) {
        return 0;
    }
    out[(*used)++] = ch;
    out[*used] = '\0';
    return 1;
}

static int redirect_parse(const char *input, char *command, size_t size, struct redirect *redir) {
    const char *p = input;
    size_t used = 0;
    int quote = 0;
    int escape = 0;

    if (!input || !command || !size || !redir) {
        return 0;
    }

    memset(redir, 0, sizeof(*redir));
    command[0] = '\0';

    while (*p) {
        char ch = *p;

        if (escape) {
            if (!copy_command_char(command, size, ch, &used)) {
                return 0;
            }
            escape = 0;
            p++;
            continue;
        }

        if (ch == '\\') {
            if (!copy_command_char(command, size, ch, &used)) {
                return 0;
            }
            escape = 1;
            p++;
            continue;
        }

        if (quote) {
            if (!copy_command_char(command, size, ch, &used)) {
                return 0;
            }
            if (ch == quote) {
                quote = 0;
            }
            p++;
            continue;
        }

        if (ch == '\'' || ch == '"') {
            if (!copy_command_char(command, size, ch, &used)) {
                return 0;
            }
            quote = ch;
            p++;
            continue;
        }

        if (ch == '2' && p[1] == '>') {
            char target[PATH_MAX];
            int append = 0;

            if (p[2] == '>') {
                append = 1;
                p += 3;
            } else {
                p += 2;
            }

            p = copy_token(p, target, sizeof(target));
            if (!p || !target[0]) {
                return 0;
            }

            strncpy(redir->error, target, sizeof(redir->error));
            redir->errappend = append;
            continue;
        }

        if (ch == '<' || ch == '>') {
            char target[PATH_MAX];
            int append = 0;

            if (ch == '>' && p[1] == '>') {
                append = 1;
                p += 2;
            } else {
                p++;
            }

            p = copy_token(p, target, sizeof(target));
            if (!p || !target[0]) {
                return 0;
            }

            if (ch == '<') {
                strncpy(redir->input, target, sizeof(redir->input));
            } else {
                strncpy(redir->output, target, sizeof(redir->output));
                redir->append = append;
            }
            continue;
        }

        if (!copy_command_char(command, size, ch, &used)) {
            return 0;
        }
        p++;
    }

    {
        char *clean = trim(command);
        if (clean != command) {
            memmove(command, clean, strlen(clean) + 1u);
        }
    }
    return command[0] != '\0';
}

static void redirect_restore(struct redirect_state *state) {
    if (!state) {
        return;
    }

    if (state->stdin_copy >= 0) {
        dup2((int)state->stdin_copy, 0);
        close((int)state->stdin_copy);
        state->stdin_copy = -1;
    }
    if (state->stdout_copy >= 0) {
        dup2((int)state->stdout_copy, 1);
        close((int)state->stdout_copy);
        state->stdout_copy = -1;
    }
    if (state->stderr_copy >= 0) {
        dup2((int)state->stderr_copy, 2);
        close((int)state->stderr_copy);
        state->stderr_copy = -1;
    }
}

static int redirect_apply(const struct redirect *redir, struct redirect_state *state) {
    long fd;

    state->stdin_copy = -1;
    state->stdout_copy = -1;
    state->stderr_copy = -1;

    if (redir->input[0]) {
        state->stdin_copy = dup(0);
        if (state->stdin_copy >= 0) {
            fcntl((int)state->stdin_copy, F_SETFD, FD_CLOEXEC);
        }
        fd = open(redir->input, OREAD);
        if (state->stdin_copy < 0 || fd < 0 || dup2((int)fd, 0) < 0) {
            if (fd >= 0) {
                close((int)fd);
            }
            redirect_restore(state);
            eputs("redirect: cannot read ");
            eputs(redir->input);
            eputch('\n');
            return 0;
        }
        close((int)fd);
    }

    if (redir->output[0]) {
        uint32_t flags = OWRITE | OCREATE | (redir->append ? OAPPEND : OTRUNC);
        state->stdout_copy = dup(1);
        if (state->stdout_copy >= 0) {
            fcntl((int)state->stdout_copy, F_SETFD, FD_CLOEXEC);
        }
        fd = open(redir->output, flags);
        if (state->stdout_copy < 0 || fd < 0 || dup2((int)fd, 1) < 0) {
            if (fd >= 0) {
                close((int)fd);
            }
            redirect_restore(state);
            eputs("redirect: cannot write ");
            eputs(redir->output);
            eputch('\n');
            return 0;
        }
        close((int)fd);
    }

    if (redir->error[0]) {
        uint32_t flags = OWRITE | OCREATE | (redir->errappend ? OAPPEND : OTRUNC);
        state->stderr_copy = dup(2);
        if (state->stderr_copy >= 0) {
            fcntl((int)state->stderr_copy, F_SETFD, FD_CLOEXEC);
        }
        fd = open(redir->error, flags);
        if (state->stderr_copy < 0 || fd < 0 || dup2((int)fd, 2) < 0) {
            if (fd >= 0) {
                close((int)fd);
            }
            redirect_restore(state);
            eputs("redirect: cannot write ");
            eputs(redir->error);
            eputch('\n');
            return 0;
        }
        close((int)fd);
    }

    return 1;
}

static void help(void) {
    puts("ush builtins:\n");
    puts("  ush -c COMMAND   run one command line non-interactively\n");
    puts("  help             show this text\n");
    puts("  exit [CODE]      leave userspace shell\n");
    puts("  cd [DIR]         change directory, defaults to HOME\n");
    puts("  pwd              print current directory\n");
    puts("  status           print last child status\n");
    puts("  env              print shell environment\n");
    puts("  echo $VAR        variables expand before commands\n");
    puts("  set NAME VALUE   set shell variable\n");
    puts("  set NAME=VALUE   set shell variable\n");
    puts("  unset NAME       unset shell variable\n");
    puts("  path [VALUE]     show or set PATH\n");
    puts("  which NAME       show resolved executable\n");
    puts("redirection: <, >, >>, 2>, and 2>> work for builtins and commands\n");
    puts("pipelines: external commands can be connected with |\n");
    puts("line editing: arrows browse history, Tab completes commands/paths\n");
    puts("external commands are searched through PATH\n");
}

static int doset(char *cursor) {
    char *name;
    char *value;
    char *equal;

    name = nextword(&cursor);
    if (!name) {
        printenv();
        return 0;
    }

    equal = strchr(name, '=');
    if (equal) {
        *equal++ = '\0';
        value = equal;
    } else {
        value = skip(cursor);
    }

    if (!setenv(name, value)) {
        eputs("set: cannot set variable\n");
        return 1;
    }
    return 0;
}



#define PIPELINE_MAX 6

static char *find_pipe(char *text) {
    int quote = 0;
    int escape = 0;

    while (text && *text) {
        if (escape) {
            escape = 0;
        } else if (*text == '\\') {
            escape = 1;
        } else if (quote) {
            if (*text == quote) {
                quote = 0;
            }
        } else if (*text == '\'' || *text == '"') {
            quote = *text;
        } else if (*text == '|') {
            return text;
        }
        text++;
    }

    return nil;
}

static int has_pipe(const char *text) {
    return find_pipe((char *)text) != nil;
}

static void close_pipes(int pipes[][2], int count) {
    for (int i = 0; i < count; i++) {
        if (pipes[i][0] >= 0) {
            close(pipes[i][0]);
            pipes[i][0] = -1;
        }
        if (pipes[i][1] >= 0) {
            close(pipes[i][1]);
            pipes[i][1] = -1;
        }
    }
}

static int resolve_external(const char *line, char *command, size_t size) {
    if (!external(line, command, size)) {
        char temp[LINE_MAX];
        char *cursor;
        char *name;

        strncpy(temp, line, sizeof(temp));
        cursor = temp;
        name = nextword(&cursor);
        eputs("pipe: command not found: ");
        eputs(name ? name : line);
        eputch('\n');
        return 0;
    }
    return 1;
}

static int execute_pipeline_full(char *text);

static int execute_pipeline(char *text) {
    if (!text || !has_pipe(text)) {
        return -1;
    }
    return execute_pipeline_full(text);
}

static int execute_pipeline_full(char *text) {
    char temp[LINE_MAX];
    char raw[PIPELINE_MAX][PATH_MAX];
    char stage[PIPELINE_MAX][PATH_MAX];
    char commandv[PIPELINE_MAX][PATH_MAX];
    struct redirect redirs[PIPELINE_MAX];
    char *cursor;
    int stages = 0;
    int too_many = 0;
    int pipes[PIPELINE_MAX - 1][2];
    long pids[PIPELINE_MAX];
    int statusv[PIPELINE_MAX];
    long savein;
    long saveout;
    long saveerr;
    int spawned = 0;
    int result = 1;

    if (!text || !has_pipe(text)) {
        return -1;
    }

    for (int i = 0; i < PIPELINE_MAX - 1; i++) {
        pipes[i][0] = -1;
        pipes[i][1] = -1;
    }
    for (int i = 0; i < PIPELINE_MAX; i++) {
        pids[i] = -1;
        statusv[i] = 0;
    }
    memset(redirs, 0, sizeof(redirs));

    strncpy(temp, text, sizeof(temp));
    cursor = temp;

    while (stages < PIPELINE_MAX) {
        char *bar = find_pipe(cursor);
        if (bar) {
            *bar = '\0';
        }

        strncpy(raw[stages], trim(cursor), sizeof(raw[stages]));
        if (!raw[stages][0]) {
            eputs("pipe: empty stage\n");
            return 2;
        }
        stages++;

        if (!bar) {
            break;
        }
        cursor = bar + 1;
        if (stages == PIPELINE_MAX) {
            too_many = 1;
            break;
        }
    }

    if (too_many) {
        eputs("pipe: too many stages\n");
        return 2;
    }
    if (stages < 2) {
        return -1;
    }

    for (int i = 0; i < stages; i++) {
        if (!redirect_parse(raw[i], stage[i], sizeof(stage[i]), &redirs[i])) {
            eputs("pipe: bad stage\n");
            return 2;
        }
        if (redirs[i].input[0] && i != 0) {
            eputs("pipe: input redirection is only supported on the first stage\n");
            return 2;
        }
        if (redirs[i].output[0] && i + 1 != stages) {
            eputs("pipe: output redirection is only supported on the last stage\n");
            return 2;
        }
        if (redirs[i].error[0] && i + 1 != stages) {
            eputs("pipe: stderr redirection is only supported on the last stage\n");
            return 2;
        }
        if (!resolve_external(stage[i], commandv[i], sizeof(commandv[i]))) {
            return 127;
        }
    }

    for (int i = 0; i < stages - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            close_pipes(pipes, i);
            perror("pipe");
            return 1;
        }
        fcntl(pipes[i][0], F_SETFD, FD_CLOEXEC);
        fcntl(pipes[i][1], F_SETFD, FD_CLOEXEC);
    }

    savein = dup(0);
    saveout = dup(1);
    saveerr = dup(2);
    if (savein < 0 || saveout < 0 || saveerr < 0) {
        if (savein >= 0) close((int)savein);
        if (saveout >= 0) close((int)saveout);
        if (saveerr >= 0) close((int)saveerr);
        close_pipes(pipes, stages - 1);
        eputs("pipe: cannot save stdio\n");
        return 1;
    }
    fcntl((int)savein, F_SETFD, FD_CLOEXEC);
    fcntl((int)saveout, F_SETFD, FD_CLOEXEC);
    fcntl((int)saveerr, F_SETFD, FD_CLOEXEC);

    for (int i = 0; i < stages; i++) {
        long fd = -1;
        char envbuf[1024];
        size_t envsize;

        if (dup2((int)savein, 0) < 0 || dup2((int)saveout, 1) < 0 || dup2((int)saveerr, 2) < 0) {
            eputs("pipe: cannot restore stdio while spawning\n");
            break;
        }

        if (i > 0) {
            if (dup2(pipes[i - 1][0], 0) < 0) {
                eputs("pipe: cannot attach stdin\n");
                break;
            }
        } else if (redirs[i].input[0]) {
            fd = open(redirs[i].input, OREAD);
            if (fd < 0 || dup2((int)fd, 0) < 0) {
                if (fd >= 0) {
                    close((int)fd);
                }
                eputs("pipe: cannot read ");
                eputs(redirs[i].input);
                eputch('\n');
                break;
            }
            close((int)fd);
        }

        if (i + 1 < stages) {
            if (dup2(pipes[i][1], 1) < 0) {
                eputs("pipe: cannot attach stdout\n");
                break;
            }
        } else if (redirs[i].output[0]) {
            uint32_t flags = OWRITE | OCREATE | (redirs[i].append ? OAPPEND : OTRUNC);
            fd = open(redirs[i].output, flags);
            if (fd < 0 || dup2((int)fd, 1) < 0) {
                if (fd >= 0) {
                    close((int)fd);
                }
                eputs("pipe: cannot write ");
                eputs(redirs[i].output);
                eputch('\n');
                break;
            }
            close((int)fd);
        }

        if (redirs[i].error[0]) {
            uint32_t flags = OWRITE | OCREATE | (redirs[i].errappend ? OAPPEND : OTRUNC);
            fd = open(redirs[i].error, flags);
            if (fd < 0 || dup2((int)fd, 2) < 0) {
                if (fd >= 0) {
                    close((int)fd);
                }
                eputs("pipe: cannot write ");
                eputs(redirs[i].error);
                eputch('\n');
                break;
            }
            close((int)fd);
        }

        envsize = envblock(envbuf, sizeof(envbuf));
        pids[i] = spawnenv(commandv[i], envbuf, envsize);
        if (pids[i] < 0) {
            perror("spawn");
            break;
        }
        spawned++;
    }

    dup2((int)savein, 0);
    dup2((int)saveout, 1);
    dup2((int)saveerr, 2);
    close((int)savein);
    close((int)saveout);
    close((int)saveerr);
    close_pipes(pipes, stages - 1);

    for (int i = 0; i < spawned; i++) {
        int status = 0;
        if (waitpid((int)pids[i], &status) < 0) {
            perror("waitpid");
            result = 1;
        } else {
            statusv[i] = status;
            result = status;
        }
    }

    return spawned == stages ? statusv[stages - 1] : result;
}

static int execute_line(char *rawline, int previous, int *leaving) {
    struct redirect redir;
    struct redirect_state redirect_state;
    char clean[LINE_MAX];
    char parse[LINE_MAX];
    char command[PATH_MAX];
    char cwd[PATH_MAX];
    char *cmdline;
    char *cursor;
    char *cmd;
    long pid;
    int status = 0;
    int last = previous;

    if (leaving) {
        *leaving = 0;
    }

    if (!redirect_parse(rawline, clean, sizeof(clean), &redir)) {
        eputs("ush: bad command or redirection\n");
        return 2;
    }

    cmdline = skip(clean);
    if (!*cmdline) {
        return previous;
    }

    strncpy(parse, cmdline, sizeof(parse));
    cursor = parse;
    cmd = nextword(&cursor);
    if (!cmd) {
        return previous;
    }

    if (!redirect_apply(&redir, &redirect_state)) {
        return 1;
    }

    if (strcmp(cmd, "help") == 0) {
        help();
        last = 0;
        goto done;
    }
    if (strcmp(cmd, "clear") == 0) {
        puts("\033[2J\033[H");
        last = 0;
        goto done;
    }
    if (strcmp(cmd, "exit") == 0) {
        char *code = nextword(&cursor);
        last = code ? atoi(code) : previous;
        if (leaving) {
            *leaving = 1;
        }
        goto done;
    }
    if (strcmp(cmd, "cd") == 0) {
        char *dir = nextword(&cursor);
        if (!dir) {
            dir = (char *)envget("HOME");
        }
        if (!dir || !*dir) {
            eputs("cd: missing directory\n");
            last = 1;
        } else if (chdir(dir) < 0) {
            perror("cd");
            last = 1;
        } else {
            last = 0;
        }
        goto done;
    }
    if (strcmp(cmd, "pwd") == 0) {
        if (getcwd(cwd, sizeof(cwd)) < 0) {
            perror("pwd");
            last = 1;
        } else {
            puts(cwd);
            putch('\n');
            last = 0;
        }
        goto done;
    }
    if (strcmp(cmd, "status") == 0) {
        puti(previous);
        putch('\n');
        last = previous;
        goto done;
    }
    if (strcmp(cmd, "env") == 0) {
        printenv();
        last = 0;
        goto done;
    }
    if (strcmp(cmd, "set") == 0) {
        last = doset(cursor);
        goto done;
    }
    if (strcmp(cmd, "unset") == 0) {
        char *name = nextword(&cursor);
        last = name && unsetenv(name) ? 0 : 1;
        if (last) {
            eputs("unset: variable not found\n");
        }
        goto done;
    }
    if (strcmp(cmd, "path") == 0) {
        char *value = skip(cursor);
        if (*value) {
            last = setenv("PATH", value) ? 0 : 1;
        } else {
            puts(envget("PATH"));
            putch('\n');
            last = 0;
        }
        goto done;
    }
    if (strcmp(cmd, "which") == 0) {
        char *name = nextword(&cursor);
        char path[PATH_MAX];
        if (name && pathelf(name, path, sizeof(path))) {
            puts(path);
            putch('\n');
            last = 0;
        } else {
            eputs("which: not found\n");
            last = 1;
        }
        goto done;
    }

    if (!external(cmdline, command, sizeof(command))) {
        eputs(cmd);
        eputs(": command not found\n");
        last = 127;
        goto done;
    }

    {
        char envbuf[1024];
        size_t envsize = envblock(envbuf, sizeof(envbuf));
        pid = spawnenv(command, envbuf, envsize);
    }
    if (pid < 0) {
        perror("spawn");
        last = 127;
        goto done;
    }

    if (waitpid((int)pid, &status) < 0) {
        perror("waitpid");
        last = 1;
        goto done;
    }

    last = status;

done:
    redirect_restore(&redirect_state);
    return last;
}


static int run_command_text(char *line, int previous, int *leaving) {
    char expanded[LINE_MAX];
    char *cmdline;
    int pipe_status;

    if (leaving) {
        *leaving = 0;
    }

    if (!expandvars(line, expanded, sizeof(expanded), previous)) {
        eputs("ush: expanded line too long\n");
        return 1;
    }

    cmdline = skip(expanded);
    if (!*cmdline) {
        return previous;
    }

    pipe_status = execute_pipeline(cmdline);
    if (pipe_status >= 0) {
        return pipe_status;
    }

    return execute_line(cmdline, previous, leaving);
}

static int join_command_args(int argc, char **argv, int first, char *out, size_t size) {
    size_t used = 0;

    if (!out || !size || first >= argc) {
        return 0;
    }

    out[0] = '\0';
    for (int i = first; i < argc; i++) {
        const char *part = argv[i] ? argv[i] : "";
        size_t len = strlen(part);

        if (i > first) {
            if (used + 2u > size) {
                return 0;
            }
            out[used++] = ' ';
            out[used] = '\0';
        }
        if (used + len + 1u > size) {
            return 0;
        }
        memcpy(out + used, part, len);
        used += len;
        out[used] = '\0';
    }

    return 1;
}

static void prompt(void) {
    char cwd[PATH_MAX];
    const char *ps1 = envget("PS1");

    if (getcwd(cwd, sizeof(cwd)) < 0) {
        strcpy(cwd, "?");
    }
    puts("\033]12;green\a");
    if (cwd[0] == '/') {
        puts("\033[92m/");
        puts("\033[32m");
        puts(cwd + 1);
    } else {
        puts("\033[32m");
        puts(cwd);
    }
    puts("\033[92m ");
    puts(ps1 ? ps1 : "$ ");
    puts("\033[0m");
}

int main(int argc, char **argv) {
    char line[LINE_MAX];
    int last = 0;

    /* The graphical PTY master is reserved for WUSH. Keeping an inherited
       duplicate in USH would prevent the slave from reaching EOF on exit. */
    close(3);
    envinit();

    if (argc >= 3 && strcmp(argv[1], "-c") == 0) {
        char command[LINE_MAX];
        int leaving = 0;
        if (!join_command_args(argc, argv, 2, command, sizeof(command))) {
            eputs("ush: -c command too long\n");
            return 2;
        }
        return run_command_text(command, 0, &leaving);
    }

    puts("Monarch userspace shell (ush). Type 'help'.\n");

    for (;;) {
        prompt();

        if (readline(line, sizeof(line), prompt) < 0) {
            return 1;
        }
        historyadd(line);

        {
            int leaving = 0;
            last = run_command_text(line, last, &leaving);
            if (leaving) {
                return last;
            }
        }
    }
}
