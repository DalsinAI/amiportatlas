/* AmiPortAtlas host tests: the feed reader and SHA-256, built with the host's
 * cc (make test). MIT, (c) 2026 The AmiCradle Team.
 *
 *   test_host [ports.json]   with a file, also reads a real feed
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/feed.h"
#include "../src/sha256.h"
#include "../src/match.h"
#include "../src/sources.h"
#include "../src/index.h"

static int failures;

#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

static void hex(const unsigned char d[32], char out[65])
{
    int i;
    for (i = 0; i < 32; i++) sprintf(out + 2 * i, "%02x", d[i]);
}

static void test_sha256(void)
{
    struct { const char *in; const char *out; } v[] = {
        { "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
        { "abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad" },
        { "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1" },
    };
    unsigned char d[32];
    char h[65];
    Sha256 s;
    int i;
    for (i = 0; i < 3; i++) {
        sha256_init(&s);
        sha256_update(&s, v[i].in, (unsigned long)strlen(v[i].in));
        sha256_final(&s, d);
        hex(d, h);
        CHECK(strcmp(h, v[i].out) == 0);
        CHECK(sha256_matches(d, v[i].out));
    }
    /* A million 'a's, fed in uneven pieces. */
    sha256_init(&s);
    for (i = 0; i < 1000000; ) {
        static char a[997];
        int n = 1000000 - i < 997 ? 1000000 - i : 997;
        memset(a, 'a', sizeof a);
        sha256_update(&s, a, (unsigned long)n);
        i += n;
    }
    sha256_final(&s, d);
    hex(d, h);
    CHECK(strcmp(h, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0") == 0);
    CHECK(!sha256_matches(d, "cdc76e5c"));
    CHECK(sha256_matches(d, "CDC76E5C9914FB9281A1C7E284D73E67F1809A48A497200E046D39CCC7112CD0"));
}

static void test_feed_small(void)
{
    const char *json =
        "\xEF\xBB\xBF{ \"schema_version\": 1, \"generated\": \"2026-11-04\", \"source\": {\"x\": [1, {\"y\": null}]},\n"
        "  \"ports\": [\n"
        "   { \"id\": \"g-b\", \"number\": 2, \"title\": \"Beta \\u2013 Caf\\u00e9\", \"year\": \"1995 (XKobo)\",\n"
        "     \"state\": \"released\", \"fit\": \"040\", \"fpu\": true, \"space\": 4000000,\n"
        "     \"download_url\": \"https://example.org/b.lha\", \"download_size\": 1468000,\n"
        "     \"download_sha256\": \"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\",\n"
        "     \"drawer\": \"Beta\", \"program\": \"beta\", \"modules\": [\"SDL2.module\", \"SDL2_image.module\"],\n"
        "     \"screenshot\": \"https://example.org/1.png\", \"screenshots\": [\"https://example.org/1.png\", \"https://example.org/2.png\"],\n"
        "     \"runs_on\": [\"68040\", \"AC090\"], \"future_field\": {\"a\": [true, false]} },\n"
        "   { \"id\": \"g-a\", \"number\": 1, \"title\": \"Alpha \xE2\x80\xA6\", \"year\": null, \"state\": \"testing\",\n"
        "     \"fit\": \"GPU\", \"download_url\": null, \"requires\": [], \"screenshot\": \"https://example.org/a.png\" },\n"
        "   { \"id\": \"g-c\", \"origin\": \"original\", \"number\": 3, \"title\": \"gamma\", \"year\": \"1987\", \"state\": \"not_started\", \"fit\": \"060\",\n"
        "     \"fpu\": \"68881 (the 68040 FPU)\" },\n"
        "   { \"id\": \"g-none\", \"number\": 4 }\n"
        "  ] }";
    const char *why = NULL;
    Feed *f = feed_parse(json, (long)strlen(json), &why);
    Port *list[3];
    int i;
    CHECK(f != NULL);
    if (!f) { printf("  why: %s\n", why); return; }
    CHECK(f->schema == 1);
    CHECK(f->count == 3);  /* the port with no title is left out */
    CHECK(strcmp(f->ports[0].title, "Beta - Caf\xE9") == 0);
    CHECK(strcmp(f->ports[1].title, "Alpha ...") == 0);
    CHECK(f->ports[0].fpu == 1 && f->ports[0].space == 4000000 && f->ports[0].download_size == 1468000);
    CHECK(f->ports[0].nmodules == 2 && strcmp(f->ports[0].modules[1], "SDL2_image.module") == 0);
    CHECK(f->ports[0].nshots == 2 && strcmp(f->ports[0].shots[1], "https://example.org/2.png") == 0);
    CHECK(f->ports[1].nshots == 1 && strcmp(f->ports[1].shots[0], "https://example.org/a.png") == 0);
    CHECK(f->ports[1].year == NULL && f->ports[1].download_url == NULL);
    CHECK(port_installable(&f->ports[0]));
    CHECK(!port_installable(&f->ports[1]));
    CHECK(strcmp(port_state_text(&f->ports[2]), "Not started") == 0);
    CHECK(port_is_original(&f->ports[2]) && !port_is_original(&f->ports[0]));
    CHECK(f->ports[2].fpu == 1 && f->ports[1].fpu == 0);   /* the Atlas says which FPU as a string */
    CHECK(port_plain_name("Kobo") && port_plain_name("Fish Fillets NG"));
    CHECK(!port_plain_name("") && !port_plain_name(NULL) && !port_plain_name("/") && !port_plain_name("Work:")
          && !port_plain_name("Games/Kobo") && !port_plain_name("a\"b"));

    for (i = 0; i < 3; i++) list[i] = &f->ports[i];
    feed_sort(list, 3, SORT_NUMBER, 0);
    CHECK(list[0]->number == 1 && list[1]->number == 2 && list[2]->number == 3);
    feed_sort(list, 3, SORT_NUMBER, 1);
    CHECK(list[0]->number == 3);
    feed_sort(list, 3, SORT_NAME, 0);
    CHECK(strcmp(list[0]->id, "g-a") == 0 && strcmp(list[2]->id, "g-c") == 0);
    feed_sort(list, 3, SORT_YEAR, 0);
    CHECK(strcmp(list[0]->id, "g-c") == 0 && strcmp(list[2]->id, "g-a") == 0);  /* no year sorts last */
    feed_sort(list, 3, SORT_STATE, 0);
    CHECK(strcmp(list[0]->id, "g-b") == 0 && strcmp(list[2]->id, "g-c") == 0);
    feed_sort(list, 3, SORT_FIT, 0);
    CHECK(strcmp(list[0]->id, "g-b") == 0 && strcmp(list[1]->id, "g-c") == 0);
    feed_free(f);

    feed_utf8 = 1;
    f = feed_parse(json, (long)strlen(json), &why);
    CHECK(f != NULL);
    if (f) {
        CHECK(strcmp(f->ports[0].title, "Beta \xE2\x80\x93 Caf\xC3\xA9") == 0);
        CHECK(strcmp(f->ports[1].title, "Alpha \xE2\x80\xA6") == 0);
    }
    feed_free(f);
    feed_utf8 = 0;
}

static void test_feed_bad(void)
{
    const char *bad[] = {
        "", "[]", "{\"schema_version\": 2, \"ports\": []}", "{\"ports\": [ {\"title\": \"x\" ",
        "{\"ports\": [ {\"title\": \"unclosed } ] }", "{\"schema_version\": 1, \"ports\": [ {\"title\": \"\\u12\"} ] }",
    };
    int i;
    for (i = 0; i < (int)(sizeof bad / sizeof bad[0]); i++) {
        const char *why = NULL;
        Feed *f = feed_parse(bad[i], (long)strlen(bad[i]), &why);
        CHECK(f == NULL && why != NULL);
        feed_free(f);
    }
}

/* The drawers found on a real lab disk (DH1:), told by their names */
static void test_match(Feed *f)
{
    static const struct { const char *drawer, *id; } v[] = {
        { "Brogue", "g-brogue-ce" }, { "Anarch", "g-anarch" }, { "Xrick", "g-xrick-rick-dangerous-engine" },
        { "REminiscence", "g-reminiscence-flashback" }, { "HoD", "g-heart-of-darkness-hode" },
        { "Angband", "g-angband" }, { "Meritous", "g-meritous" }, { "FishFillets", "g-fish-fillets-ng" },
        { "OpenDUNE", "g-opendune" }, { "BurgerSpace", "g-burgerspace" }, { "Cosmo", "g-cosmo-engine" },
        { "LibreQuake", "g-librequake" }, { "Kobo Deluxe", "g-kobo-deluxe" }, { "Zod", "g-zod-engine-z" },
    };
    static const char *const none[] = { "Test", "PkgTest", "C", "Prefs", "Quake", "Games" };
    Port **list = (Port **)malloc(sizeof(Port *) * (size_t)f->count);
    Matcher *m;
    unsigned i;
    int strength;
    Port *p;
    char key[MATCH_NAME];
    match_key("Fish Fillets_NG!", key, sizeof key);
    CHECK(strcmp(key, "fishfilletsng") == 0);
    if (!list) return;
    for (i = 0; i < (unsigned)f->count; i++) list[i] = &f->ports[i];
    m = matcher_new(list, f->count);
    CHECK(m != NULL);
    for (i = 0; i < sizeof v / sizeof v[0]; i++) {
        p = matcher_find(m, v[i].drawer, &strength);
        if (!p || strcmp(p->id, v[i].id) != 0) printf("  %s: %s\n", v[i].drawer, p ? p->id : "none");
        CHECK(p && strcmp(p->id, v[i].id) == 0);
    }
    for (i = 0; i < sizeof none / sizeof none[0]; i++) {
        p = matcher_find(m, none[i], NULL);
        if (p) printf("  %s: %s\n", none[i], p->id);
        CHECK(p == NULL);   /* "Quake" fits several Quakes: none of them */
    }
    /* the program in a drawer: the drawer's name first, never leftovers */
    p = matcher_find(m, "Brogue", NULL);
    CHECK(match_program(p, "Brogue", "brogue") > match_program(p, "Brogue", "brogue-debug"));
    CHECK(match_program(p, "Brogue", "brogue.old") == 0 && match_program(p, "Brogue", "brogue.info") == 0);
    CHECK(match_program(p, "Brogue", "Brogue.log") == 0 && match_program(p, "Brogue", "rs_old") == 0);
    p = matcher_find(m, "REminiscence", NULL);
    CHECK(match_program(p, "REminiscence", "rs") > match_program(p, "REminiscence", "rs.cfg"));
    p = matcher_find(m, "Angband", NULL);
    CHECK(match_program(p, "Angband", "angband") > match_program(p, "Angband", "angband.new"));
    CHECK(match_program(NULL, "Wolf3D", "wolf3d") > match_program(NULL, "Wolf3D", "wolf3d.fix"));
    {
        char v[24];
        CHECK(match_version("$VER: Brogue CE 1.15.1 Amiga (03.11.2026)", v, sizeof v) && !strcmp(v, "1.15.1"));
        CHECK(match_version("$VER: Anarch 1.1d (03.11.2026) AmiCradle", v, sizeof v) && !strcmp(v, "1.1d"));
        CHECK(match_version("$VER: xrick 021212 (02.11.2026) AmiCradle", v, sizeof v) && !strcmp(v, "021212"));
        CHECK(!match_version("$VER: meritous (03.11.2026)", v, sizeof v));
        CHECK(match_version("$VER: Foo v1.2 (01.01.2026)", v, sizeof v) && !strcmp(v, "1.2"));
        CHECK(!match_version("$VER: Foo version (01.01.2026)", v, sizeof v));
        CHECK(version_cmp("1.9", "1.10") < 0 && version_cmp("1.10", "1.9") > 0 && version_cmp("1.2", "1.2.1") < 0);
        CHECK(version_cmp("1.15.1", "1.15.1") == 0 && version_cmp("unknown", "1.0") == 0 && version_cmp("1.1d", "1.1") == 0);
        CHECK(version_cmp("1.6.0", "1.6.0 RC2") == 0 && version_cmp("2.30", "2.30 test 4") == 0 &&
              version_cmp("1.5", "1.6.0 RC2") < 0 && version_cmp("0.1.0", "0.1.0 alpha 1") == 0);
    }
    matcher_free(m);
    free(list);
}

static void test_feed_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    long len;
    char *buf;
    const char *why = NULL;
    Feed *f;
    int i, with_shot = 0;
    CHECK(fp != NULL);
    if (!fp) return;
    fseek(fp, 0, SEEK_END);
    len = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    buf = (char *)malloc((size_t)len);
    CHECK(buf && fread(buf, 1, (size_t)len, fp) == (size_t)len);
    fclose(fp);
    f = feed_parse(buf, len, &why);
    CHECK(f != NULL);
    if (f) {
        test_match(f);
        for (i = 0; i < f->count; i++) {
            CHECK(f->ports[i].title && f->ports[i].number > 0);
            if (f->ports[i].nshots) with_shot++;
        }
        printf("  %s: %d ports, %d with a picture, generated %s\n", path, f->count, with_shot,
               f->generated ? f->generated : "?");
    } else {
        printf("  why: %s\n", why);
    }
    feed_free(f);
    free(buf);
}

static char *slurp(const char *path, long *len)
{
    FILE *fp = fopen(path, "rb");
    char *b;
    long n;
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END);
    n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    b = (char *)malloc((size_t)n + 1);
    if (b && fread(b, 1, (size_t)n, fp) != (size_t)n) { free(b); b = NULL; }
    if (b) b[n] = 0;
    fclose(fp);
    *len = n;
    return b;
}

static void test_sources(void)
{
    const char *json =
        "{\"schema_version\": 1, \"generated\": \"2026-11-05\", \"sources\": [\n"
        " {\"id\": \"amicradle\", \"name\": \"AmiCradle Port Atlas\", \"index\": \"https://www.amicradle.com/ports/ports.json\",\n"
        "  \"on\": true, \"checked\": true, \"pictures\": true, \"systems\": [\"os3\", \"gfos\"], \"later\": {\"x\": 1}},\n"
        " {\"id\": \"aminet-game\", \"name\": \"Aminet: games\", \"index\": null, \"on\": true},\n"
        " {\"id\": \"Bad Id\", \"index\": \"https://x.org/i.json\"},\n"
        " {\"id\": \"amicradle\", \"index\": \"https://elsewhere.org/i.json\"},\n"
        " {\"id\": \"ftp-one\", \"index\": \"ftp://x.org/i.json\"},\n"
        " {\"id\": \"os4depot\", \"index\": \"https://example.org/os4depot.json\", \"on\": false}\n"
        "]}";
    const char *why = NULL;
    Sources *s = sources_parse(json, (long)strlen(json), &why);
    Source *x;
    char *t, *t2;
    CHECK(s != NULL);
    if (!s) { printf("  why: %s\n", why); return; }
    CHECK(s->count == 3);  /* the bad id, the second amicradle and the ftp index are left out */
    x = sources_find(s, "amicradle");
    CHECK(x && x->on && x->checked && x->pictures && x->nsystems == 2 && x->state == SRC_UNREAD);
    CHECK(x && strcmp(x->index, "https://www.amicradle.com/ports/ports.json") == 0);
    x = sources_find(s, "aminet-game");
    CHECK(x && x->index == NULL && x->state == SRC_NO_INDEX && strcmp(x->name, "Aminet: games") == 0);
    x = sources_find(s, "os4depot");
    CHECK(x && !x->on && strcmp(x->name, "os4depot") == 0);  /* no name: the id */

    CHECK(sources_apply(s, "; mine\n\non os4depot\noff amicradle https://evil.org/i.json\n"
                           "on mygames https://example.org/amiga/index.json Dale's games\n"
                           "on nourl\noff bad! https://x.org/\non spaced https://a.org/i.json   \n") == 4);
    CHECK(sources_find(s, "os4depot")->on);
    x = sources_find(s, "amicradle");
    CHECK(x && !x->on && strcmp(x->index, "https://www.amicradle.com/ports/ports.json") == 0);  /* the Team's index stays */
    x = sources_find(s, "mygames");
    CHECK(x && x->user && x->on && !x->checked && strcmp(x->name, "Dale's games") == 0);
    x = sources_find(s, "spaced");
    CHECK(x && strcmp(x->name, "spaced") == 0 && strcmp(x->index, "https://a.org/i.json") == 0);
    CHECK(!sources_find(s, "nourl"));
    sources_free(s);

    CHECK(source_id_ok("a") && source_id_ok("aminet-game") && !source_id_ok("") && !source_id_ok("Aminet")
          && !source_id_ok("a_b") && !source_id_ok("abcdefghijklmnopqrstuvwxy"));

    t = sources_edit(NULL, "off", "aminet-game", NULL, NULL);
    CHECK(t && strstr(t, "; AmiPortAtlas sources") && strstr(t, "\noff aminet-game\n"));
    t2 = sources_edit(t, "add", "mine", "https://example.org/i.json", "My games");
    free(t);
    CHECK(t2 && strstr(t2, "\non mine https://example.org/i.json My games\n") && strstr(t2, "off aminet-game\n"));
    t = sources_edit(t2, "off", "mine", NULL, NULL);
    free(t2);
    CHECK(t && strstr(t, "\noff mine https://example.org/i.json My games\n") && !strstr(t, "on mine"));
    t2 = sources_edit(t, "on", "aminet-game", NULL, NULL);
    free(t);
    CHECK(t2 && strstr(t2, "\non aminet-game\n") && !strstr(t2, "off aminet-game"));
    t = sources_edit(t2, "drop", "mine", NULL, NULL);
    free(t2);
    CHECK(t && !strstr(t, " mine ") && strstr(t, "on aminet-game"));
    CHECK(sources_edit(t, "add", "x", "notaurl", "n") == NULL);
    CHECK(sources_edit(t, "add", "X", "https://a.org/", "n") == NULL);
    CHECK(sources_edit(t, "maybe", "x", NULL, NULL) == NULL);
    free(t);

    s = sources_builtin();
    CHECK(s && s->count == 1 && strcmp(s->list[0].id, ATLAS_SOURCE) == 0 && s->list[0].on);
    sources_free(s);

    {
        long n;
        char *text = slurp("data/sources.json", &n);
        CHECK(text != NULL);
        if (text) {
            s = sources_parse(text, n, &why);
            CHECK(s && s->count >= 5 && sources_find(s, ATLAS_SOURCE) && sources_find(s, "os4depot")
                  && sources_find(s, "morphos-storage") && sources_find(s, "aros-archives") && sources_find(s, "aminet-game"));
            if (s) printf("  data/sources.json: %d sources\n", s->count);
            sources_free(s);
            free(text);
        }
    }
}

static void test_merge(void)
{
    const char *a = "{\"schema_version\":1,\"generated\":\"2026-11-04\",\"ports\":["
                    "{\"id\":\"g-kobo\",\"number\":2,\"title\":\"Kobo Deluxe\"},{\"id\":\"g-ab\",\"number\":1,\"title\":\"Abuse\"}]}";
    const char *b = "{\"schema_version\":1,\"generated\":\"2026-11-05\",\"name\":\"Aminet: games\",\"ports\":["
                    "{\"id\":\"kobo\",\"title\":\"Kobo Deluxe\",\"kind\":\"game\",\"short\":\"Shooter\",\"author\":\"D. Olofson\","
                    "\"category\":\"game/shoot\",\"install\":\"unpack\",\"source\":\"lies\"},{\"title\":\"No id\"}]}";
    const char *why;
    Feed *fs[3];
    const char *ids[3] = { "aminet-game", NULL, ATLAS_SOURCE };
    Feed *m;
    Port *list[4];
    int i;
    fs[0] = feed_parse(b, (long)strlen(b), &why);
    fs[1] = NULL;
    fs[2] = feed_parse(a, (long)strlen(a), &why);
    CHECK(fs[0] && fs[2]);
    m = feed_merge(fs, ids, 3);
    CHECK(m && m->count == 4 && fs[0] == NULL && fs[2] == NULL);
    if (!m) return;
    CHECK(strcmp(m->generated, "2026-11-04") == 0);   /* the Atlas's */
    CHECK(strcmp(m->ports[0].id, "kobo@aminet-game") == 0 && strcmp(m->ports[0].source, "aminet-game") == 0);
    CHECK(strcmp(m->ports[0].summary, "Shooter") == 0 && strcmp(m->ports[0].author, "D. Olofson") == 0
          && strcmp(m->ports[0].category, "game/shoot") == 0 && strcmp(m->ports[0].kind, "game") == 0);
    CHECK(port_unpack_only(&m->ports[0]) && !port_unpack_only(&m->ports[2]));
    CHECK(m->ports[1].id == NULL && strcmp(m->ports[1].source, "aminet-game") == 0);
    CHECK(strcmp(m->ports[2].id, "g-kobo") == 0 && strcmp(m->ports[2].source, ATLAS_SOURCE) == 0);
    for (i = 0; i < 4; i++) list[i] = &m->ports[i];
    feed_sort(list, 4, SORT_NUMBER, 0);
    CHECK(list[0]->number == 1 && list[1]->number == 2 && list[2]->number == 0);  /* numbered first */
    feed_free(m);
}

static char *sdup(const char *s)
{
    char *d = (char *)malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

static void test_index(void)
{
    const char *readme =
        "Short:        Fish \xE9 puzzles\r\n"
        "Author:       ALTAR team\n"
        "Uploader:     someone\n"
        "Type:         game/think\n"
        "Version:      1.0.2\n"
        "Architecture: m68k-amigaos >= 3.0\n"
        "Requires:     SDL2.module\n"
        "\n"
        "Type: not a field any more\n";
    Readme r;
    char id[40], u[16];
    Feed *f;
    Port p;
    char *text;
    long len;
    const char *why;
    CHECK(readme_parse(readme, (long)strlen(readme), &r));
    CHECK(strcmp(r.shortdesc, "Fish \xC3\xA9 puzzles") == 0);  /* ISO-8859-1 made UTF-8 */
    CHECK(strcmp(r.author, "ALTAR team") == 0 && strcmp(r.type, "game/think") == 0 && strcmp(r.version, "1.0.2") == 0);
    CHECK(strcmp(r.arch, "m68k-amigaos >= 3.0") == 0 && strcmp(r.requires, "SDL2.module") == 0);
    CHECK(strcmp(readme_kind("game/think"), "game") == 0 && strcmp(readme_kind("util/libs"), "library") == 0
          && strcmp(readme_kind("util/arc"), "tool") == 0 && strcmp(readme_kind("mus/play"), "application") == 0);
    CHECK(!readme_parse("Author: x\n", 10, &r));
    latin1_to_utf8("caf\xC3\xA9", u, sizeof u);
    CHECK(strcmp(u, "caf\xC3\xA9") == 0);   /* already UTF-8: kept */

    index_id("Work:Out/Fish-Fillets_1.2.lha", id, sizeof id);
    CHECK(strcmp(id, "fish-fillets-1-2") == 0);
    index_id("Game.LZX", id, sizeof id);
    CHECK(strcmp(id, "game") == 0);

    feed_utf8 = 1;
    {
        const char *old = "{\"schema_version\":1,\"generated\":\"x\",\"ports\":[{\"id\":\"a\",\"number\":7,\"title\":\"Old A\"}]}";
        f = feed_parse(old, (long)strlen(old), &why);
    }
    CHECK(f != NULL);
    if (!f) { feed_utf8 = 0; return; }
    memset(&p, 0, sizeof p);
    p.id = sdup("a"); p.title = sdup("New \"A\"\tcaf\xC3\xA9"); p.version = sdup("2");
    p.download_url = sdup("https://example.org/a.lha"); p.download_size = 1234;
    p.download_sha256 = sdup("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    p.drawer = sdup("A"); p.program = sdup("a"); p.install = sdup("unpack"); p.fpu = 1;
    p.shots = (char **)malloc(sizeof(char *)); p.shots[0] = sdup("https://example.org/a.png"); p.nshots = 1;
    CHECK(index_put(f, &p) && f->count == 1 && f->ports[0].number == 7 && p.id == NULL);
    memset(&p, 0, sizeof p);
    p.id = sdup("b"); p.title = sdup("B");
    CHECK(index_put(f, &p) && f->count == 2);
    text = feed_write(f, "Dale's games", "2026-11-05", &len);
    feed_free(f);
    CHECK(text != NULL);
    if (text) {
        Feed *g = feed_parse(text, len, &why);
        CHECK(g && g->count == 2 && strcmp(g->generated, "2026-11-05") == 0);
        if (g) {
            CHECK(strcmp(g->ports[0].title, "New \"A\"\tcaf\xC3\xA9") == 0 && g->ports[0].number == 7);
            CHECK(port_installable(&g->ports[0]) && port_unpack_only(&g->ports[0]) && g->ports[0].fpu);
            CHECK(g->ports[0].nshots == 1 && g->ports[0].download_size == 1234);
            CHECK(strcmp(g->ports[1].id, "b") == 0 && !port_installable(&g->ports[1]));
        } else printf("  why: %s\n%s\n", why, text);
        feed_free(g);
        free(text);
    }
    feed_utf8 = 0;
}

int main(int argc, char **argv)
{
    test_sha256();
    test_feed_small();
    test_feed_bad();
    test_sources();
    test_merge();
    test_index();
    if (argc > 1) test_feed_file(argv[1]);
    printf(failures ? "%d failed\n" : "all passed\n", failures);
    return failures != 0;
}
