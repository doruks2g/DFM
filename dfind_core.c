/*
 * dfind_core.c
 * ---------------------------------------------------------------------
 * Bu dosya, kullanicinin yazdigi dfind.c icindeki search() fonksiyonunun
 * BIREBIR AYNI algoritmasidir (recursive readdir + strstr eslesme).
 * TEK FARK: printf(...) ile ekrana basmak yerine, bir callback
 * fonksiyonunu cagirir. Boylece dfind.c dosyasi hic degistirilmeden
 * orijinal CLI araci olarak da calismaya devam eder (bkz. search_run_external),
 * ayni zamanda bu dosya file manager icine gomulu (in-process) hizli
 * arama icin kullanilir.
 *
 * dfind.c'de degisen/degismeyen noktalar:
 *   - . ve .. atlama mantigi          -> AYNI
 *   - strstr ile isim eslesme mantigi -> AYNI
 *   - DT_DIR ile recursive inis       -> AYNI
 *   - printf(CLR_BLUE ...)            -> callback(full_path, userdata)
 *   - found_count global degisken     -> SearchResultList->count (search.c'de)
 * --------------------------------------------------------------------- */

#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>

#include "../include/search.h"

void dfind_core_search(const char *path, const char *pattern,
                        search_match_cb cb, void *userdata) {
    struct dirent *entry;
    DIR *dir = opendir(path);

    if (!dir) return; /* dfind.c ile ayni: erisim izni olmayan klasoru sessizce gec */

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        if (strstr(entry->d_name, pattern) != NULL) {
            char full_path[4096];
            snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
            cb(full_path, userdata); /* dfind.c'deki printf yerine burasi */
        }

        if (entry->d_type == DT_DIR) {
            char next_path[4096];
            int len = snprintf(next_path, sizeof(next_path) - 1, "%s/%s", path, entry->d_name);
            if (len < (int)sizeof(next_path) - 1) {
                dfind_core_search(next_path, pattern, cb, userdata);
            }
        }
    }
    closedir(dir);
}
