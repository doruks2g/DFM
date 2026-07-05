#ifndef DFM_FS_H
#define DFM_FS_H

#include "common.h"

/* ---------------------------------------------------------------------
 * DirView: Bir dizinin "sanal" görünümü.
 *
 * Lazy Loading Notu:
 *   - fs_open_dir() dizini tarar ama sadece isimleri (readdir) toplar.
 *   - stat() çağrısı YAPILMAZ. stat pahalıdır; 5000 dosyalık bir
 *     dizinde 5000 stat çağrısı gecikmeye yol açar.
 *   - fs_ensure_stat() sadece ekranda GÖRÜNEN aralık (visible window)
 *     için çağrılır; bunu main.c render öncesi yapar.
 * --------------------------------------------------------------------- */

typedef struct {
    char *arena;            /* Zero-copy: tüm isimlerin tutulduğu tek buffer */
    size_t arena_size;
    size_t arena_used;

    FileEntry *entries;      /* Dinamik dizi (linked list yerine array - cache-friendly) */
    size_t count;
    size_t capacity;

    char cwd[4096];
} DirView;

/* Dizini aç ve isim listesini oluştur (stat YOK - lazy) */
int fs_open_dir(DirView *dv, const char *path);

/* Belleği serbest bırak */
void fs_free_dir(DirView *dv);

/* Tek bir entry için stat bilgisini doldur (henüz yapılmadıysa) */
void fs_ensure_stat(DirView *dv, size_t index);

/* Sadece [start, end) aralığındaki entry'ler için stat çağır.
 * main.c bunu her render öncesi SADECE görünür satırlar için çağırır. */
void fs_ensure_stat_range(DirView *dv, size_t start, size_t end);

/* Zero-copy erişim: entry'nin adını döndürür (arena içinde pointer, kopya değil) */
static inline const char *fs_entry_name(const DirView *dv, size_t index) {
    return dv->arena + dv->entries[index].name_offset;
}

#endif /* DFM_FS_H */
