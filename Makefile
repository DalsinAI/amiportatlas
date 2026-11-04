# AmiPortAtlas. MIT, (c) 2026 The AmiCradle Team.
#
#   make test          the host tests (feed reader, SHA-256): any cc
#   make os3           AmigaOS 3.2.3 and GFOS (68020 and up), m68k-amigaos-gcc
#   make aros-m68k     AROS m68k, m68k-aros-gcc
#   make aros-i386     AROS x86, i386-aros-gcc
#   make os4           AmigaOS 4.1, ppc-amigaos-gcc
#
# GFOpenTLS (https) comes from DalsinAI/openamigatls: TLS_INC points at its
# include drawer. A build without it fetches http:// only.
# Amiga builds run on origimagic, never in the cloud.

TLS_INC ?= ../openamigatls/include
OUT     ?= build

COMMON  = src/feed.c src/sha256.c
AMIGA   = $(COMMON) src/http.c src/install.c src/platform.c src/app.c
CWARN   = -Wall -Wextra -Wno-unused-parameter

.PHONY: all test os3 aros-m68k aros-i386 os4 clean

all: test

test: $(OUT)/test_host
	$(OUT)/test_host $(FEED)

$(OUT)/test_host: tests/test_host.c $(COMMON) src/feed.h src/sha256.h
	@mkdir -p $(OUT)
	$(HOSTCC) -std=c99 $(CWARN) -O1 -g -o $@ tests/test_host.c $(COMMON)

HOSTCC ?= cc

os3: $(OUT)/os3/AmiPortAtlas
$(OUT)/os3/AmiPortAtlas: $(AMIGA) src/*.h
	@mkdir -p $(dir $@)
	m68k-amigaos-gcc -noixemul -m68020 -O2 $(CWARN) -DAP_HAVE_GFOPENTLS -I$(TLS_INC) -o $@ $(AMIGA) -lamiga

aros-m68k: $(OUT)/aros-m68k/AmiPortAtlas
$(OUT)/aros-m68k/AmiPortAtlas: $(AMIGA) src/*.h
	@mkdir -p $(dir $@)
	m68k-aros-gcc -O2 $(CWARN) -DAP_HAVE_GFOPENTLS -I$(TLS_INC) -o $@ $(AMIGA) -lamiga

aros-i386: $(OUT)/aros-i386/AmiPortAtlas
$(OUT)/aros-i386/AmiPortAtlas: $(AMIGA) src/*.h
	@mkdir -p $(dir $@)
	i386-aros-gcc -O2 $(CWARN) $(AROS_TLS) -o $@ $(AMIGA) -lamiga

os4: $(OUT)/os4/AmiPortAtlas
$(OUT)/os4/AmiPortAtlas: $(AMIGA) src/*.h
	@mkdir -p $(dir $@)
	ppc-amigaos-gcc -O2 $(CWARN) -D__USE_INLINE__ $(OS4_TLS) -o $@ $(AMIGA)

clean:
	rm -rf $(OUT)
