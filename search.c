#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#include "../include/search.h"

/* dfind_core.c icinde tanimli - callback tabanli arama motoru */
extern void dfind_core_search(const char *path, const char *pattern,
                               search_match_cb cb, void *userdata);

void search_result_list_init(SearchResultList *list) {
    list->capacity = 64;
    list->paths = malloc(list->capacity * sizeof(char *));
    list->count = 0;
}

void search_result_list_free(SearchResultList *list) {
    for (size_t i = 0; i < list->count; i++) free(list->paths[i]);
    free(list->paths);
    list->paths = NULL;
    list->count = 0;
    list->capacity = 0;
}

static void list_push(SearchResultList *list, const char *path) {
    if (list->count >= list->capacity) {
        size_t new_cap = list->capacity * 2;
        char **tmp = realloc(list->paths, new_cap * sizeof(char *));
        if (!tmp) return;
        list->paths = tmp;
        list->capacity = new_cap;
    }
    list->paths[list->count++] = strdup(path);
}

/* dfind_core_search'un cagiracagi callback: sonucu listeye ekle */
static void on_match(const char *full_path, void *userdata) {
    SearchResultList *list = (SearchResultList *)userdata;
    list_push(list, full_path);
}

int search_run_inprocess(const char *start_path, const char *pattern,
                          SearchResultList *out_results) {
    if (!out_results->paths) search_result_list_init(out_results);
    dfind_core_search(start_path, pattern, on_match, out_results);
    return (int)out_results->count;
}

/* ---------------------------------------------------------------------
 * Yontem A: dfind binary'sini fork/exec ile calistir.
 * Kullanim senaryosu: dfind.c ayri derlenmis bir CLI aracı olarak
 * kalsin istersen (degistirmeden), bu fonksiyon onu bir alt-process
 * olarak calistirip stdout'unu pipe uzerinden okur.
 * --------------------------------------------------------------------- */
int search_run_external(const char *dfind_binary_path, const char *start_path,
                         const char *pattern, SearchResultList *out_results) {
    if (!out_results->paths) search_result_list_init(out_results);

    int pipefd[2];
    if (pipe(pipefd) != 0) return -1;

    pid_t pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return -1;
    }

    if (pid == 0) {
        /* Child: stdout'u pipe'in yazma ucuna yonlendir, dfind'i exec et */
        close(pipefd[0]);          /* okuma ucunu kapat */
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        /* dfind.c'nin calistigi dizini start_path yap */
        if (chdir(start_path) != 0) {
            _exit(127);
        }

        execlp(dfind_binary_path, dfind_binary_path, pattern, (char *)NULL);
        _exit(127); /* execlp basarisiz olursa */
    }

    /* Parent: yazma ucunu kapat, okuma ucundan satir satir oku */
    close(pipefd[1]);

    FILE *fp = fdopen(pipefd[0], "r");
    if (!fp) {
        close(pipefd[0]);
        waitpid(pid, NULL, 0);
        return -1;
    }

    char line[4096];
    while (fgets(line, sizeof(line), fp) != NULL) {
        /* dfind.c ciktisinda ANSI renk kodlari var (CLR_BLUE vs).
         * Basit bir temizlik: \x1b ile baslayan escape dizilerini at,
         * satir sonu \n karakterini kirp. */
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

        /* dfind cikti formati: "<ESC>[34m<path><ESC>[0m" veya
         * "[+] Toplam N eslesme..." gibi ozet satirlar da olabilir.
         * Sadece path gibi gorunen (ESC[34m ile baslayan) satirlari al. */
        if (strncmp(line, "\x1b[34m", 5) == 0) {
            char *path_start = line + 5;
            char *esc_end = strstr(path_start, "\x1b[0m");
            if (esc_end) *esc_end = '\0';
            list_push(out_results, path_start);
        }
    }

    fclose(fp); /* pipefd[0]'i da kapatir */
    waitpid(pid, NULL, 0);
    return (int)out_results->count;
}
