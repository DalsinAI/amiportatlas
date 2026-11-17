/* AmiPortAtlas host tests: the games index (src/gamesdb.c), built with the
 * host's cc and its SQLite (make test). MIT, (c) 2026 The AmiCradle Team.
 *
 *   test_gamesdb [ports.json]   with a file, also indexes a real feed
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/gamesdb.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static GamesDb *db;

static const char *ask(const char *sql)
{
    static char out[4][256];
    static int at;
    at = (at + 1) % 4;
    if (!gamesdb_text(db, sql, out[at], (int)sizeof out[at])) return "(none)";
    return out[at];
}

static int is(const char *sql, const char *want)
{
    const char *got = ask(sql);
    if (strcmp(got, want) == 0) return 1;
    printf("  %s\n  gave \"%s\", not \"%s\"\n", sql, got, want);
    return 0;
}

static char news[8][40];
static int nnews;
static void news_fn(void *user, const char *id) { (void)user; if (nnews < 8) snprintf(news[nnews++], 40, "%s", id); }

static char found_text[400];
static void found_fn(void *user, const char *id, const char *folder, const char *title, const char *path)
{
    size_t at = strlen(found_text);
    (void)user;
    snprintf(found_text + at, sizeof found_text - at, "%s|%s|%s|%s;", id, folder, title, path);
}

static Feed *feed(const char *json)
{
    const char *why = NULL;
    Feed *f = feed_parse(json, (long)strlen(json), &why);
    if (!f) printf("feed: %s\n", why ? why : "?");
    return f;
}

/* Two sources' feeds as AmiPortAtlas makes them one list. */
static Feed *one_list(const char *atlas, const char *other, const char *other_id)
{
    Feed *fs[2];
    const char *ids[2];
    fs[0] = feed(atlas);
    fs[1] = other ? feed(other) : NULL;
    ids[0] = ATLAS_SOURCE;
    ids[1] = other_id;
    return feed_merge(fs, ids, 2);
}

static const char atlas_json[] =
    "{ \"schema_version\": 1, \"generated\": \"2026-11-17\", \"ports\": [\n"
    " { \"id\": \"kobo\", \"number\": 1, \"title\": \"Kobo Deluxe\", \"year\": \"1995\", \"state\": \"released\", \"fit\": \"040\",\n"
    "   \"game_data\": \"free\", \"version\": \"0.5.1\", \"drawer\": \"KoboDeluxe\", \"program\": \"kobodl\", \"cpu\": \"68040\",\n"
    "   \"fpu\": true, \"space\": 4000000, \"download_url\": \"https://example.org/kobo.lha\", \"download_size\": 1468000,\n"
    "   \"download_sha256\": \"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\",\n"
    "   \"requires\": [\"GFOpenGPU\"], \"modules\": [\"SDL2.module\", \"SDL2_image.module\"],\n"
    "   \"screenshots\": [\"https://example.org/k1.png\", \"https://example.org/k2.png\"] },\n"
    " { \"id\": \"canvas-skies\", \"number\": 2, \"origin\": \"original\", \"title\": \"Canvas Skies\", \"state\": \"testing\",\n"
    "   \"drawer\": \"CanvasSkies\", \"program\": \"CanvasSkies\" },\n"
    " { \"id\": \"cafe\", \"number\": 3, \"title\": \"Caf\\u00e9 Racer\", \"state\": \"not_started\" },\n"
    " { \"id\": \"4pane\", \"number\": 4, \"kind\": \"application\", \"title\": \"4Pane\", \"state\": \"released\" }\n"
    "] }";

static const char aminet_json[] =
    "{ \"schema_version\": 1, \"generated\": \"2026-11-16\", \"ports\": [\n"
    " { \"id\": \"kobo\", \"title\": \"Kobo (Aminet)\", \"short\": \"A space shooter\", \"author\": \"Somebody\",\n"
    "   \"category\": \"game/shoot\", \"drawer\": \"Kobo\", \"program\": \"Kobo\", \"install\": \"unpack\" },\n"
    " { \"id\": \"dirutil\", \"kind\": \"tool\", \"title\": \"DirUtil\", \"category\": \"util/dir\" }\n"
    "] }";

static void test_folders(void)
{
    Port p;
    CHECK(GF_COUNT == 5);
    CHECK(strcmp(games_folder_name(GF_PORTS), "Ports") == 0 && strcmp(games_folder_name(GF_WHDLOAD), "WHDLoad") == 0);
    CHECK(strcmp(games_folder_name(GF_ITCH), "Itch.io") == 0 && strcmp(games_folder_name(GF_AMINET), "Aminet") == 0);
    CHECK(strcmp(games_folder_name(GF_EXCLUSIVE), "Exclusive") == 0);
    CHECK(games_folder_name(GF_NONE) == NULL && games_folder_name(GF_COUNT) == NULL);
    CHECK(games_folder_named("whdload") == GF_WHDLOAD && games_folder_named("ITCH.IO") == GF_ITCH);
    CHECK(games_folder_named("Itch") == GF_NONE && games_folder_named("") == GF_NONE && games_folder_named(NULL) == GF_NONE);
    CHECK(games_folder_of_source("amicradle") == GF_PORTS && games_folder_of_source("aminet-game") == GF_AMINET);
    CHECK(games_folder_of_source("itch") == GF_ITCH && games_folder_of_source("itch-io") == GF_ITCH);
    CHECK(games_folder_of_source("whdload") == GF_WHDLOAD && games_folder_of_source("whdload-pimiga") == GF_WHDLOAD);
    CHECK(games_folder_of_source("os4depot") == GF_NONE && games_folder_of_source("mygames") == GF_NONE);
    CHECK(games_folder_of_source(NULL) == GF_NONE);
    CHECK(games_shelf_drawer("A") && games_shelf_drawer("#") && games_shelf_drawer("0-9"));
    CHECK(!games_shelf_drawer("AB") && !games_shelf_drawer("") && !games_shelf_drawer(NULL) && !games_shelf_drawer("0-99"));
    memset(&p, 0, sizeof p);
    CHECK(games_folder_of(&p) == GF_PORTS);                 /* no source: the Atlas's own */
    p.source = "aminet-game";  CHECK(games_folder_of(&p) == GF_AMINET);
    p.origin = "original";     CHECK(games_folder_of(&p) == GF_EXCLUSIVE);   /* ours, wherever it is listed */
    p.kind = "tool";           CHECK(games_folder_of(&p) == GF_NONE);        /* not a game */
    p.kind = "game"; p.origin = "port"; p.source = "os4depot"; CHECK(games_folder_of(&p) == GF_NONE);
}

static void test_index(const char *path)
{
    char why[200];
    Feed *f;
    Sources *s;
    const char *swhy = NULL;
    static const char master[] =
        "{ \"schema_version\": 1, \"sources\": ["
        " {\"id\": \"amicradle\", \"name\": \"AmiCradle Port Atlas\", \"index\": \"https://www.amicradle.com/ports/ports.json\","
        "  \"home\": \"https://www.amicradle.com/ports/\", \"on\": true, \"checked\": true},"
        " {\"id\": \"aminet-game\", \"name\": \"Aminet: games\", \"index\": null, \"on\": true},"
        " {\"id\": \"os4depot\", \"name\": \"OS4Depot\", \"index\": null, \"on\": false} ] }";

    remove(path);
    db = gamesdb_open(path, why, sizeof why);
    CHECK(db != NULL);
    if (!db) { printf("%s\n", why); return; }
    CHECK(is("PRAGMA user_version", "2"));
    CHECK(is("SELECT group_concat(name, ',') FROM (SELECT name FROM folders ORDER BY folder)",
             "Ports,WHDLoad,Itch.io,Aminet,Exclusive"));
    CHECK(gamesdb_count(db, GF_NONE, 0) == 0);

    s = sources_parse(master, (long)strlen(master), &swhy);
    CHECK(s != NULL);
    if (s) {
        CHECK(gamesdb_put_sources(db, s));
        CHECK(gamesdb_put_sources(db, s));                   /* again: replaced, not doubled */
        CHECK(is("SELECT count(*) FROM sources", "3"));
        CHECK(is("SELECT f.name FROM sources s JOIN folders f ON f.folder = s.folder WHERE s.id = 'aminet-game'", "Aminet"));
        CHECK(is("SELECT folder IS NULL FROM sources WHERE id = 'os4depot'", "1"));
        CHECK(is("SELECT is_on || checked FROM sources WHERE id = 'amicradle'", "11"));
        sources_free(s);
    }

    f = one_list(atlas_json, aminet_json, "aminet-game");
    CHECK(f != NULL);
    if (!f) return;
    gamesdb_set_today(db, "2026-11-17");
    CHECK(gamesdb_put_feed(db, f) == 6);
    CHECK(gamesdb_new_since(db, "2026-01-01") == 0);         /* the first fill: nothing is news */
    CHECK(is("SELECT count(*) FROM games WHERE first_seen IS NOT NULL OR updated IS NOT NULL", "0"));
    CHECK(is("SELECT value FROM meta WHERE key = 'generated'", "2026-11-17"));
    CHECK(gamesdb_count(db, GF_NONE, 0) == 4);               /* 4Pane and DirUtil are no games */
    CHECK(gamesdb_count(db, GF_PORTS, 0) == 2 && gamesdb_count(db, GF_EXCLUSIVE, 0) == 1);
    CHECK(gamesdb_count(db, GF_AMINET, 0) == 1 && gamesdb_count(db, GF_WHDLOAD, 0) == 0 && gamesdb_count(db, GF_ITCH, 0) == 0);
    CHECK(gamesdb_count(db, GF_NONE, 1) == 0);
    CHECK(is("SELECT title || '|' || year || '|' || state || '|' || fit || '|' || game_data || '|' || version || '|' ||"
             " drawer || '|' || program || '|' || cpu || '|' || fpu || '|' || space || '|' || download_size || '|' || shot"
             " FROM games WHERE id = 'kobo'",
             "Kobo Deluxe|1995|released|040|free|0.5.1|KoboDeluxe|kobodl|68040|1|4000000|1468000|https://example.org/k1.png"));
    CHECK(is("SELECT source || '|' || kind || '|' || summary || '|' || author || '|' || category || '|' || install"
             " FROM games WHERE id = 'kobo@aminet-game'", "aminet-game|game|A space shooter|Somebody|game/shoot|unpack"));
    CHECK(is("SELECT hex(title) FROM games WHERE id = 'cafe'", "436166C3A9205261636572"));   /* UTF-8 in the index */
    CHECK(is("SELECT group_concat(url, ' ') FROM (SELECT url FROM shots WHERE game = 'kobo' ORDER BY n)",
             "https://example.org/k1.png https://example.org/k2.png"));
    CHECK(is("SELECT group_concat(what || '=' || name, ' ') FROM (SELECT * FROM needs WHERE game = 'kobo' ORDER BY what, name)",
             "module=SDL2.module module=SDL2_image.module requires=GFOpenGPU"));
    CHECK(is("SELECT kind || '|' || (folder IS NULL) FROM games WHERE id = '4pane'", "application|1"));
    CHECK(is("SELECT group_concat(title, ',') FROM (SELECT title FROM games WHERE kind = 'game' ORDER BY title)",
             "Caf\xC3\xA9 Racer,Canvas Skies,Kobo (Aminet),Kobo Deluxe"));
    CHECK(is("SELECT id FROM games WHERE title = 'KOBO DELUXE'", "kobo"));   /* names without case */

    /* What the machine knows. */
    CHECK(gamesdb_set_installed(db, "kobo", "0.5.0", "Games:Ports/KoboDeluxe"));
    CHECK(!gamesdb_set_installed(db, "nothing", "1", "Games:Ports/Nothing"));
    CHECK(gamesdb_count(db, GF_PORTS, 1) == 1 && gamesdb_count(db, GF_NONE, 1) == 1);
    CHECK(gamesdb_put_found(db, GF_EXCLUSIVE, "canvasskies", "Games:Exclusive/CanvasSkies", "CanvasSkies") == 1);
    CHECK(gamesdb_put_found(db, GF_WHDLOAD, "Lemmings", "Games:WHDLoad/Lemmings", "Lemmings.slave") == 0);
    CHECK(gamesdb_put_found(db, GF_WHDLOAD, "LEMMINGS", "Games:WHDLoad/LEMMINGS", NULL) == 0);     /* the same drawer */
    CHECK(gamesdb_put_found(db, GF_PORTS, "Kobo", "Games:Ports/Kobo", NULL) == 0);     /* Aminet's Kobo is not in Ports */
    CHECK(gamesdb_put_found(db, GF_COUNT, "X", "Games:X", NULL) == -1 && gamesdb_put_found(db, GF_PORTS, "", "Games:Ports", NULL) == -1);
    CHECK(is("SELECT found || '|' || path || '|' || found_program FROM games WHERE id = 'canvas-skies'",
             "1|Games:Exclusive/CanvasSkies|CanvasSkies"));
    CHECK(is("SELECT title || '|' || path || '|' || (program IS NULL) || '|' || (source IS NULL) || '|' || listed"
             " FROM games WHERE id = 'disk:whdload/lemmings'", "Lemmings|Games:WHDLoad/LEMMINGS|1|1|0"));
    CHECK(is("SELECT count(*) FROM games WHERE id LIKE 'disk:%'", "2"));
    CHECK(gamesdb_count(db, GF_WHDLOAD, 0) == 1 && gamesdb_count(db, GF_WHDLOAD, 1) == 1);
    CHECK(gamesdb_count(db, GF_NONE, 1) == 4);
    feed_free(f);

    /* The sources read again: Kobo has a new version, Café Racer and Canvas Skies are gone, one is new. */
    f = one_list("{ \"schema_version\": 1, \"generated\": \"2026-11-18\", \"ports\": ["
                 " { \"id\": \"kobo\", \"number\": 1, \"title\": \"Kobo Deluxe\", \"state\": \"released\", \"version\": \"0.5.2\","
                 "   \"drawer\": \"KoboDeluxe\", \"screenshots\": [\"https://example.org/k3.png\"] },"
                 " { \"id\": \"xboing\", \"number\": 5, \"title\": \"XBoing\", \"state\": \"released\", \"drawer\": \"XBoing\" } ] }", NULL, NULL);
    CHECK(f != NULL);
    if (!f) return;
    gamesdb_set_today(db, "2026-11-18");
    CHECK(gamesdb_put_feed(db, f) == 2);
    /* The New page: XBoing is new, Kobo Deluxe has a new version; in order, the newest first. */
    CHECK(is("SELECT group_concat(title || ':' || ifnull(first_seen, '-') || ':' || ifnull(updated, '-'), ' ') FROM"
             " (SELECT * FROM games WHERE listed = 1 AND (first_seen >= '2026-11-18' OR updated >= '2026-11-18') ORDER BY title)",
             "Kobo Deluxe:-:2026-11-18 XBoing:2026-11-18:-"));
    CHECK(gamesdb_new_since(db, "2026-11-18") == 2 && gamesdb_new_since(db, "2026-11-19") == 0);
    nnews = 0;
    CHECK(gamesdb_each_new(db, "2026-11-18", news_fn, NULL) == 2 && nnews == 2);
    CHECK(strcmp(news[0], "kobo") == 0 && strcmp(news[1], "xboing") == 0);      /* by id, for a binary search */
    nnews = 0;
    CHECK(gamesdb_each_new(db, "2026-11-19", news_fn, NULL) == 0 && nnews == 0);
    gamesdb_set_today(db, "2026-11-20");
    CHECK(gamesdb_put_feed(db, f) == 2);                     /* read again with nothing changed: no news */
    CHECK(gamesdb_new_since(db, "2026-11-19") == 0 && gamesdb_new_since(db, "2026-11-18") == 2);
    gamesdb_set_today(db, NULL);
    CHECK(is("SELECT version || '|' || installed_version || '|' || path || '|' || listed FROM games WHERE id = 'kobo'",
             "0.5.2|0.5.0|Games:Ports/KoboDeluxe|1"));            /* an update shows: listed 0.5.2, here 0.5.0 */
    CHECK(is("SELECT count(*) || '|' || max(url) FROM shots WHERE game = 'kobo'", "1|https://example.org/k3.png"));
    CHECK(is("SELECT count(*) FROM needs", "0"));
    CHECK(is("SELECT count(*) FROM games WHERE id IN ('cafe', '4pane', 'kobo@aminet-game')", "0"));   /* listed nowhere, not here */
    CHECK(is("SELECT listed || '|' || found FROM games WHERE id = 'canvas-skies'", "0|1"));           /* here, so kept */
    CHECK(is("SELECT count(*) FROM games WHERE id LIKE 'disk:%'", "2"));
    CHECK(is("SELECT value FROM meta WHERE key = 'generated'", "2026-11-18"));
    feed_free(f);

    /* A drawer the player made at the top of the drive is a folder too, found each time. */
    {
        int own = gamesdb_folder(db, "More Games");
        CHECK(gamesdb_folder(db, "whdload") == GF_WHDLOAD && own == GF_COUNT && gamesdb_folder(db, "more games") == own);
        CHECK(gamesdb_folder(db, "Demos") == own + 1 && gamesdb_folder(db, "") == -1);
        CHECK(gamesdb_put_found(db, own, "Zeewolf", "Games:More Games/Zeewolf", NULL) == 0);
        CHECK(gamesdb_put_found(db, own, "XBoing", "Games:More Games/XBoing", NULL) == 1);   /* listed for Ports, kept here */
        CHECK(gamesdb_put_found(db, own + 2, "X", "Games:X/X", NULL) == -1);                 /* no such folder */
        CHECK(is("SELECT found || '|' || path FROM games WHERE id = 'xboing'", "1|Games:More Games/XBoing"));
        CHECK(is("SELECT title || '|' || folder FROM games WHERE id = 'disk:more games/zeewolf'", "Zeewolf|5"));
        found_text[0] = 0;
        CHECK(gamesdb_each_found(db, found_fn, NULL) == 3);
        CHECK(strcmp(found_text, "disk:ports/kobo|Ports|Kobo|Games:Ports/Kobo;"
                                 "disk:whdload/lemmings|WHDLoad|Lemmings|Games:WHDLoad/LEMMINGS;"
                                 "disk:more games/zeewolf|More Games|Zeewolf|Games:More Games/Zeewolf;") == 0);
    }

    /* The drive looked through again: Canvas Skies was deleted by hand, Lemmings is still there. */
    CHECK(gamesdb_clear_found(db));
    CHECK(is("SELECT count(*) FROM games WHERE id LIKE 'disk:%' OR id = 'canvas-skies'", "0"));
    CHECK(is("SELECT count(*) || '|' || max(folder) FROM folders", "5|4"));
    CHECK(is("SELECT found || '|' || (path IS NULL) FROM games WHERE id = 'xboing'", "0|1"));
    CHECK(is("SELECT path FROM games WHERE id = 'kobo'", "Games:Ports/KoboDeluxe"));    /* installed through AmiPortAtlas: stays */
    CHECK(gamesdb_put_found(db, GF_WHDLOAD, "Lemmings", "Games:WHDLoad/Lemmings", NULL) == 0);
    CHECK(gamesdb_put_found(db, GF_PORTS, "kobodeluxe", "Games:Ports/KoboDeluxe", "kobodl") == 1);
    CHECK(gamesdb_set_installed(db, "kobo", NULL, NULL));           /* removed from the installed list, still on disk */
    CHECK(is("SELECT (installed_version IS NULL) || '|' || found || '|' || path FROM games WHERE id = 'kobo'",
             "1|1|Games:Ports/KoboDeluxe"));
    CHECK(gamesdb_set_installed(db, "xboing", "2.4", "Games:Ports/XBoing"));
    CHECK(gamesdb_set_installed(db, "xboing", NULL, NULL));
    CHECK(is("SELECT (path IS NULL) || '|' || listed FROM games WHERE id = 'xboing'", "1|1"));

    /* Kept from one run to the next; an index from a newer AmiPortAtlas is refused, an older one made again. */
    gamesdb_close(db);
    db = gamesdb_open(path, why, sizeof why);
    CHECK(db != NULL);
    if (!db) return;
    CHECK(gamesdb_count(db, GF_NONE, 0) == 3 && gamesdb_count(db, GF_NONE, 1) == 2);
    CHECK(gamesdb_text(db, "PRAGMA user_version = 3", why, sizeof why) == 0);
    gamesdb_close(db);
    db = gamesdb_open(path, why, sizeof why);
    CHECK(db == NULL && strstr(why, "newer AmiPortAtlas") != NULL);
    {
        FILE *fh = fopen(path, "wb");
        CHECK(fh != NULL);
        if (fh) { fputs("this is not a database, it is a text file of more than sixteen bytes\n", fh); fclose(fh); }
    }
    db = gamesdb_open(path, why, sizeof why);
    CHECK(db == NULL && strstr(why, "not a games index") != NULL);
    remove(path);
    db = gamesdb_open(path, why, sizeof why);
    CHECK(db != NULL);
    if (!db) return;
    CHECK(gamesdb_text(db, "PRAGMA user_version = 0", why, sizeof why) == 0);
    gamesdb_close(db);
    db = gamesdb_open(path, why, sizeof why);                       /* schema 0: made again */
    CHECK(db != NULL);
    if (!db) return;
    CHECK(is("PRAGMA user_version", "2") && gamesdb_count(db, GF_NONE, 0) == 0);
    gamesdb_close(db);
    db = NULL;
    remove(path);
}

static void test_real_feed(const char *file, const char *path)
{
    FILE *fh = fopen(file, "rb");
    char *text, why[200];
    const char *fwhy = NULL;
    long len;
    Feed *one, *fs[1];
    const char *ids[1] = { ATLAS_SOURCE };
    int i, games = 0, ours = 0;
    if (!fh) { printf("FAIL: cannot open %s\n", file); failures++; return; }
    fseek(fh, 0, SEEK_END); len = ftell(fh); fseek(fh, 0, SEEK_SET);
    text = malloc((size_t)len + 1);
    if (!text || fread(text, 1, (size_t)len, fh) != (size_t)len) { printf("FAIL: cannot read %s\n", file); failures++; fclose(fh); free(text); return; }
    fclose(fh);
    fs[0] = feed_parse(text, len, &fwhy);
    free(text);
    CHECK(fs[0] != NULL);
    if (!fs[0]) return;
    one = feed_merge(fs, ids, 1);
    CHECK(one != NULL);
    if (!one) return;
    for (i = 0; i < one->count; i++) {
        if (games_folder_of(&one->ports[i]) != GF_NONE) games++;
        if (games_folder_of(&one->ports[i]) == GF_EXCLUSIVE) ours++;
    }
    remove(path);
    db = gamesdb_open(path, why, sizeof why);
    CHECK(db != NULL);
    if (!db) return;
    CHECK(gamesdb_put_feed(db, one) == one->count);
    CHECK(gamesdb_put_feed(db, one) == one->count);          /* a second time changes nothing */
    CHECK(gamesdb_count(db, GF_NONE, 0) == games && games > 0);
    CHECK(gamesdb_count(db, GF_EXCLUSIVE, 0) == ours);
    CHECK(gamesdb_count(db, GF_PORTS, 0) == games - ours);
    CHECK(is("PRAGMA integrity_check", "ok"));
    printf("%s: %d entries, %d games (%d in Ports, %d in Exclusive)\n", file, one->count, games, games - ours, ours);
    gamesdb_close(db);
    db = NULL;
    remove(path);
    feed_free(one);
}

int main(int argc, char **argv)
{
    const char *path = "build/test-games.db";
    test_folders();
    test_index(path);
    if (argc > 1) test_real_feed(argv[1], path);
    if (failures) { printf("%d FAILED\n", failures); return 1; }
    printf("games index: all passed\n");
    return 0;
}
