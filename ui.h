#ifndef DFM_UI_H
#define DFM_UI_H

#include "common.h"
#include "fs.h"

/* ---------------------------------------------------------------------
 * Double Buffering Notu:
 *   - ScreenBuffer, bir önceki çizilen frame'in metnini satır satır
 *     tutar (back buffer mantığının tersi ama aynı fikir: "önceki
 *     durumu hatırla, sadece farkı yaz").
 *   - ui_render() her çağrıldığında YENİ içerik üretir ama satır satır
 *     karşılaştırıp SADECE DEĞİŞEN satırları terminale basar.
 *   - Bu sayede tam ekran temizleme (\x1b[2J) HİÇBİR ZAMAN her frame'de
 *     yapılmaz; sadece ilk açılışta.
 * --------------------------------------------------------------------- */

typedef struct {
    char **lines;         /* Önceki frame'de her satıra yazılan metin */
    size_t line_count;
    size_t capacity;
} ScreenBuffer;

void ui_init(ScreenBuffer *sb);
void ui_free(ScreenBuffer *sb);

/* Terminal boyutunu al (ioctl TIOCGWINSZ) */
void ui_get_termsize(TermSize *ts);

/* Ekranı tamamen temizle - sadece program başlarken/biterken çağrılır */
void ui_clear_screen(void);

/* İmleci gizle/göster (flicker azaltma) */
void ui_hide_cursor(void);
void ui_show_cursor(void);

/* Dosya listesini render et - selected: seçili satır index'i (dv içindeki)
 * top: kaydırma ofseti (kaç satır yukarıdan başlanacak - virtual scroll) */
void ui_render(ScreenBuffer *sb, const DirView *dv, size_t selected, size_t top,
               const TermSize *ts, const char *status_msg);

#endif /* DFM_UI_H */
