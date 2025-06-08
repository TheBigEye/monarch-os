#ifndef _KERNEL_FILESYSTEM_H
#define _KERNEL_FILESYSTEM_H 1

#include "../../common/common.h"

#define MAX_NAME_LEN 32
#define MAX_CONTENT_LEN 1024

typedef struct File {
    uint32_t size;
    char *name;
    char *content;
    struct File *next;
} File;

typedef struct Directory {
    char *name;
    struct Directory *parent;
    struct Directory *subdirs;
    struct Directory *next;
    struct File *files;
} Directory;

extern Directory *BFS_PRIMARY_DIR;
extern Directory *BFS_CURRENT_DIR;

void mountFileSystem(void);

bool bfsCheckName(const char *name);

File *bfsCreateFile(Directory *parent, const char *name);
Directory *bfsCreateDirectory(Directory *parent, const char *name);

Directory *bfsFindDirectory(const char *path);
File *bfsFindFile(const char *path);
void bfsCopyFile(File *file, Directory *destination, const char *newName);

void bfsRemoveFile(Directory *parent, const char *name);
void bfsRemoveDirectory(Directory *directory);

void bfsWriteFile(File *file, const char *content);
void bfsReadFile(File *file);

void bfsPrintTree(Directory *directory, uint8_t level);

#endif /* _KERNEL_FILESYSTEM_H */
