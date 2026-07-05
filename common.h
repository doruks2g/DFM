#ifndef DFM_COMMON_H
#define DFM_COMMON_H

#include <sys/types.h>

/* ---------------------------------------------------------------------
 * Zero-Copy Strateji Notu:
 * FileEntry, dosya adını KENDİ İÇİNDE KOPYALAMAZ. Bunun yerine
 * NameArena (fs.c içinde tanımlı büyük bir char buffer) içindeki
 * bir konuma "name_offset" ile işaret eder. Böylece 5000 dosyalı
 * bir dizinde 5000 ayrı malloc + strcpy yerine TEK bir büyük
 * buffer ve 5000 tane offset/pointer tutulur.
 * --------------------------------------------------------------------- */

typedef enum {
    ENTRY_UNKNOWN = 0,
    ENTRY_FILE,
    ENTRY_DIR,
    ENTRY_LINK,
    ENTRY_OTHER
} EntryType;

typedef struct {
    unsigned int name_offset;  /* NameArena buffer'ında offset (zero-copy) */
    unsigned short name_len;
    EntryType type;
    off_t size;                /* Lazy: stat çağrılmadıysa -1 */
    mode_t mode;                /* Lazy: stat çağrılmadıysa 0 */
    char stat_loaded;           /* 0 = henüz stat edilmedi */
} FileEntry;

/* Terminal boyutu (ioctl TIOCGWINSZ ile doldurulur) */
typedef struct {
    unsigned short rows;
    unsigned short cols;
} TermSize;

#endif /* DFM_COMMON_H */
