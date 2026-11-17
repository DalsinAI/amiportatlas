/* AmiPortAtlas as a commodity: what each of Exchange's (and the systray's) commands does, and the tool types it
 * reads. Plain C with no system headers, so the host tests (tests/test_cx.c) run it; app.c does the doing.
 *
 * Tool types (from the icon, WBStartup's copy too, or the Shell's KEY=VALUE words), Exchange's own words:
 *   CX_PRIORITY  -128 to 127 (0)
 *   CX_POPUP     YES opens the window at the start (YES; the WBStartup icon says NO)
 *   CX_POPKEY    the hot key that shows the window ("ctrl alt g"; NONE for no key)
 * MIT, (c) 2026 The AmiCradle Team. */
#ifndef AMIPORTATLAS_CXCMD_H
#define AMIPORTATLAS_CXCMD_H

#define APCX_KEYLEN 64

typedef struct apcx_opts {
    int pri;
    int popup;
    char popkey[APCX_KEYLEN];       /* "" for none */
} apcx_opts;

/* commodities.library's command numbers (libraries/commodities.h), so this header needs no system one */
enum { APCX_DISABLE = 15, APCX_ENABLE = 17, APCX_APPEAR = 19, APCX_DISAPPEAR = 21, APCX_KILL = 23, APCX_UNIQUE = 25 };

/* what a command asks of the program */
#define APCX_SHOW      1    /* open the window (it may be open already: then to the front) */
#define APCX_HIDE      2    /* close the window; the program stays, in the systray */
#define APCX_QUIT      4
#define APCX_OFF       8    /* the broker off: the hot key does nothing */
#define APCX_ON       16

static inline int apcx_upper(int c) { return c >= 'a' && c <= 'z' ? c - 32 : c; }

/* the word is name (letter case aside), then '=', spaces or its end; *value: what follows */
static inline int apcx_key(const char *s, const char *name, const char **value)
{
    while (*name && apcx_upper((unsigned char)*s) == *name) s++, name++;
    if (*name) return 0;
    while (*s == ' ') s++;
    if (*s == '=') { s++; while (*s == ' ') s++; *value = s; return 1; }
    if (!*s) { *value = s; return 1; }
    return 0;
}

static inline void apcx_defaults(apcx_opts *o)
{
    const char *k = "ctrl alt g";
    int i;
    o->pri = 0;
    o->popup = 1;
    for (i = 0; k[i]; i++) o->popkey[i] = k[i];
    o->popkey[i] = 0;
}

/* YES, Y..., ON or nothing after the name */
static inline int apcx_yes(const char *v)
{
    return !*v || apcx_upper((unsigned char)v[0]) == 'Y' || (apcx_upper((unsigned char)v[0]) == 'O' && apcx_upper((unsigned char)v[1]) == 'N');
}

/* one tool type or word; 1 when it was one of ours. A priority that isn't a number leaves it as it was; one past
 * -128..127 is held to it. CX_POPKEY=NONE (or empty) is no key; a key longer than APCX_KEYLEN - 1 is refused. */
static inline int apcx_word(apcx_opts *o, const char *w)
{
    const char *v;
    if (!w) return 0;
    if (apcx_key(w, "CX_PRIORITY", &v)) {
        long n = 0;
        int neg = *v == '-', any = 0;
        if (*v == '-' || *v == '+') v++;
        for (; *v >= '0' && *v <= '9' && n < 1000; v++) n = n * 10 + (*v - '0'), any = 1;
        if (any) { if (neg) n = -n; o->pri = n < -128 ? -128 : n > 127 ? 127 : (int)n; }
        return 1;
    }
    if (apcx_key(w, "CX_POPUP", &v)) { o->popup = apcx_yes(v); return 1; }
    if (apcx_key(w, "CX_POPKEY", &v)) {
        int i = 0;
        if (apcx_upper((unsigned char)v[0]) == 'N' && apcx_upper((unsigned char)v[1]) == 'O' &&
            apcx_upper((unsigned char)v[2]) == 'N' && apcx_upper((unsigned char)v[3]) == 'E' && !v[4]) { o->popkey[0] = 0; return 1; }
        while (v[i] && i < APCX_KEYLEN) i++;
        if (i >= APCX_KEYLEN) return 1;
        for (i = 0; v[i]; i++) o->popkey[i] = v[i];
        o->popkey[i] = 0;
        return 1;
    }
    return 0;
}

/* A command from Exchange or the systray, with the window shown or not and the broker off (Disable) or not.
 * Show while disabled does nothing, as every GFOpen commodity. A second start of the program (UNIQUE) shows
 * the window of the one already running, which turns back on if it was off. The hot key is APPEAR's. */
static inline int apcx_do(long id, int shown, int disabled)
{
    switch (id) {
    case APCX_APPEAR:    return disabled ? 0 : APCX_SHOW;
    case APCX_DISAPPEAR: return shown ? APCX_HIDE : 0;
    case APCX_DISABLE:   return disabled ? 0 : APCX_OFF | (shown ? APCX_HIDE : 0);
    case APCX_ENABLE:    return disabled ? APCX_ON : 0;
    case APCX_KILL:      return APCX_QUIT;
    case APCX_UNIQUE:    return (disabled ? APCX_ON : 0) | APCX_SHOW;
    }
    return 0;
}

#endif
