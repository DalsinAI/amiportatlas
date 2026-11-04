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
        "   { \"id\": \"g-c\", \"origin\": \"original\", \"number\": 3, \"title\": \"gamma\", \"year\": \"1987\", \"state\": \"not_started\", \"fit\": \"060\" },\n"
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
        CHECK(version_cmp("1.9", "1.10") < 0 && version_cmp("1.10", "1.9") > 0 && version_cmp("1.2", "1.2.1") < 0);
        CHECK(version_cmp("1.15.1", "1.15.1") == 0 && version_cmp("unknown", "1.0") == 0 && version_cmp("1.1d", "1.1") == 0);
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

int main(int argc, char **argv)
{
    test_sha256();
    test_feed_small();
    test_feed_bad();
    if (argc > 1) test_feed_file(argv[1]);
    printf(failures ? "%d failed\n" : "all passed\n", failures);
    return failures != 0;
}
