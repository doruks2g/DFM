#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/select.h>
#include <signal.h>

#include "../include/common.h"
#include "../include/fs.h"
#include "../include/ui.h"
#include "../include/search.h"

static struct termios g_orig_termios;
static volatile sig_atomic_t g_resized = 0;

/* ---------------------------------------------------------------------
 * Raw Mode: termios.h ile terminali "ham" moda al.
 * - ICANON kapatilir: satir tamponlama yok, her tus aninda okunur.
 * - ECHO kapatilir: yazilan tuslar ekrana otomatik basilmaz (biz cizeriz).
 * - VMIN=0, VTIME=0: read() bloklamaz, veri yoksa hemen doner
 *   (ama biz zaten select() ile bekleyecegiz, bu ekstra guvenlik).
 * --------------------------------------------------------------------- */
static void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &g_orig_termios);
    struct termios raw = g_orig_termios;

    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |= (CS8);

    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &g_orig_termios);
}

static void on_sigwinch(int sig) {
    (void)sig;
    g_resized = 1;
}

/* ---------------------------------------------------------------------
 * Event-Driven Input: select() ile stdin'i bekle.
 * CPU tuketimi %0'da kalir cunku busy-loop / polling yok; kernel
 * bize veri gelene kadar process'i uyutur (block eder).
 * timeout: NULL verilirse sonsuza kadar bekler (tam event-driven).
 * --------------------------------------------------------------------- */
static int wait_for_input(void) {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    int ret = select(STDIN_FILENO + 1, &fds, NULL, NULL, NULL);
    return ret; /* >0: veri var, -1: sinyal ile kesildi (orn. SIGWINCH) */
}

typedef enum {
    KEY_NONE = 0,
    KEY_UP, KEY_DOWN, KEY_ENTER, KEY_QUIT, KEY_SEARCH,
    KEY_BACKSPACE_DIR, KEY_OTHER
} Key;

/* Ok tuslari ANSI escape sequence olarak gelir: ESC [ A/B/C/D */
static Key read_key(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return KEY_NONE;

    if (c == 'q' || c == 'Q') return KEY_QUIT;
    if (c == '/') return KEY_SEARCH;
    if (c == '\r' || c == '\n') return KEY_ENTER;
    if (c == 'h') return KEY_BACKSPACE_DIR;

    if (c == '\x1b') {
        char seq[2];
        /* Bir sonraki iki byte hemen gelmezse bu yalniz basina ESC'tir */
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_OTHER;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_OTHER;

        if (seq[0] == '[') {
            switch (seq[1]) {
                case 'A': return KEY_UP;
                case 'B': return KEY_DOWN;
                case 'D': return KEY_BACKSPACE_DIR; /* sol ok = ust dizin */
            }
        }
        return KEY_OTHER;
    }
    return KEY_OTHER;
}

/* Basit bir arama-modu prompt'u: kullanicidan pattern alir.
 * Not: raw mode acikken ECHO kapali oldugu icin manuel basiyoruz. */
static void prompt_search_pattern(char *out, size_t out_size, const TermSize *ts) {
    printf("\x1b[%d;1H\x1b[2K/", ts->rows);
    fflush(stdout);

    size_t len = 0;
    out[0] = '\0';

    while (1) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) continue;

        if (c == '\r' || c == '\n') break;
        if (c == 127 || c == 8) { /* backspace */
            if (len > 0) {
                len--;
                out[len] = '\0';
                printf("\b \b");
                fflush(stdout);
            }
            continue;
        }
        if (c == '\x1b') { out[0] = '\0'; break; } /* iptal */

        if (len < out_size - 1) {
            out[len++] = c;
            out[len] = '\0';
            putchar(c);
            fflush(stdout);
        }
    }
}

static void do_search_and_show(DirView *dv, const TermSize *ts) {
    char pattern[256];
    prompt_search_pattern(pattern, sizeof(pattern), ts);
    if (pattern[0] == '\0') return;

    SearchResultList results;
    search_result_list_init(&results);

    /* Yontem B: in-process arama (dfind_core.c uzerinden) */
    search_run_inprocess(dv->cwd, pattern, &results);

    /* Basit sonuc gosterimi: ekrani temizle, sonuclari listele, tusa bas */
    ui_clear_screen();
    printf("Arama sonucu: '%s'  (%zu eslesme)\r\n\r\n", pattern, results.count);
    size_t show_max = results.count < 40 ? results.count : 40;
    for (size_t i = 0; i < show_max; i++) {
        printf("  %s\r\n", results.paths[i]);
    }
    if (results.count > show_max) {
        printf("  ... ve %zu tane daha\r\n", results.count - show_max);
    }
    printf("\r\n[herhangi bir tusa basin]\r\n");
    fflush(stdout);

    char tmp;
    while (read(STDIN_FILENO, &tmp, 1) != 1) { /* bekle */ }

    search_result_list_free(&results);
    ui_clear_screen();
}

int main(int argc, char *argv[]) {
    const char *start_path = (argc > 1) ? argv[1] : ".";

    DirView dv;
    if (fs_open_dir(&dv, start_path) != 0) {
        fprintf(stderr, "Dizin acilamadi: %s\n", start_path);
        return 1;
    }

    enable_raw_mode();
    atexit(disable_raw_mode); /* programdan nasil cikilirsa cikilsin terminal duzelsin */
    signal(SIGWINCH, on_sigwinch);

    ScreenBuffer sb;
    ui_init(&sb);
    ui_clear_screen();
    ui_hide_cursor();

    TermSize ts;
    ui_get_termsize(&ts);

    size_t selected = 0;
    size_t top = 0;       /* virtual scroll ofseti */
    int running = 1;
    char status[8300] = "";

    while (running) {
        if (g_resized) {
            ui_get_termsize(&ts);
            ui_clear_screen();
            /* Buffer'i sifirla ki yeni boyuta gore her satir yeniden yazilsin */
            ui_free(&sb);
            ui_init(&sb);
            g_resized = 0;
        }

        int list_rows = ts.rows - 2;
        if (list_rows < 1) list_rows = 1;

        /* --- Lazy Loading: SADECE gorunur araligin stat'ini al --- */
        fs_ensure_stat_range(&dv, top, top + (size_t)list_rows);

        ui_render(&sb, &dv, selected, top, &ts, status[0] ? status : NULL);
        status[0] = '\0';

        /* --- Event-Driven Input: veri gelene kadar CPU harcamadan bekle --- */
        int r = wait_for_input();
        if (r < 0) continue; /* sinyal (orn. resize) tarafindan kesildi, dongu basina don */

        Key k = read_key();

        switch (k) {
            case KEY_QUIT:
                running = 0;
                break;

            case KEY_UP:
                if (selected > 0) selected--;
                if (selected < top) top = selected; /* yukari kaydir */
                break;

            case KEY_DOWN:
                if (selected + 1 < dv.count) selected++;
                if (selected >= top + (size_t)list_rows) top = selected - list_rows + 1;
                break;

            case KEY_ENTER: {
                if (dv.count == 0) break;
                fs_ensure_stat(&dv, selected);
                if (dv.entries[selected].type == ENTRY_DIR) {
                    char next[8192];
                    snprintf(next, sizeof(next), "%s/%s", dv.cwd, fs_entry_name(&dv, selected));
                    DirView newdv;
                    if (fs_open_dir(&newdv, next) == 0) {
                        fs_free_dir(&dv);
                        dv = newdv;
                        selected = 0;
                        top = 0;
                    } else {
                        snprintf(status, sizeof(status), "Girilemedi: %s", next);
                    }
                }
                break;
            }

            case KEY_BACKSPACE_DIR: {
                char parent[8192];
                snprintf(parent, sizeof(parent), "%s/..", dv.cwd);
                DirView newdv;
                if (fs_open_dir(&newdv, parent) == 0) {
                    fs_free_dir(&dv);
                    dv = newdv;
                    selected = 0;
                    top = 0;
                }
                break;
            }

            case KEY_SEARCH:
                do_search_and_show(&dv, &ts);
                ui_free(&sb);
                ui_init(&sb);
                break;

            default:
                break;
        }
    }

    ui_show_cursor();
    ui_clear_screen();
    ui_free(&sb);
    fs_free_dir(&dv);
    return 0;
}
