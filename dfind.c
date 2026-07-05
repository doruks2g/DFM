#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>

// Renk tanımları - Işık hızındaki sonuçları süslemek için
#define CLR_BLUE  "\x1b[34m"
#define CLR_GREEN "\x1b[32m"
#define CLR_RED   "\x1b[31m"
#define CLR_RESET "\x1b[0m"

int found_count = 0;

void search(const char *path, const char *pattern) {
    struct dirent *entry;
    DIR *dir = opendir(path);

    if (!dir) return; // Erişim izni olmayan klasörleri sessizce geç (hız için)

    while ((entry = readdir(dir)) != NULL) {
        // . ve .. klasörlerini atla (sonsuz döngüyü önlemek için)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        // Eşleşme kontrolü (Isim içinde pattern var mı?)
        if (strstr(entry->d_name, pattern) != NULL) {
            printf("%s%s/%s%s\n", CLR_BLUE, path, entry->d_name, CLR_RESET);
            found_count++;
        }

        // Eğer bu bir klasörse içine dal (Recursive)
        if (entry->d_type == DT_DIR) {
            char next_path[4096];
            int len = snprintf(next_path, sizeof(next_path) - 1, "%s/%s", path, entry->d_name);
            if (len < sizeof(next_path) - 1) {
                search(next_path, pattern);
            }
        }
    }
    closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Kullanim: dfind <aranacak_kelime>\n");
        return 1;
    }

    const char *pattern = argv[1];
    
    // Aramayı bulunduğun klasörden başlat (.)
    search(".", pattern);

    if (found_count == 0) {
        printf("%s[-] Eslesme bulunamadi.%s\n", CLR_RED, CLR_RESET);
    } else {
        printf("\n%s[+] Toplam %d eslesme bulundu.%s\n", CLR_GREEN, found_count, CLR_RESET);
    }

    return 0;
}