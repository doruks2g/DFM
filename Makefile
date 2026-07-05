CC      = gcc
# _DEFAULT_SOURCE: glibc'nin DT_DIR/DT_LNK (dirent.h) ve lstat (sys/stat.h)
# gibi POSIX/BSD uzantilarini -std=c11 gibi katı standart modlarinda da
# gorunur kilar (aksi halde sadece salt ISO C gorunur).
CFLAGS  = -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -O2 -Iinclude
LDFLAGS_STATIC = -static

SRC = src/main.c src/fs.c src/ui.c src/search.c src/dfind_core.c
OBJ = $(SRC:.c=.o)
BIN = dfm

.PHONY: all static clean dfind

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

# Statik linkli build: hicbir sistemde libc bagimliligina takilmadan calisir.
# Not: glibc ile tam statik binary bazi dagitimlarda uyari verebilir
# (NSS/getpwnam gibi cagrilar icin); bu projede sadece dosya sistemi
# cagrilari kullandigimizdan sorun cikarmaz.
static: CFLAGS += -static
static: $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS_STATIC) -o $(BIN) $(OBJ)

# Orijinal dfind.c'yi ayri, degistirilmemis bir CLI binary olarak derle
# (search_run_external() ile fork/exec kullanmak istersen).
dfind: dfind.c
	$(CC) -std=c11 -D_DEFAULT_SOURCE -Wall -O2 -o dfind dfind.c

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN) dfind
