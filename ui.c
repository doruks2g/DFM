#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "../include/ui.h"

#define ANSI_CLEAR       "\x1b[2J"
#define ANSI_HOME        "\x1b[H"
#define ANSI_HIDE_CUR    "\x1b[?25l"
#define ANSI_SHOW_CUR    "\x1b[?25h"
#define ANSI_CLR_LINE    "\x1b[2K"
#define ANSI_RESET       "\x1b[0m"
#define ANSI_INVERT      "\x1b[7m"
#define ANSI_FG_BLUE     "\x1b[34m"
#define ANSI_FG_GREEN    "\x1b[32m"
#define ANSI_FG_YELLOW   "\x1b[33m"

/* İmleci belirli bir satıra taşı (1-indexed, ANSI standardı) */
static void move_to_row(int row) {
    printf("\x1b[%d;1H", row);
}

void ui_init(ScreenBuffer *sb) {
    sb->capacity = 128;
    sb->lines = calloc(sb->capacity, sizeof(char *));
    sb->line_count = 0;
}

void ui_free(ScreenBuffer *sb) {
    for (size_t i = 0; i < sb->line_count; i++) free(sb->lines[i]);
    free(sb->lines);
    sb->lines = NULL;
    sb->line_count = 0;
}

void ui_get_termsize(TermSize *ts) {
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        ts->rows = w.ws_row;
        ts->cols = w.ws_col;
    } else {
        /* ioctl başarısız olursa güvenli varsayılan */
        ts->rows = 24;
        ts->cols = 80;
    }
}

void ui_clear_screen(void) {
    fputs(ANSI_CLEAR ANSI_HOME, stdout);
    fflush(stdout);
}

void ui_hide_cursor(void) { fputs(ANSI_HIDE_CUR, stdout); }
void ui_show_cursor(void) { fputs(ANSI_SHOW_CUR, stdout); }

/* Tek bir dosya satırını verilen buffer'a formatlar (henüz basmaz) */
static void format_entry_line(char *out, size_t out_size, const DirView *dv,
                               size_t idx, int is_selected, int cols) {
    const char *name = fs_entry_name(dv, idx);
    const FileEntry *fe = &dv->entries[idx];

    const char *type_glyph = "?";
    if (fe->type == ENTRY_DIR)  type_glyph = "/";
    else if (fe->type == ENTRY_LINK) type_glyph = "@";
    else if (fe->type == ENTRY_FILE) type_glyph = " ";

    char size_str[32] = "";
    if (fe->stat_loaded && fe->type == ENTRY_FILE && fe->size >= 0) {
        if (fe->size < 1024)
            snprintf(size_str, sizeof(size_str), "%5ldB", (long)fe->size);
        else if (fe->size < 1024 * 1024)
            snprintf(size_str, sizeof(size_str), "%5.1fK", fe->size / 1024.0);
        else
            snprintf(size_str, sizeof(size_str), "%5.1fM", fe->size / (1024.0 * 1024.0));
    }

    /* Uzun isimleri terminal genişliğine göre kırp */
    int name_field = cols - 12;
    if (name_field < 8) name_field = 8;

    char trimmed[4096];
    if ((int)fe->name_len > name_field) {
        snprintf(trimmed, sizeof(trimmed), "%.*s~", name_field - 1, name);
    } else {
        snprintf(trimmed, sizeof(trimmed), "%s", name);
    }

    const char *prefix = is_selected ? ANSI_INVERT : "";
    const char *suffix = is_selected ? ANSI_RESET : "";
    const char *color = "";
    if (!is_selected) {
        if (fe->type == ENTRY_DIR) color = ANSI_FG_BLUE;
        else if (fe->type == ENTRY_LINK) color = ANSI_FG_YELLOW;
    }

    snprintf(out, out_size, "%s%s%-6s %s%s%s%s",
             prefix, color, size_str, trimmed, type_glyph, ANSI_RESET, suffix);
}

void ui_render(ScreenBuffer *sb, const DirView *dv, size_t selected, size_t top,
               const TermSize *ts, const char *status_msg) {
    int rows = ts->rows;
    int cols = ts->cols;
    int list_rows = rows - 2; /* üst başlık + alt status için 2 satır ayır */
    if (list_rows < 1) list_rows = 1;

    /* Gereken toplam satır sayısı: başlık(1) + liste + status(1) */
    size_t total_lines = (size_t)(list_rows + 2);

    if (sb->capacity < total_lines) {
        size_t new_cap = total_lines + 16;
        char **tmp = realloc(sb->lines, new_cap * sizeof(char *));
        if (!tmp) return;
        for (size_t i = sb->capacity; i < new_cap; i++) tmp[i] = NULL;
        sb->lines = tmp;
        sb->capacity = new_cap;
    }

    char newline[4096];

    /* --- Satır 0: başlık (cwd) --- */
    snprintf(newline, sizeof(newline), "%s[%zu ogeler]  %s",
             dv->cwd, dv->count, ANSI_RESET);
    if (!sb->lines[0] || strcmp(sb->lines[0], newline) != 0) {
        move_to_row(1);
        fputs(ANSI_CLR_LINE, stdout);
        fputs(newline, stdout);
        free(sb->lines[0]);
        sb->lines[0] = strdup(newline);
    }

    /* --- Liste satırları (virtual scroll: sadece [top, top+list_rows) render edilir) --- */
    for (int r = 0; r < list_rows; r++) {
        size_t idx = top + (size_t)r;
        size_t buf_line = (size_t)(r + 1);

        if (idx < dv->count) {
            format_entry_line(newline, sizeof(newline), dv, idx, idx == selected, cols);
        } else {
            newline[0] = '\0'; /* liste bittiyse boş satır */
        }

        if (!sb->lines[buf_line] || strcmp(sb->lines[buf_line], newline) != 0) {
            move_to_row((int)buf_line + 1);
            fputs(ANSI_CLR_LINE, stdout);
            fputs(newline, stdout);
            free(sb->lines[buf_line]);
            sb->lines[buf_line] = strdup(newline);
        }
    }

    /* --- Son satır: status/komut çubuğu --- */
    size_t status_line = (size_t)(list_rows + 1);
    snprintf(newline, sizeof(newline), "%s",
             status_msg ? status_msg : "q:cikis  /:ara  ok-tuslari:gezin");
    if (!sb->lines[status_line] || strcmp(sb->lines[status_line], newline) != 0) {
        move_to_row((int)status_line + 1);
        fputs(ANSI_CLR_LINE, stdout);
        fputs(newline, stdout);
        free(sb->lines[status_line]);
        sb->lines[status_line] = strdup(newline);
    }

    fflush(stdout);
}
