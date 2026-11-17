#!/bin/sh
# AmiPortAtlas and Atlas for AmigaOS 3.2.3 and GFOS (68040, soft float), for
# Cradle's stove_build (target .) or by hand: Cradle checks out this
# repository alone and sets STOVE and OUT, so the three repositories it
# builds on are fetched here at their main (or given as OLUI, OGT, TLS):
#   DalsinAI/gfopenlayout   GFOpenLayout UI (libolui.a, built here)
#   DalsinAI/gfopentls      GFOpenTLS's headers
#   DalsinAI/amicradle-guest libraries/opengadtools, the theme reader: private,
#                           so Cradle's own checkout of it is used when there is one
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
OUT=${OUT:-$HERE/build/os3}
STOVE=${STOVE:-$HOME/AmiCradle/stoves/os32-gcc16}
CC="$STOVE/prefix/bin/m68k-amigaos-gcc"
DEPS=${DEPS:-$HERE/build/deps}   # beside the checkout, not among the outputs
mkdir -p "$OUT" "$DEPS"
export GIT_TERMINAL_PROMPT=0    # a private repository fails at once rather than waiting for a password

fetch() {   # repo dir [sparse path]
    if [ ! -d "$DEPS/$2/.git" ]; then
        if [ $# -ge 3 ]; then
            git clone -q --depth 1 --filter=blob:none --sparse "https://github.com/$1" "$DEPS/$2"
            git -C "$DEPS/$2" sparse-checkout set "$3"
        else
            git clone -q --depth 1 "https://github.com/$1" "$DEPS/$2"
        fi
    fi
    echo "$1 at $(git -C "$DEPS/$2" rev-parse --short HEAD)"
}
if [ -z "${OLUI:-}" ]; then fetch DalsinAI/gfopenlayout gfopenlayout; OLUI=$DEPS/gfopenlayout; fi
if [ -z "${OGT:-}" ]; then
    for d in "$HOME"/AmiCradle-dev/cradle-builds/*amicradle-guest* "$HOME"/AmiCradle-dev/cradle-builds/*/amicradle-guest \
             "$HOME"/AmiCradle-dev/amicradle-guest "$HOME"/AmiCradle/amicradle-guest; do
        if [ -f "$d/libraries/opengadtools/lib/ogt_theme.c" ]; then OGT=$d/libraries/opengadtools; break; fi
    done
fi
if [ -z "${OGT:-}" ]; then fetch DalsinAI/amicradle-guest amicradle-guest libraries/opengadtools; OGT=$DEPS/amicradle-guest/libraries/opengadtools; fi
echo "theme reader: $OGT"
if [ -z "${TLS:-}" ]; then fetch DalsinAI/gfopentls gfopentls; TLS=$DEPS/gfopentls; fi

# The games index (src/gamesdb.c) is SQLite: DalsinAI/gfopensqlite built soft float for one task, as GFOpenBase
# uses it. SQLITE names a built one (its include/ and lib/); else it is fetched and built here, and SQLite's own
# source comes from sqlite.org, checked against the SHA-256 in gfopensqlite's SOURCES. SQLITE=no leaves the
# index out (Atlas INDEX then says so).
if [ -z "${SQLITE:-}" ]; then
    fetch DalsinAI/gfopensqlite gfopensqlite
    SQLITE=$DEPS/gfopensqlite/out-soft-st
    if [ ! -f "$SQLITE/lib/libsqlite3.a" ]; then
        zip=$(awk '$1 == "file:" {print $2; exit}' "$DEPS/gfopensqlite/SOURCES")
        url=$(awk '$1 == "from:" {print $2; exit}' "$DEPS/gfopensqlite/SOURCES")
        mkdir -p "$DEPS/gfopensqlite/tarballs"
        [ -f "$DEPS/gfopensqlite/tarballs/$zip" ] || curl -fsSL -o "$DEPS/gfopensqlite/tarballs/$zip" "$url"
        OS32_GCC16=$STOVE FLOAT=soft THREADSAFE=0 sh "$DEPS/gfopensqlite/build.sh"
    fi
fi
SQL=""; SQLSRC=""; SQLLIB=""
if [ "$SQLITE" != no ]; then
    SQL="-DAP_HAVE_SQLITE -I$SQLITE/include"; SQLSRC="$HERE/src/gamesdb.c $HERE/src/gamesindex.c"; SQLLIB="$SQLITE/lib/libsqlite3.a"
    echo "games index: $SQLLIB"
fi

SPECS=$HERE/specs/m68k-soft-m68040.specs
LATE=""
if "$CC" -fno-late-combine-instructions -S -o /dev/null -x c /dev/null 2>/dev/null; then LATE="-fno-late-combine-instructions"; fi

STOVE=$STOVE OGT_ROOT=$OGT SPECS=$SPECS OUT=$OUT/olui sh "$OLUI/build-ui-amiga.sh" >/dev/null
FLAGS="-m68040 -specs=$SPECS -noixemul -O2 -fno-delete-null-pointer-checks $LATE -fno-common -std=gnu99 \
  -Wall -Wextra -Wno-unused-parameter -Wno-pointer-sign -DAP_HAVE_GFOPENTLS -I$OLUI/include -I$TLS/include"
S=$HERE/src
COMMON="$S/json.c $S/feed.c $S/sources.c $S/index.c $S/sha256.c $S/match.c $S/gamesfolders.c $S/http.c $S/install.c $S/platform.c $S/repo.c"
"$CC" $FLAGS $SQL -o "$OUT/AmiPortAtlas" $COMMON "$S/ui.c" "$S/app.c" $SQLSRC "$OUT/olui/libolui.a" $SQLLIB -lamiga
"$CC" $FLAGS $SQL -o "$OUT/Atlas" $COMMON "$S/ftp.c" "$S/atlas.c" $SQLSRC $SQLLIB -lamiga
for f in "$OUT/AmiPortAtlas" "$OUT/Atlas"; do
    echo "$f ($(wc -c < "$f") bytes) $(sha256sum "$f" | cut -c1-16)"
done
