#ifndef DFM_SEARCH_H
#define DFM_SEARCH_H

#include <stddef.h>

/* ---------------------------------------------------------------------
 * dfind.c Entegrasyon Stratejisi
 * ---------------------------------------------------------------------
 * Orijinal dfind.c bir CLI aracıdır: sonuçları doğrudan printf ile
 * stdout'a basar ve int main() içinde çalışır. Dosya yöneticisi
 * içinden onu İKİ YOLDAN çağırabiliriz:
 *
 *   (A) Fork/Exec (Süreç İzolasyonu - "harici araç" gibi kullan)
 *       - dfind ayrı bir binary olarak derlenir.
 *       - search_run_external() fork() + execvp() ile onu çalıştırır,
 *         stdout'unu bir pipe'a yönlendirir (dup2), satır satır okur.
 *       - Avantaj: dfind.c'yi HİÇ DEĞİŞTİRMEZSİN. Çöker/kilitlenirse
 *         file manager etkilenmez (ayrı process).
 *       - Dezavantaj: fork() maliyeti + IPC overhead.
 *
 *   (B) Fonksiyon Olarak Gömme (In-Process - "kütüphane" gibi kullan)
 *       - dfind.c'nin search() fonksiyonu callback alacak şekilde
 *         hafifçe uyarlanır (printf yerine callback çağrılır).
 *       - search_run_inprocess() bunu doğrudan çağırır, sonuçlar
 *         bir SearchResultList içine toplanır.
 *       - Avantaj: Çok hızlı, ekstra process yok, sonuçları anlık
 *         (incremental) UI'da gösterebilirsin.
 *       - Dezavantaj: dfind.c'de küçük bir değişiklik gerekir
 *         (printf -> callback). Aşağıda bunu ayrı bir dosyada
 *         (dfind_core.c) izole ederek orijinali bozmadan yapıyoruz.
 *
 * BU PROJEDE: (B) fonksiyon-olarak-gömme tercih edildi çünkü arama
 * sırasında sonuçları anlık listelemek istiyoruz. (A) yöntemi de
 * search_run_external() olarak bırakıldı - istersen ileride ağır
 * aramaları ayrı process'te izole etmek için kullanabilirsin.
 * --------------------------------------------------------------------- */

typedef struct {
    char **paths;      /* Bulunan tam yollar (her biri malloc'lu) */
    size_t count;
    size_t capacity;
} SearchResultList;

void search_result_list_init(SearchResultList *list);
void search_result_list_free(SearchResultList *list);

/* Callback imzası: dfind_core her eşleşme bulduğunda bunu çağırır.
 * userdata olarak SearchResultList* geçilir. */
typedef void (*search_match_cb)(const char *full_path, void *userdata);

/* Yöntem B: in-process arama (dfind_core.c'deki uyarlanmış motoru kullanır) */
int search_run_inprocess(const char *start_path, const char *pattern,
                          SearchResultList *out_results);

/* Yöntem A: dfind binary'sini fork/exec ile çalıştır, çıktıyı satır satır oku.
 * dfind_binary_path: örn "./dfind" veya "/usr/local/bin/dfind" */
int search_run_external(const char *dfind_binary_path, const char *start_path,
                         const char *pattern, SearchResultList *out_results);

#endif /* DFM_SEARCH_H */
