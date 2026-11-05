# AmiPortAtlas. MIT, (c) 2026 The AmiCradle Team.
#
#   make test        host tests: the feed reader, SHA-256, and the real window
#                    laid out by GFOpenLayout UI (needs OLUI and OGT, below)
#   make render      the window drawn as build/window.png (Python 3 + Pillow)
#   make os3         AmigaOS 3.2.3 and GFOS: 68040, soft float, as every GFOpen program:
#                    AmiPortAtlas (the window) and Atlas (the Shell command)
#   ./build-amiga.sh the same with a stove, fetching what it builds on (Cradle's stove_build)
#
# OLUI  a DalsinAI/gfopenlayout checkout (for os3, with build-amiga/ui/libolui.a
#       from its build-ui-amiga.sh)
# OGT   amicradle-guest's libraries/opengadtools (the theme reader, host only)
# TLS_INC  DalsinAI/gfopentls's include drawer (GFOpenTLS)
# NDK_INC  the NDK 3.2 / OpenNDK headers, when the compiler doesn't have them
# SPECS    the GFOpen SDK's Lib/m68k-soft-m68040.specs
# Amiga builds run on origimagic, never in the cloud.

OLUI    ?= ../gfopenlayout
OGT     ?= ../amicradle-guest/libraries/opengadtools
TLS_INC ?= ../gfopentls/include
NDK_INC ?=
SPECS   ?=
FEED    ?= tests/ports.json
THEME   ?= $(OGT)/themes/Open.theme
OUT     ?= build
HOSTCC  ?= cc
CC68K   ?= m68k-amigaos-gcc

COMMON  = src/json.c src/feed.c src/sources.c src/index.c src/sha256.c src/match.c
NETWORK = src/http.c src/install.c src/platform.c src/repo.c
AMIGA   = $(COMMON) $(NETWORK) src/ui.c src/app.c
ATLAS   = $(COMMON) $(NETWORK) src/ftp.c src/atlas.c
CWARN   = -Wall -Wextra -Wno-unused-parameter -Wno-pointer-sign
OLUI_HOST = $(OLUI)/src/openlayout.c $(OLUI)/src/gfcss_parse.c $(OLUI)/src/gfcss_values.c \
            $(OLUI)/src/gfcss_select.c $(OLUI)/src/openlayout_css.c $(OLUI)/ui/olui.c $(OLUI)/ui/olui_css.c \
            $(OLUI)/ui/olui_markup.c $(OLUI)/ui/olui_theme.c $(OGT)/lib/ogt_theme.c

.PHONY: all test render os3 clean
all: test

test: $(OUT)/test_host $(OUT)/ui_host
	$(OUT)/test_host $(FEED)
	$(OUT)/ui_host $(FEED) $(THEME) $(OUT)/window-ops.txt

render: test
	python3 tools/render_ui.py $(OUT)/window-ops.txt $(OUT)/window.png $(OLUI)/tests/ui_render.py $(PICTURE)

$(OUT)/test_host: tests/test_host.c $(COMMON) src/*.h
	@mkdir -p $(OUT)
	$(HOSTCC) -std=c99 $(CWARN) -O1 -g -o $@ tests/test_host.c $(COMMON)

$(OUT)/ui_host: tests/ui_host.c src/ui.c src/ui.h $(COMMON)
	@mkdir -p $(OUT)
	$(HOSTCC) -std=c99 -O1 -g -w -I$(OLUI)/include -I$(OLUI)/ui -I$(OLUI)/src -I$(OGT)/lib -o $@ \
	  tests/ui_host.c src/ui.c $(COMMON) $(OLUI_HOST)

os3: $(OUT)/os3/AmiPortAtlas $(OUT)/os3/Atlas
$(OUT)/os3/AmiPortAtlas: $(AMIGA) src/*.h
	@mkdir -p $(dir $@)
	$(CC68K) -m68040 $(if $(SPECS),-specs=$(SPECS)) -noixemul -O2 -fno-delete-null-pointer-checks $(CWARN) \
	  -DAP_HAVE_GFOPENTLS -I$(OLUI)/include -I$(TLS_INC) $(if $(NDK_INC),-I$(NDK_INC)) \
	  -o $@ $(AMIGA) $(OLUI)/build-amiga/ui/libolui.a -lamiga
$(OUT)/os3/Atlas: $(ATLAS) src/*.h
	@mkdir -p $(dir $@)
	$(CC68K) -m68040 $(if $(SPECS),-specs=$(SPECS)) -noixemul -O2 -fno-delete-null-pointer-checks $(CWARN) \
	  -DAP_HAVE_GFOPENTLS -I$(TLS_INC) $(if $(NDK_INC),-I$(NDK_INC)) -o $@ $(ATLAS) -lamiga

clean:
	rm -rf $(OUT)
