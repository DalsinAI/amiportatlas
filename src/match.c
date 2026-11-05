/* AmiPortAtlas: telling a game's drawer and its program by their names.
 * MIT, (c) 2026 The AmiCradle Team.
 */
#include <stdlib.h>
#include <string.h>
#include "match.h"

#define BUCKETS 2048

typedef struct Name {
    char key[MATCH_NAME];
    Port *port;
    int strength;
    struct Name *next;
} Name;

struct Matcher {
    Name *bucket[BUCKETS];
};

static int lower(int c) { return c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c; }
static int alnum(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); }

void match_key(const char *name, char *out, int max)
{
    int n = 0;
    for (; name && *name && n < max - 1; name++)
        if (alnum((unsigned char)*name)) out[n++] = (char)lower((unsigned char)*name);
    out[n] = 0;
}

static unsigned hash(const char *s)
{
    unsigned h = 5381;
    while (*s) h = h * 33u + (unsigned char)*s++;
    return h % BUCKETS;
}

/* One name for p: kept when no port has it yet, or a weaker one has */
static void add(Matcher *m, Port *p, const char *key, int strength)
{
    Name *n;
    unsigned h;
    if (strlen(key) < 3) return;
    h = hash(key);
    for (n = m->bucket[h]; n; n = n->next)
        if (strcmp(n->key, key) == 0) {
            if (strength > n->strength) { n->port = p; n->strength = strength; }
            else if (strength == n->strength && n->port != p) n->port = NULL;   /* two games: neither */
            return;
        }
    n = (Name *)malloc(sizeof *n);
    if (!n) return;
    strncpy(n->key, key, MATCH_NAME - 1);
    n->key[MATCH_NAME - 1] = 0;
    n->port = p;
    n->strength = strength;
    n->next = m->bucket[h];
    m->bucket[h] = n;
}

/* text split at spaces, '-', '_', ':', '/' and '(': each run of first words
 * ("Fish", "Fish Fillets", "Fish Fillets NG") a name, the whole one at whole;
 * the initials of three words or more ("Heart of Darkness": "hod") too. */
static void add_words(Matcher *m, Port *p, const char *text, int whole, int skip_g)
{
    char key[MATCH_NAME], initials[MATCH_NAME];
    int n = 0, ni = 0, words = 0, i;
    const char *s = text;
    if (skip_g && strncmp(s, "g-", 2) == 0) s += 2;
    for (i = 0; s[i]; i++) {
        int c = (unsigned char)s[i];
        if (alnum(c)) {
            if (n == 0 || !alnum((unsigned char)s[i - 1])) {
                if (ni < MATCH_NAME - 1) initials[ni++] = (char)lower(c);
                words++;
            }
            if (n < MATCH_NAME - 1) key[n++] = (char)lower(c);
            continue;
        }
        if (c == '(' || c == '/' || c == ':') break;        /* "Neverball / Neverputt": "Neverball" */
        if (n > 0 && alnum((unsigned char)s[i - 1])) {
            key[n] = 0;
            add(m, p, key, words == 1 ? 1 : 2);             /* a first word only is a weak guess */
        }
    }
    key[n] = 0;
    add(m, p, key, s[i] ? 3 : whole);
    if (words >= 3) { initials[ni] = 0; add(m, p, initials, 2); }
}

Matcher *matcher_new(Port *const *ports, int n)
{
    Matcher *m = (Matcher *)calloc(1, sizeof *m);
    int i;
    char key[MATCH_NAME];
    if (!m) return NULL;
    for (i = 0; i < n; i++) {
        Port *p = ports[i];
        if (p->title) add_words(m, p, p->title, 4, 0);
        if (p->id) add_words(m, p, p->id, 4, 1);
        if (p->drawer) { match_key(p->drawer, key, sizeof key); add(m, p, key, 5); }
    }
    return m;
}

void matcher_free(Matcher *m)
{
    int i;
    if (!m) return;
    for (i = 0; i < BUCKETS; i++) {
        Name *n = m->bucket[i];
        while (n) { Name *x = n->next; free(n); n = x; }
    }
    free(m);
}

Port *matcher_find(const Matcher *m, const char *name, int *strength)
{
    char key[MATCH_NAME];
    Name *n;
    match_key(name, key, sizeof key);
    if (!m || !key[0]) return NULL;
    for (n = m->bucket[hash(key)]; n; n = n->next)
        if (strcmp(n->key, key) == 0) {
            if (strength) *strength = n->strength;
            return n->port;     /* NULL when the name fits two games equally */
        }
    return NULL;
}

static int same(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) if (lower((unsigned char)*a) != lower((unsigned char)*b)) return 0;
    return *a == *b;
}

static int ends(const char *s, const char *tail)
{
    size_t n = strlen(s), t = strlen(tail);
    return n > t && same(s + n - t, tail);
}

int match_program(const Port *p, const char *drawer, const char *file)
{
    static const char *const junk[] = { ".info", ".log", ".txt", ".old", ".new", ".bak", ".dbg", ".debug",
                                        ".readme", ".guide", ".cfg", ".ini", ".lha", ".map", "_old", "_new",
                                        "-old", "-new", ".orig" };
    char fkey[MATCH_NAME], key[MATCH_NAME];
    unsigned i;
    int score = 1;
    for (i = 0; i < sizeof junk / sizeof junk[0]; i++) if (ends(file, junk[i])) return 0;
    if (p && p->program && same(file, p->program)) return 10;
    match_key(file, fkey, sizeof fkey);
    match_key(drawer, key, sizeof key);
    if (key[0] && strcmp(fkey, key) == 0) return 8;                 /* Brogue/brogue */
    if (p && p->id) {
        match_key(strncmp(p->id, "g-", 2) == 0 ? p->id + 2 : p->id, key, sizeof key);
        if (strcmp(fkey, key) == 0) return 7;
    }
    if (p && p->title) {
        match_key(p->title, key, sizeof key);
        if (strcmp(fkey, key) == 0) return 7;
    }
    if (!strchr(file, '.')) score++;                                /* quake, not quake.gpu */
    return score;
}

int match_version(const char *ver, char *out, int max)
{
    const char *s = ver, *t;
    int n;
    if (!ver || max < 2) return 0;
    if (strncmp(s, "$VER:", 5) == 0) s += 5;
    while (*s == ' ') s++;
    while (*s && *s != ' ') s++;                    /* the program's name */
    for (;;) {
        while (*s == ' ') s++;
        if (!*s || *s == '(' || *s == '\r' || *s == '\n') return 0;
        for (t = s; *t && *t != ' ' && *t != '(' && *t != '\r' && *t != '\n'; t++) {}
        if ((*s == 'v' || *s == 'V') && s[1] >= '0' && s[1] <= '9') s++;   /* "v1.2" */
        if (*s >= '0' && *s <= '9') {
            n = (int)(t - s) < max - 1 ? (int)(t - s) : max - 1;
            memcpy(out, s, (size_t)n);
            out[n] = 0;
            return 1;
        }
        s = t;
    }
}

/* The first number run of a version, digits and dots: "1.6.0 RC2" gives
 * "1.6.0", "test 4 (git d4b94bb)" gives "4"; how long it is */
static int number_run(const char **v)
{
    const char *s = *v;
    int n = 0;
    while (*s && !(*s >= '0' && *s <= '9')) s++;
    *v = s;
    while ((s[n] >= '0' && s[n] <= '9') || (s[n] == '.' && s[n + 1] >= '0' && s[n + 1] <= '9')) n++;
    return n;
}

int version_cmp(const char *a, const char *b)
{
    const char *ea, *eb;
    if (!a || !b) return 0;
    {
        int na = number_run(&a), nb = number_run(&b);
        ea = a + na;
        eb = b + nb;
    }
    if (a == ea || b == eb) return 0;
    while (a < ea && b < eb) {
        unsigned long x = 0, y = 0;
        while (a < ea && *a != '.') x = x * 10 + (unsigned long)(*a++ - '0');
        while (b < eb && *b != '.') y = y * 10 + (unsigned long)(*b++ - '0');
        if (x != y) return x < y ? -1 : 1;
        if (a < ea) a++;
        if (b < eb) b++;
    }
    return a < ea ? 1 : b < eb ? -1 : 0;           /* 1.2 before 1.2.1 */
}
