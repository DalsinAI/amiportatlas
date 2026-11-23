/* AmiPortAtlas: the games index made and read on the Amiga.
 * MIT, (c) 2026 The AmiCradle Team. */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "gamesindex.h"
#include "install.h"

int games_index_drive(const char *drive)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR win = me->pr_WindowPtr;
    BPTR l;
    me->pr_WindowPtr = (APTR)-1;
    l = Lock((STRPTR)drive, ACCESS_READ);
    me->pr_WindowPtr = win;
    if (!l) return 0;
    UnLock(l);
    return 1;
}

static void join(char *out, int max, const char *dir, const char *name)
{
    size_t d = strlen(dir);
    snprintf(out, (size_t)max, "%s%s%s", dir, d && dir[d - 1] != ':' && dir[d - 1] != '/' ? "/" : "", name);
}

/* The drawers in dir put into the index as games of folder. How many, -1
 * when the index refused one. */
static int drawers(GamesDb *db, int folder, const char *dir, int shelves, int (*stop)(void), int *stopped)
{
    struct FileInfoBlock *fib;
    char path[400];
    int n = 0;
    BPTR l = Lock((STRPTR)dir, ACCESS_READ);
    if (!l) return 0;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (fib && Examine(l, fib)) {
        while (n >= 0 && ExNext(l, fib)) {
            const char *name = (const char *)fib->fib_FileName;
            if (stop && stop()) { *stopped = 1; break; }
            if (fib->fib_DirEntryType <= 0) continue;                  /* a file */
            join(path, (int)sizeof path, dir, name);
            if (shelves && games_shelf_drawer(name)) {
                int in = drawers(db, folder, path, 0, stop, stopped);
                n = in < 0 ? -1 : n + in;
            } else if (gamesdb_put_found(db, folder, name, path, NULL) < 0) n = -1;
            else n++;
        }
    }
    if (fib) FreeDosObject(DOS_FIB, fib);
    UnLock(l);
    return n;
}

int games_index_own_drawer(const char *name)
{
    const char *own = "amiportatlas";
    for (; *name && *own; name++, own++) if ((*name | 32) != *own) return 0;
    return *name == *own;
}

/* The drawers at the top of drive, up to max of them (read before the
 * index is written to, as its file is in the same drawer). */
#define TOP_MAX  48
#define TOP_NAME 64
static int top_drawers(const char *drive, char names[TOP_MAX][TOP_NAME])
{
    struct FileInfoBlock *fib;
    int n = 0;
    BPTR l = Lock((STRPTR)drive, ACCESS_READ);
    if (!l) return 0;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (fib && Examine(l, fib))
        while (n < TOP_MAX && ExNext(l, fib)) {
            const char *name = (const char *)fib->fib_FileName;
            if (fib->fib_DirEntryType <= 0 || strlen(name) >= TOP_NAME || games_index_own_drawer(name)) continue;
            strcpy(names[n++], name);
        }
    if (fib) FreeDosObject(DOS_FIB, fib);
    UnLock(l);
    return n;
}

static int date_ago(int days, char out[12])
{
    time_t t = time(NULL) - (time_t)days * 86400;
    struct tm *tm = localtime(&t);
    return tm && strftime(out, 12, "%Y-%m-%d", tm) > 0;
}

int games_index_make(const char *drive, const char *file, const Feed *f, const Sources *s,
                     int (*stop)(void), GamesIndexReport *r, char *why, int whylen)
{
    static char top[TOP_MAX][TOP_NAME];
    char dir[300], today[12];
    GamesDb *db;
    int i, ntop;
    memset(r, 0, sizeof *r);
    ntop = top_drawers(drive, top);
    db = gamesdb_open(file, why, whylen);
    if (!db) return 0;
    today[0] = 0;
    if (date_ago(0, today)) gamesdb_set_today(db, today);
    if (!gamesdb_put_sources(db, s) || (r->listed = gamesdb_put_feed(db, f)) < 0 || !gamesdb_clear_found(db)) goto bad;
    if (!gamesdb_begin(db)) goto bad;           /* what is on this machine, written once (each alone: minutes) */
    for (i = 0; i < f->count; i++) {
        const Port *p = &f->ports[i];
        if (p->id && installed_version(p) && !installed_found(p))
            gamesdb_set_installed(db, p->id, installed_version(p), installed_path(p));
    }
    if (today[0]) r->news_today = gamesdb_new_since(db, today);
    for (i = 0; i < ntop && !r->stopped; i++) {
        int folder = gamesdb_folder(db, top[i]), n;
        if (folder < 0) goto bad;
        join(dir, (int)sizeof dir, drive, top[i]);
        n = drawers(db, folder, dir, 1, stop, &r->stopped);
        if (n < 0) goto bad;
        if (folder < GF_COUNT) r->on_drive[folder] += n;
        else { r->other_drawers++; r->on_drive_other += n; }
    }
    if (!gamesdb_end(db, 1)) goto bad;
    for (i = 0; i < GF_COUNT; i++) r->listed_in[i] = gamesdb_count(db, i, 0);
    gamesdb_close(db);
    return 1;
bad:
    snprintf(why, (size_t)whylen, "The games index %s: %s", file, gamesdb_error(db));
    gamesdb_end(db, 0);                         /* none open: nothing happens */
    gamesdb_close(db);
    return 0;
}

long games_index_found(const char *file, GamesDbFoundFn fn, void *user)
{
    char why[160];
    GamesDb *db;
    long n;
    BPTR l = Lock((STRPTR)file, ACCESS_READ);
    if (!l) return 0;
    UnLock(l);
    db = gamesdb_open(file, why, (int)sizeof why);
    if (!db) return -1;
    n = gamesdb_each_found(db, fn, user);
    gamesdb_close(db);
    return n;
}

struct News { char **ids; int n, cap, bad; };

static void news_one(void *user, const char *id)
{
    struct News *w = (struct News *)user;
    if (w->bad) return;
    if (w->n == w->cap) {
        char **more = (char **)realloc(w->ids, (size_t)(w->cap ? w->cap * 2 : 64) * sizeof(char *));
        if (!more) { w->bad = 1; return; }
        w->ids = more;
        w->cap = w->cap ? w->cap * 2 : 64;
    }
    if (!(w->ids[w->n] = strdup(id))) { w->bad = 1; return; }
    w->n++;
}

int games_index_news(const char *file, int days, char ***ids)
{
    struct News w;
    char since[12], why[160];
    GamesDb *db;
    BPTR l;
    *ids = NULL;
    memset(&w, 0, sizeof w);
    if (!date_ago(days, since)) return 0;
    l = Lock((STRPTR)file, ACCESS_READ);            /* no index yet: none is made just to look */
    if (!l) return 0;
    UnLock(l);
    db = gamesdb_open(file, why, (int)sizeof why);
    if (!db) return 0;
    gamesdb_each_new(db, since, news_one, &w);      /* sorted by id */
    gamesdb_close(db);
    if (w.bad) { games_index_news_free(w.ids, w.n); return -1; }
    *ids = w.ids;
    return w.n;
}

void games_index_news_free(char **ids, int n)
{
    int i;
    for (i = 0; i < n; i++) free(ids[i]);
    free(ids);
}

int games_index_is_news(char *const *ids, int n, const char *id)
{
    int lo = 0, hi = n - 1;
    while (id && lo <= hi) {
        int mid = (lo + hi) / 2, c = strcmp(id, ids[mid]);
        if (c == 0) return 1;
        if (c < 0) hi = mid - 1; else lo = mid + 1;
    }
    return 0;
}
