/* AmiPortAtlas: the games index. MIT, (c) 2026 The AmiCradle Team.
 * Plain C with SQLite and no Amiga calls. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <sqlite3.h>
#include "gamesdb.h"
#include "index.h"

#define GF_COUNT_TEXT "5"          /* GF_COUNT, for the SQL */
typedef char gf_count_is_5[GF_COUNT == 5 ? 1 : -1];

struct GamesDb {
    sqlite3 *sql;
    char     err[160];
    char     today[12];
};

/* The tables. A change here is a new GAMESDB_SCHEMA. */
static const char schema_sql[] =
    "CREATE TABLE meta (key TEXT PRIMARY KEY, value TEXT) WITHOUT ROWID;"
    "CREATE TABLE folders (folder INTEGER PRIMARY KEY, name TEXT NOT NULL);"
    "CREATE TABLE sources (id TEXT PRIMARY KEY, name TEXT, home TEXT, idx TEXT, note TEXT,"
    " is_on INTEGER, checked INTEGER, folder INTEGER) WITHOUT ROWID;"
    "CREATE TABLE games (id TEXT PRIMARY KEY, source TEXT, folder INTEGER, number INTEGER,"
    " origin TEXT, kind TEXT NOT NULL, title TEXT NOT NULL COLLATE NOCASE, year TEXT, state TEXT, fit TEXT,"
    " game_data TEXT, version TEXT, summary TEXT, author TEXT, category TEXT, cpu TEXT, fpu INTEGER,"
    " space INTEGER, download_url TEXT, download_size INTEGER, download_sha256 TEXT, release_url TEXT,"
    " port_repo TEXT, drawer TEXT COLLATE NOCASE, program TEXT, install TEXT, shot TEXT,"
    " listed INTEGER NOT NULL DEFAULT 0, installed_version TEXT, found INTEGER NOT NULL DEFAULT 0,"
    " path TEXT, found_program TEXT, first_seen TEXT, updated TEXT);"
    "CREATE INDEX games_by_news ON games (first_seen, updated);"
    "CREATE INDEX games_by_title ON games (title);"
    "CREATE INDEX games_by_folder ON games (folder, title);"
    "CREATE INDEX games_by_drawer ON games (drawer, folder);"   /* put_found asks by drawer, with or without folder */
    "CREATE TABLE shots (game TEXT NOT NULL, n INTEGER NOT NULL, url TEXT NOT NULL, PRIMARY KEY (game, n)) WITHOUT ROWID;"
    "CREATE TABLE needs (game TEXT NOT NULL, what TEXT NOT NULL, name TEXT NOT NULL,"
    " PRIMARY KEY (game, what, name)) WITHOUT ROWID;";

static const char drop_sql[] =
    "DROP TABLE IF EXISTS meta; DROP TABLE IF EXISTS folders; DROP TABLE IF EXISTS sources;"
    "DROP TABLE IF EXISTS games; DROP TABLE IF EXISTS shots; DROP TABLE IF EXISTS needs;";

static int run(GamesDb *db, const char *sql)
{
    if (sqlite3_exec(db->sql, sql, NULL, NULL, NULL) == SQLITE_OK) return 1;
    snprintf(db->err, sizeof db->err, "%s", sqlite3_errmsg(db->sql));
    return 0;
}

static int fail(GamesDb *db)
{
    snprintf(db->err, sizeof db->err, "%s", sqlite3_errmsg(db->sql));
    return 0;
}

/* Text bound as UTF-8, whichever way the feed was read; NULL stays NULL. */
static void bind_text(sqlite3_stmt *st, int at, const char *s)
{
    char small[512], *big;
    size_t need;
    if (!s) { sqlite3_bind_null(st, at); return; }
    need = strlen(s) * 2 + 1;
    if (need <= sizeof small) {
        latin1_to_utf8(s, small, (int)sizeof small);
        sqlite3_bind_text(st, at, small, -1, SQLITE_TRANSIENT);
    } else if ((big = malloc(need)) != NULL) {
        latin1_to_utf8(s, big, (int)need);
        sqlite3_bind_text(st, at, big, -1, free);
    } else sqlite3_bind_null(st, at);
}

static void bind_folder(sqlite3_stmt *st, int at, int folder)
{
    if (folder == GF_NONE) sqlite3_bind_null(st, at); else sqlite3_bind_int(st, at, folder);
}

static long user_version(GamesDb *db)
{
    sqlite3_stmt *st;
    long v = -1;
    if (sqlite3_prepare_v2(db->sql, "PRAGMA user_version", -1, &st, NULL) != SQLITE_OK) return -1;
    if (sqlite3_step(st) == SQLITE_ROW) v = (long)sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return v;
}

static int make_tables(GamesDb *db)
{
    char sql[96];
    int i;
    if (!run(db, "BEGIN") || !run(db, drop_sql) || !run(db, schema_sql)) goto bad;
    for (i = 0; i < GF_COUNT; i++) {
        char *q = sqlite3_mprintf("INSERT INTO folders VALUES (%d, %Q)", i, games_folder_name(i));
        int ok = q && run(db, q);
        sqlite3_free(q);
        if (!ok) goto bad;
    }
    snprintf(sql, sizeof sql, "PRAGMA user_version = %d", GAMESDB_SCHEMA);
    if (!run(db, sql) || !run(db, "COMMIT")) goto bad;
    return 1;
bad:
    sqlite3_exec(db->sql, "ROLLBACK", NULL, NULL, NULL);
    return 0;
}

GamesDb *gamesdb_open(const char *path, char *why, int whylen)
{
    GamesDb *db = calloc(1, sizeof *db);
    long v;
    if (why && whylen > 0) why[0] = 0;
    if (!db) { if (why) snprintf(why, (size_t)whylen, "Out of memory."); return NULL; }
    if (sqlite3_open(path, &db->sql) != SQLITE_OK) {
        if (why) snprintf(why, (size_t)whylen, "The games index %s can't be opened: %s.", path,
                          db->sql ? sqlite3_errmsg(db->sql) : "no memory");
        goto bad;
    }
    v = user_version(db);
    if (v < 0) {
        if (why) snprintf(why, (size_t)whylen, "%s is not a games index: %s.", path, sqlite3_errmsg(db->sql));
        goto bad;
    }
    if (v > GAMESDB_SCHEMA) {
        if (why) snprintf(why, (size_t)whylen, "The games index %s was made by a newer AmiPortAtlas (schema %ld).", path, v);
        goto bad;
    }
    if (v < GAMESDB_SCHEMA && !make_tables(db)) {
        if (why) snprintf(why, (size_t)whylen, "The games index %s can't be made: %s.", path, db->err);
        goto bad;
    }
    return db;
bad:
    if (db->sql) sqlite3_close(db->sql);
    free(db);
    return NULL;
}

void gamesdb_close(GamesDb *db)
{
    if (!db) return;
    sqlite3_close(db->sql);
    free(db);
}

const char *gamesdb_error(GamesDb *db)
{
    return db->err;
}

int gamesdb_put_sources(GamesDb *db, const Sources *s)
{
    sqlite3_stmt *st = NULL;
    int i, ok = 0;
    if (!run(db, "BEGIN")) return 0;
    if (!run(db, "DELETE FROM sources")) goto done;
    if (sqlite3_prepare_v2(db->sql, "INSERT OR REPLACE INTO sources VALUES (?1,?2,?3,?4,?5,?6,?7,?8)", -1, &st, NULL)
        != SQLITE_OK) { fail(db); goto done; }
    for (i = 0; i < s->count; i++) {
        const Source *e = &s->list[i];
        bind_text(st, 1, e->id);
        bind_text(st, 2, e->name);
        bind_text(st, 3, e->home);
        bind_text(st, 4, e->index);
        bind_text(st, 5, e->note);
        sqlite3_bind_int(st, 6, e->on);
        sqlite3_bind_int(st, 7, e->checked);
        bind_folder(st, 8, games_folder_of_source(e->id));
        if (sqlite3_step(st) != SQLITE_DONE) { fail(db); goto done; }
        sqlite3_reset(st);
    }
    ok = 1;
done:
    if (st) sqlite3_finalize(st);
    if (ok) ok = run(db, "COMMIT");
    if (!ok) sqlite3_exec(db->sql, "ROLLBACK", NULL, NULL, NULL);
    return ok;
}

static const char put_sql[] =
    "INSERT INTO games (id, source, folder, number, origin, kind, title, year, state, fit, game_data, version,"
    " summary, author, category, cpu, fpu, space, download_url, download_size, download_sha256, release_url,"
    " port_repo, drawer, program, install, shot, listed, first_seen)"
    " VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?22,?23,?24,?25,?26,?27,1,?28)"
    " ON CONFLICT (id) DO UPDATE SET"
    " updated = CASE WHEN ?28 IS NOT NULL AND excluded.version IS NOT NULL AND games.version IS NOT excluded.version"
    "  THEN ?28 ELSE games.updated END,"
    " source=excluded.source, folder=excluded.folder, number=excluded.number,"
    " origin=excluded.origin, kind=excluded.kind, title=excluded.title, year=excluded.year, state=excluded.state,"
    " fit=excluded.fit, game_data=excluded.game_data, version=excluded.version, summary=excluded.summary,"
    " author=excluded.author, category=excluded.category, cpu=excluded.cpu, fpu=excluded.fpu, space=excluded.space,"
    " download_url=excluded.download_url, download_size=excluded.download_size,"
    " download_sha256=excluded.download_sha256, release_url=excluded.release_url, port_repo=excluded.port_repo,"
    " drawer=excluded.drawer, program=excluded.program, install=excluded.install, shot=excluded.shot, listed=1";

static int put_list(GamesDb *db, sqlite3_stmt *st, const char *id, const char *what, char **names, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        if (!names[i] || !*names[i]) continue;
        bind_text(st, 1, id);
        sqlite3_bind_text(st, 2, what, -1, SQLITE_STATIC);
        bind_text(st, 3, names[i]);
        if (sqlite3_step(st) != SQLITE_DONE) return fail(db);
        sqlite3_reset(st);
    }
    return 1;
}

void gamesdb_set_today(GamesDb *db, const char *date)
{
    snprintf(db->today, sizeof db->today, "%s", date ? date : "");
}

static long gamesdb_count_listed(GamesDb *db)
{
    char out[24];
    return gamesdb_text(db, "SELECT count(*) FROM games WHERE listed = 1", out, (int)sizeof out) ? atol(out) : 0;
}

long gamesdb_each_new(GamesDb *db, const char *date, GamesDbIdFn fn, void *user)
{
    sqlite3_stmt *st;
    long n = 0;
    if (sqlite3_prepare_v2(db->sql, "SELECT id FROM games WHERE listed = 1 AND (first_seen >= ?1 OR updated >= ?1)"
                           " ORDER BY id COLLATE BINARY", -1, &st, NULL) != SQLITE_OK) { fail(db); return -1; }
    sqlite3_bind_text(st, 1, date, -1, SQLITE_TRANSIENT);
    while (sqlite3_step(st) == SQLITE_ROW) { fn(user, (const char *)sqlite3_column_text(st, 0)); n++; }
    sqlite3_finalize(st);
    return n;
}

long gamesdb_new_since(GamesDb *db, const char *date)
{
    sqlite3_stmt *st;
    long n = -1;
    if (sqlite3_prepare_v2(db->sql, "SELECT count(*) FROM games WHERE listed = 1 AND (first_seen >= ?1 OR updated >= ?1)",
                           -1, &st, NULL) != SQLITE_OK) { fail(db); return -1; }
    sqlite3_bind_text(st, 1, date, -1, SQLITE_TRANSIENT);
    if (sqlite3_step(st) == SQLITE_ROW) n = (long)sqlite3_column_int64(st, 0); else fail(db);
    sqlite3_finalize(st);
    return n;
}

/* FNV-1a, 64 bits, over every field the index keeps: the same list gives
 * the same mark, so an unchanged list is not written again. */
static void mark_text(unsigned long long *h, const char *s)
{
    if (s) for (; *s; s++) { *h ^= (unsigned char)*s; *h *= 1099511628211ULL; }
    *h ^= 0xFF; *h *= 1099511628211ULL;       /* the end of a field: "ab","c" is not "a","bc" */
}

static void mark_number(unsigned long long *h, long v)
{
    unsigned long u = (unsigned long)v;
    int i;
    for (i = 0; i < 4; i++) { *h ^= (u >> (8 * i)) & 0xFF; *h *= 1099511628211ULL; }   /* the same on every host */
}

static void mark_list(unsigned long long *h, char **names, int n)
{
    int i;
    mark_number(h, n);
    for (i = 0; i < n; i++) mark_text(h, names[i]);
}

void gamesdb_feed_mark(const Feed *f, char out[17])
{
    unsigned long long h = 14695981039346656037ULL;
    int i;
    mark_number(&h, f->count);
    mark_text(&h, f->generated);
    for (i = 0; i < f->count; i++) {
        const Port *p = &f->ports[i];
        mark_text(&h, p->id); mark_text(&h, p->source); mark_number(&h, games_folder_of(p)); mark_number(&h, p->number);
        mark_text(&h, p->origin); mark_text(&h, p->kind); mark_text(&h, p->title); mark_text(&h, p->year);
        mark_text(&h, p->state); mark_text(&h, p->fit); mark_text(&h, p->game_data); mark_text(&h, p->version);
        mark_text(&h, p->summary); mark_text(&h, p->author); mark_text(&h, p->category); mark_text(&h, p->cpu);
        mark_number(&h, p->fpu); mark_number(&h, p->space); mark_text(&h, p->download_url);
        mark_number(&h, p->download_size); mark_text(&h, p->download_sha256); mark_text(&h, p->release_url);
        mark_text(&h, p->port_repo); mark_text(&h, p->drawer); mark_text(&h, p->program); mark_text(&h, p->install);
        mark_list(&h, p->shots, p->nshots); mark_list(&h, p->requires, p->nrequires); mark_list(&h, p->modules, p->nmodules);
    }
    snprintf(out, 17, "%08lx%08lx", (unsigned long)(h >> 32), (unsigned long)(h & 0xFFFFFFFFUL));
}

int gamesdb_put_feed(GamesDb *db, const Feed *f)
{
    sqlite3_stmt *put = NULL, *shot = NULL, *need = NULL;
    int i, k, n = 0, ok = 0, dated;
    char mark[17], was[24];
    /* The same list as last time: nothing to write (lab, 23 Nov: writing 10,140
     * entries again at every start took minutes on a slow drive). */
    gamesdb_feed_mark(f, mark);
    if (gamesdb_text(db, "SELECT value FROM meta WHERE key = 'feed'", was, (int)sizeof was) && strcmp(was, mark) == 0)
        return (int)gamesdb_count_listed(db);
    /* The first fill of an empty index dates nothing. */
    dated = db->today[0] && gamesdb_count_listed(db) > 0;
    if (!run(db, "BEGIN")) return -1;
    /* Rows found on disk alone ("disk:...") were never listed and are not the feed's to change. */
    if (!run(db, "UPDATE games SET listed = 0 WHERE listed = 1") || !run(db, "DELETE FROM shots")
        || !run(db, "DELETE FROM needs")) goto done;
    if (sqlite3_prepare_v2(db->sql, put_sql, -1, &put, NULL) != SQLITE_OK
        || sqlite3_prepare_v2(db->sql, "INSERT OR IGNORE INTO shots VALUES (?1,?2,?3)", -1, &shot, NULL) != SQLITE_OK
        || sqlite3_prepare_v2(db->sql, "INSERT OR IGNORE INTO needs VALUES (?1,?2,?3)", -1, &need, NULL) != SQLITE_OK) {
        fail(db);
        goto done;
    }
    for (i = 0; i < f->count; i++) {
        const Port *p = &f->ports[i];
        if (!p->id || !*p->id || !p->title) continue;
        bind_text(put, 1, p->id);
        bind_text(put, 2, p->source ? p->source : ATLAS_SOURCE);
        bind_folder(put, 3, games_folder_of(p));
        if (p->number > 0) sqlite3_bind_int64(put, 4, p->number); else sqlite3_bind_null(put, 4);
        bind_text(put, 5, p->origin);
        bind_text(put, 6, p->kind && *p->kind ? p->kind : "game");
        bind_text(put, 7, p->title);
        bind_text(put, 8, p->year);
        bind_text(put, 9, p->state);
        bind_text(put, 10, p->fit);
        bind_text(put, 11, p->game_data);
        bind_text(put, 12, p->version);
        bind_text(put, 13, p->summary);
        bind_text(put, 14, p->author);
        bind_text(put, 15, p->category);
        bind_text(put, 16, p->cpu);
        sqlite3_bind_int(put, 17, p->fpu);
        sqlite3_bind_int64(put, 18, p->space);
        bind_text(put, 19, p->download_url);
        sqlite3_bind_int64(put, 20, p->download_size);
        bind_text(put, 21, p->download_sha256);
        bind_text(put, 22, p->release_url);
        bind_text(put, 23, p->port_repo);
        bind_text(put, 24, p->drawer);
        bind_text(put, 25, p->program);
        bind_text(put, 26, p->install);
        bind_text(put, 27, p->nshots > 0 ? p->shots[0] : NULL);
        if (dated) sqlite3_bind_text(put, 28, db->today, -1, SQLITE_STATIC); else sqlite3_bind_null(put, 28);
        if (sqlite3_step(put) != SQLITE_DONE) { fail(db); goto done; }
        sqlite3_reset(put);
        for (k = 0; k < p->nshots; k++) {
            if (!p->shots[k]) continue;
            bind_text(shot, 1, p->id);
            sqlite3_bind_int(shot, 2, k);
            bind_text(shot, 3, p->shots[k]);
            if (sqlite3_step(shot) != SQLITE_DONE) { fail(db); goto done; }
            sqlite3_reset(shot);
        }
        if (!put_list(db, need, p->id, "requires", p->requires, p->nrequires)
            || !put_list(db, need, p->id, "module", p->modules, p->nmodules)) goto done;
        n++;
    }
    /* What no source lists any more goes, unless it is on this machine. */
    if (!run(db, "DELETE FROM games WHERE listed = 0 AND id NOT LIKE 'disk:%'"
                 " AND installed_version IS NULL AND found = 0")) goto done;
    {
        char *q = sqlite3_mprintf("INSERT OR REPLACE INTO meta VALUES ('generated', %Q), ('feed', %Q)", f->generated, mark);
        ok = q && run(db, q);
        sqlite3_free(q);
    }
done:
    if (put) sqlite3_finalize(put);
    if (shot) sqlite3_finalize(shot);
    if (need) sqlite3_finalize(need);
    if (ok) ok = run(db, "COMMIT");
    if (!ok) { sqlite3_exec(db->sql, "ROLLBACK", NULL, NULL, NULL); return -1; }
    return n;
}

int gamesdb_set_installed(GamesDb *db, const char *id, const char *version, const char *path)
{
    sqlite3_stmt *st;
    int changed;
    if (sqlite3_prepare_v2(db->sql, "UPDATE games SET installed_version = ?2,"
                           " path = CASE WHEN ?2 IS NULL AND found = 0 THEN NULL WHEN ?3 IS NULL THEN path ELSE ?3 END"
                           " WHERE id = ?1", -1, &st, NULL) != SQLITE_OK) return fail(db);
    bind_text(st, 1, id);
    bind_text(st, 2, version);
    bind_text(st, 3, version ? path : NULL);
    if (sqlite3_step(st) != SQLITE_DONE) { sqlite3_finalize(st); return fail(db); }
    sqlite3_finalize(st);
    changed = sqlite3_changes(db->sql);
    /* A game the sources dropped stayed only because it was installed. */
    if (changed && !version)
        run(db, "DELETE FROM games WHERE listed = 0 AND id NOT LIKE 'disk:%' AND installed_version IS NULL AND found = 0");
    return changed > 0;
}

int gamesdb_clear_found(GamesDb *db)
{
    if (!run(db, "BEGIN")) return 0;
    if (run(db, "DELETE FROM games WHERE id LIKE 'disk:%'")
        && run(db, "DELETE FROM folders WHERE folder >= " GF_COUNT_TEXT)      /* the player's own are found again */
        && run(db, "UPDATE games SET found = 0, found_program = NULL,"
                   " path = CASE WHEN installed_version IS NULL THEN NULL ELSE path END WHERE found = 1")
        && run(db, "DELETE FROM games WHERE listed = 0 AND installed_version IS NULL AND found = 0")
        && run(db, "COMMIT")) return 1;
    sqlite3_exec(db->sql, "ROLLBACK", NULL, NULL, NULL);
    return 0;
}

int gamesdb_folder(GamesDb *db, const char *name)
{
    sqlite3_stmt *st;
    int folder = games_folder_named(name), n;
    if (folder != GF_NONE) return folder;
    if (!name || !*name) return -1;
    if (sqlite3_prepare_v2(db->sql, "SELECT folder FROM folders WHERE name = ?1 COLLATE NOCASE", -1, &st, NULL) != SQLITE_OK) {
        fail(db);
        return -1;
    }
    bind_text(st, 1, name);
    n = sqlite3_step(st);
    if (n == SQLITE_ROW) folder = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    if (n == SQLITE_ROW) return folder;
    if (n != SQLITE_DONE) { fail(db); return -1; }
    if (sqlite3_prepare_v2(db->sql, "INSERT INTO folders SELECT max(folder) + 1, ?1 FROM folders", -1, &st, NULL) != SQLITE_OK) {
        fail(db);
        return -1;
    }
    bind_text(st, 1, name);
    n = sqlite3_step(st);
    sqlite3_finalize(st);
    if (n != SQLITE_DONE) { fail(db); return -1; }
    return (int)sqlite3_last_insert_rowid(db->sql);
}

/* The folder's name as the index has it; 0 when it has no such folder. */
static int folder_name(GamesDb *db, int folder, char *out, int max)
{
    sqlite3_stmt *st;
    int got = 0;
    out[0] = 0;
    if (sqlite3_prepare_v2(db->sql, "SELECT name FROM folders WHERE folder = ?1", -1, &st, NULL) != SQLITE_OK) return fail(db);
    sqlite3_bind_int(st, 1, folder);
    if (sqlite3_step(st) == SQLITE_ROW) {
        snprintf(out, (size_t)max, "%s", (const char *)sqlite3_column_text(st, 0));
        got = 1;
    }
    sqlite3_finalize(st);
    return got;
}

#define FOUND_SET "UPDATE games SET found = 1, path = ?3, found_program = ?4 WHERE id = (SELECT id FROM games WHERE "
#define FOUND_PICK " AND listed = 1 ORDER BY (source = '" ATLAS_SOURCE "') DESC, id LIMIT 1)"
#define FOUND_IN_FOLDER FOUND_SET "drawer = ?2 AND folder = ?1" FOUND_PICK
#define FOUND_ANYWHERE  FOUND_SET "drawer = ?2 AND ?1 >= 0" FOUND_PICK

const char *gamesdb_found_sql(int in_folder) { return in_folder ? FOUND_IN_FOLDER : FOUND_ANYWHERE; }

int gamesdb_begin(GamesDb *db) { return run(db, "BEGIN"); }

int gamesdb_end(GamesDb *db, int keep)
{
    if (keep && run(db, "COMMIT")) return 1;
    sqlite3_exec(db->sql, "ROLLBACK", NULL, NULL, NULL);
    return 0;
}

int gamesdb_put_found(GamesDb *db, int folder, const char *drawer, const char *path, const char *program)
{
    sqlite3_stmt *st;
    char id[200], low[120], in[64];
    int i, n;
    if (folder < 0 || !drawer || !*drawer || !path || !folder_name(db, folder, in, (int)sizeof in)) return -1;
    /* Two forms rather than "folder = ?1 OR ?1 >= 5", which SQLite answers by reading every row. */
    if (sqlite3_prepare_v2(db->sql, folder < GF_COUNT ? FOUND_IN_FOLDER : FOUND_ANYWHERE, -1, &st, NULL) != SQLITE_OK) {
        fail(db);
        return -1;
    }
    sqlite3_bind_int(st, 1, folder);
    bind_text(st, 2, drawer);
    bind_text(st, 3, path);
    bind_text(st, 4, program);
    n = sqlite3_step(st);
    sqlite3_finalize(st);
    if (n != SQLITE_DONE) { fail(db); return -1; }
    if (sqlite3_changes(db->sql) > 0) return 1;

    for (i = 0; drawer[i] && i < (int)sizeof low - 1; i++) low[i] = (char)tolower((unsigned char)drawer[i]);
    low[i] = 0;
    snprintf(id, sizeof id, "disk:%s/%s", in, low);
    for (i = 5; id[i] && id[i] != '/'; i++) id[i] = (char)tolower((unsigned char)id[i]);
    if (sqlite3_prepare_v2(db->sql, "INSERT INTO games (id, folder, kind, title, drawer, program, found, path, found_program)"
                           " VALUES (?1, ?2, 'game', ?3, ?3, ?5, 1, ?4, ?5)"
                           " ON CONFLICT (id) DO UPDATE SET found = 1, path = excluded.path, program = excluded.program,"
                           " found_program = excluded.found_program", -1, &st, NULL) != SQLITE_OK) {
        fail(db);
        return -1;
    }
    bind_text(st, 1, id);
    sqlite3_bind_int(st, 2, folder);
    bind_text(st, 3, drawer);
    bind_text(st, 4, path);
    bind_text(st, 5, program);
    n = sqlite3_step(st);
    sqlite3_finalize(st);
    if (n != SQLITE_DONE) { fail(db); return -1; }
    return 0;
}

long gamesdb_each_found(GamesDb *db, GamesDbFoundFn fn, void *user)
{
    sqlite3_stmt *st;
    long n = 0;
    int rc;
    if (sqlite3_prepare_v2(db->sql, "SELECT g.id, f.name, g.title, g.path FROM games g JOIN folders f ON f.folder = g.folder"
                           " WHERE g.id LIKE 'disk:%' ORDER BY g.folder, g.title COLLATE NOCASE, g.id", -1, &st, NULL) != SQLITE_OK) {
        fail(db);
        return -1;
    }
    while ((rc = sqlite3_step(st)) == SQLITE_ROW) {
        fn(user, (const char *)sqlite3_column_text(st, 0), (const char *)sqlite3_column_text(st, 1),
           (const char *)sqlite3_column_text(st, 2), (const char *)sqlite3_column_text(st, 3));
        n++;
    }
    sqlite3_finalize(st);
    if (rc != SQLITE_DONE) { fail(db); return -1; }
    return n;
}

long gamesdb_count(GamesDb *db, int folder, int here)
{
    sqlite3_stmt *st;
    long n = -1;
    if (sqlite3_prepare_v2(db->sql, "SELECT count(*) FROM games WHERE kind = 'game' AND (?1 IS NULL OR folder = ?1)"
                           " AND (?2 = 0 OR found = 1 OR installed_version IS NOT NULL)", -1, &st, NULL) != SQLITE_OK) {
        fail(db);
        return -1;
    }
    bind_folder(st, 1, folder);
    sqlite3_bind_int(st, 2, here ? 1 : 0);
    if (sqlite3_step(st) == SQLITE_ROW) n = (long)sqlite3_column_int64(st, 0); else fail(db);
    sqlite3_finalize(st);
    return n;
}

int gamesdb_text(GamesDb *db, const char *sql, char *out, int max)
{
    sqlite3_stmt *st;
    int got = 0;
    if (max > 0) out[0] = 0;
    if (sqlite3_prepare_v2(db->sql, sql, -1, &st, NULL) != SQLITE_OK) return fail(db);
    if (sqlite3_step(st) == SQLITE_ROW) {
        const unsigned char *t = sqlite3_column_text(st, 0);
        snprintf(out, (size_t)max, "%s", t ? (const char *)t : "");
        got = 1;
    }
    sqlite3_finalize(st);
    return got;
}
