# dfm (Doruk File Manager)

Terminalde çalışan, hafif bir dosya yöneticisi (TUI). Ekstra kütüphane
bağımlılığı yoktur — `ncurses` kullanılmaz; yalnızca standart POSIX/glibc
başlıkları (`termios`, `sys/ioctl`, `select`, `dirent`) ile yazılmıştır.
Kendi geliştirdiğim `dfind` arama aracı, dosya yöneticisinin içine
gömülü (in-process) arama motoru olarak entegre edilmiştir.

Bağımsız bir projedir; `bursched` ile bir ilgisi yoktur.

Bu proje profesyonel bir yapım değildir, hobi amaçlı geliştirilmektedir.
Kod elle yazılmamış olup Claude (Anthropic) yardımıyla oluşturulmuştur.

## Özellikler

- **ncurses'siz TUI**: ham ANSI escape kodları + `termios` raw mode ile
  tam ekran, tuş bazlı gezinme.
- **Event-driven input**: `select()` ile stdin beklenir; veri gelene
  kadar CPU tüketimi ölçülebilir şekilde ~%0'da kalır (busy-loop yok).
- **Double buffering**: her karede yalnızca değişen satırlar terminale
  yeniden yazılır (`\x1b[2J` tam ekran temizleme yalnızca açılışta/
  kapanışta bir kez yapılır) — bu, titremeyi (flicker) önler.
- **Lazy stat loading**: `readdir()` ile isim listesi anında toplanır;
  `stat()` çağrısı yalnızca o an ekranda görünen satırlar için,
  render'dan hemen önce yapılır. Binlerce dosyalı dizinlerde gecikmeyi
  önler.
- **Zero-copy dosya adı deposu (arena)**: her `FileEntry` kendi adını
  `malloc`+`strcpy` ile kopyalamaz; tüm isimler tek bir büyük arena
  buffer'ında tutulur, `FileEntry` yalnızca bir offset taşır.
- **Gömülü arama (dfind entegrasyonu)**: `/` tuşuna basıp bir desen
  yazdığında, `dfind_core.c` (aşağıya bakın) mevcut dizinden itibaren
  recursive arama yapar ve sonuçları anlık gösterir.

## Mimari / Dosyalar

| Dosya            | Sorumluluk                                                        |
|------------------|--------------------------------------------------------------------|
| `main.c`         | Olay döngüsü: raw mode, `select()` ile input bekleme, tuş yönetimi |
| `fs.c` / `fs.h`  | Dizin okuma, lazy stat, zero-copy arena tabanlı isim deposu        |
| `ui.c` / `ui.h`  | ANSI tabanlı çizim, double-buffer diff render, terminal boyutu     |
| `search.c/.h`    | `dfind_core.c` için köprü: in-process ve fork/exec olmak üzere iki arama yöntemi |
| `dfind_core.c`   | `dfind.c`'nin callback tabanlı, gömülebilir versiyonu               |
| `dfind.c`        | Bağımsız çalışan orijinal CLI arama aracı (`dfind` binary'si)       |
| `common.h`       | Paylaşılan tipler: `FileEntry`, `EntryType`, `TermSize`             |

### `dfind.c` ile `dfind_core.c` arasındaki ilişki — dikkat

`dfind_core.c`, `dfind.c` içindeki `search()` fonksiyonunun **birebir
aynı algoritmasının** (recursive `readdir` + `strstr` eşleşme) callback
alacak şekilde uyarlanmış halidir. Bu bilinçli bir tercih: `dfind.c`
hiç değiştirilmeden bağımsız bir CLI aracı olarak kalmaya devam eder,
`dfind_core.c` ise dosya yöneticisine gömülü hızlı arama için kullanılır.

**Önemli**: Bu iki dosya şu an elle senkronize tutuluyor. `dfind.c`
içindeki arama mantığında bir değişiklik/düzeltme yaparsan, aynı
değişikliği `dfind_core.c` içine de manuel olarak taşıman gerekir —
otomatik bir senkronizasyon mekanizması yoktur. Zamanla ikisi
birbirinden sessizce sapabilir (örn. biri güncellenip diğeri
unutulabilir), bu bilinen bir teknik borç.

## Build

```bash
make            # normal derleme → ./dfm
make static     # statik linkli binary (harici .so bağımlılığı yok)
make dfind      # orijinal bağımsız dfind CLI aracını ayrıca derler
make clean      # derleme çıktılarını temizler
```

`-D_DEFAULT_SOURCE` bayrağı zorunludur (`Makefile` içinde zaten var):
`DT_DIR`/`DT_LNK` (dirent.h) ve `lstat` gibi POSIX/BSD uzantıları
`-std=c11` gibi katı modlarda bu makro olmadan görünmez.

## Çalıştırma

```bash
./dfm [baslangic_dizini]     # argüman verilmezse mevcut dizinden başlar
```

### Tuşlar

| Tuş                | İşlev                          |
|---------------------|--------------------------------|
| `↑` / `↓`           | Yukarı / aşağı gezin           |
| `Enter`             | Seçili dizine gir              |
| `←` veya `h`        | Üst dizine çık                 |
| `/`                 | Arama modu (deseni yaz, Enter) |
| `q`                 | Çıkış                          |

## Bilinmesi gereken sınırlamalar ve riskler

`bursched`'in aksine bu proje root yetkisi veya systemd servisi
gerektirmez — sıradan bir kullanıcı olarak çalıştırılan bir terminal
uygulamasıdır. Yine de dikkat edilmesi gereken noktalar var:

- **Path buffer'ları sabit boyutludur.** `fs.c`, `main.c` ve
  `dfind_core.c` içinde `char full[8192]`, `char next[8192]` gibi sabit
  boyutlu tamponlar kullanılıyor. `snprintf` ile taşma engellense de,
  çok derin iç içe dizin yapılarında (8192 karakteri aşan tam yollar)
  yol sessizce kırpılabilir. Pratikte nadir bir durumdur ama sıra dışı
  dosya sistemlerinde göz önünde bulundurulmalı.
- **`dfind.c` / `dfind_core.c` senkronizasyon borcu** (yukarıda
  detaylandırıldı) — ikisini ayrı ayrı elle güncellemeyi unutma.
- **Sembolik link döngüsü koruması yok.** `fs_open_dir` ve
  `dfind_core_search`, sembolik linkleri normal girişler gibi listeler/
  gezer; kendine referans veren bir symlink zinciri varsa (örn. bir
  dizin kendi altına symlink veriyorsa) arama fonksiyonu bu döngüye
  girip gereksiz derinlemesine inebilir. `bursched` projesinde bu
  `FTW_PHYS` ile engellenmişti, burada henüz yok.
- **Arama sonuç gösterimi 40 satırla sınırlı** (`do_search_and_show`
  içinde `show_max = 40`) — bu bir hata değil, bilinçli bir tasarım
  sınırı; daha fazla sonucu görmek istersen bu sınırı `main.c` içinde
  artırman gerekir.
- **`search_run_external` (fork/exec yöntemi) şu an kullanılmıyor**,
  yalnızca `search_run_inprocess` aktif. `dfind` binary'sini ayrı bir
  process olarak izole etmek istersen bu fonksiyon zaten hazır durumda.
- **Statik build notu**: `make static` ile üretilen tam statik glibc
  binary'si, `getpwnam` gibi NSS çağrıları kullanan projelerde sorun
  çıkarabilir; bu proje yalnızca dosya sistemi çağrıları kullandığından
  şu an için sorun yaratmıyor, ama gelecekte kullanıcı adı/izin
  çözümleme gibi bir özellik eklenirse bu notu hatırlamakta fayda var.

## Yapılacaklar / geliştirme fikirleri

- `dfind.c` ↔ `dfind_core.c` senkronizasyonunu otomatikleştirmek (örn.
  ortak bir header'a algoritmanın çekirdeğini taşımak).
- Symlink döngü koruması eklemek.
- Dosya işlemleri (kopyala/taşı/sil) henüz yok — şu an salt gezinme ve
  arama var.
- Renk/tema özelleştirmesi (şu an ANSI renkleri sabit kodlanmış).

## Lisans

Kişisel proje — lisans belirtilmemiş.
