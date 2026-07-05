#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../include/fs.h"

#define INITIAL_ARENA_SIZE  (64 * 1024)   /* 64KB başlangıç - dosya adları için */
#define INITIAL_CAPACITY    256

/* Arena'ya yer yoksa büyüt (realloc). Entry'lerin offset'leri değişmez
 * çünkü offset kullanıyoruz, pointer değil - realloc sonrası hala geçerli. */
static int arena_grow(DirView *dv, size_t needed) {
    if (dv->arena_used + needed <= dv->arena_size) return 1;

    size_t new_size = dv->arena_size * 2;
    while (new_size < dv->arena_used + needed) new_size *= 2;

    char *tmp = realloc(dv->arena, new_size);
    if (!tmp) return 0;

    dv->arena = tmp;
    dv->arena_size = new_size;
    return 1;
}

static int entries_grow(DirView *dv) {
    if (dv->count < dv->capacity) return 1;

    size_t new_cap = dv->capacity * 2;
    FileEntry *tmp = realloc(dv->entries, new_cap * sizeof(FileEntry));
    if (!tmp) return 0;

    dv->entries = tmp;
    dv->capacity = new_cap;
    return 1;
}

static EntryType dtype_to_entrytype(unsigned char d_type) {
    switch (d_type) {
        case DT_DIR:  return ENTRY_DIR;
        case DT_LNK:  return ENTRY_LINK;
        case DT_REG:  return ENTRY_FILE;
        default:      return ENTRY_OTHER; /* DT_UNKNOWN dahil - stat lazy'de netleşir */
    }
}

int fs_open_dir(DirView *dv, const char *path) {
    memset(dv, 0, sizeof(*dv));

    dv->arena = malloc(INITIAL_ARENA_SIZE);
    if (!dv->arena) return -1;
    dv->arena_size = INITIAL_ARENA_SIZE;

    dv->entries = malloc(INITIAL_CAPACITY * sizeof(FileEntry));
    if (!dv->entries) { free(dv->arena); return -1; }
    dv->capacity = INITIAL_CAPACITY;

    /* Gerçek yolu sakla (relatif path'lerle karışmamak için) */
    if (!realpath(path, dv->cwd)) {
        strncpy(dv->cwd, path, sizeof(dv->cwd) - 1);
    }

    DIR *dir = opendir(dv->cwd);
    if (!dir) return -1;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0) continue;
        /* ".." dizin köke gelmediyse gösterilir (yukarı çıkış için) */

        size_t len = strlen(ent->d_name);

        if (!arena_grow(dv, len + 1)) break;
        if (!entries_grow(dv)) break;

        FileEntry *fe = &dv->entries[dv->count];
        fe->name_offset = (unsigned int)dv->arena_used;
        fe->name_len = (unsigned short)len;
        fe->type = dtype_to_entrytype(ent->d_type);
        fe->size = -1;      /* lazy: henüz bilinmiyor */
        fe->mode = 0;
        fe->stat_loaded = 0;

        memcpy(dv->arena + dv->arena_used, ent->d_name, len + 1); /* +1: NUL dahil */
        dv->arena_used += len + 1;

        dv->count++;
    }
    closedir(dir);
    return 0;
}

void fs_free_dir(DirView *dv) {
    free(dv->arena);
    free(dv->entries);
    memset(dv, 0, sizeof(*dv));
}

void fs_ensure_stat(DirView *dv, size_t index) {
    if (index >= dv->count) return;
    FileEntry *fe = &dv->entries[index];
    if (fe->stat_loaded) return;

    char full[8192];
    snprintf(full, sizeof(full), "%s/%s", dv->cwd, fs_entry_name(dv, index));

    struct stat st;
    if (lstat(full, &st) == 0) {
        fe->size = st.st_size;
        fe->mode = st.st_mode;
        if (fe->type == ENTRY_UNKNOWN || fe->type == ENTRY_OTHER) {
            if (S_ISDIR(st.st_mode)) fe->type = ENTRY_DIR;
            else if (S_ISLNK(st.st_mode)) fe->type = ENTRY_LINK;
            else if (S_ISREG(st.st_mode)) fe->type = ENTRY_FILE;
        }
    }
    fe->stat_loaded = 1;
}

void fs_ensure_stat_range(DirView *dv, size_t start, size_t end) {
    if (end > dv->count) end = dv->count;
    for (size_t i = start; i < end; i++) {
        fs_ensure_stat(dv, i);
    }
}
