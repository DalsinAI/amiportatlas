/* AmiPortAtlas: reads ports.json.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * Every source's index has this one form (docs/SOURCES.md), so one reader
 * serves them all. Unknown keys are skipped, so newer feeds keep working
 * with older copies of the program.
 */
#include <stdlib.h>
#include <string.h>
#include "feed.h"
#include "json.h"
#include "sha256.h"

#define KEY(name) (strcmp(key, name) == 0)

/* true or false, or a string saying which (the Atlas writes "68881 (the
 * 68040 FPU)" for a game that needs one): any string but "" is true. */
static int flag(JsonReader *r, int *out)
{
    char *s;
    while (r->p < r->end && (*r->p == ' ' || *r->p == '\t' || *r->p == '\n' || *r->p == '\r')) r->p++;
    if (r->p < r->end && *r->p == '"') {
        if (!json_string(r, &s, 0)) return 0;
        *out = s[0] != 0;
        free(s);
        return 1;
    }
    return json_boolean(r, out);
}

static int port(JsonReader *r, Port *p)
{
    memset(p, 0, sizeof *p);
    if (!json_take(r, '{')) return json_fail(r, "a port should be an object");
    if (json_take(r, '}')) return 1;
    for (;;) {
        char *key;
        int ok;
        if (!json_string(r, &key, 0)) return 0;
        if (!json_take(r, ':')) { free(key); return json_fail(r, "a ':' was expected"); }
        if      (KEY("number"))          ok = json_number(r, &p->number);
        else if (KEY("id"))              ok = json_string(r, &p->id, 1);
        else if (KEY("origin"))          ok = json_string(r, &p->origin, 1);
        else if (KEY("title"))           ok = json_string(r, &p->title, 1);
        else if (KEY("year"))            ok = json_string(r, &p->year, 1);
        else if (KEY("display"))         ok = json_string(r, &p->display, 1);
        else if (KEY("state"))           ok = json_string(r, &p->state, 1);
        else if (KEY("fit"))             ok = json_string(r, &p->fit, 1);
        else if (KEY("game_data"))       ok = json_string(r, &p->game_data, 1);
        else if (KEY("version"))         ok = json_string(r, &p->version, 1);
        else if (KEY("release_url"))     ok = json_string(r, &p->release_url, 1);
        else if (KEY("port_repo"))       ok = json_string(r, &p->port_repo, 1);
        else if (KEY("download_url"))    ok = json_string(r, &p->download_url, 1);
        else if (KEY("download_size"))   ok = json_number(r, &p->download_size);
        else if (KEY("download_sha256")) ok = json_string(r, &p->download_sha256, 1);
        else if (KEY("drawer"))          ok = json_string(r, &p->drawer, 1);
        else if (KEY("program"))         ok = json_string(r, &p->program, 1);
        else if (KEY("cpu"))             ok = json_string(r, &p->cpu, 1);
        else if (KEY("fpu"))             ok = flag(r, &p->fpu);
        else if (KEY("space"))           ok = json_number(r, &p->space);
        else if (KEY("requires"))        ok = json_strings(r, &p->requires, &p->nrequires);
        else if (KEY("modules"))         ok = json_strings(r, &p->modules, &p->nmodules);
        else if (KEY("source"))          ok = json_skip(r, 2);   /* the merge says where it came from */
        else if (KEY("kind"))            ok = json_string(r, &p->kind, 1);
        else if (KEY("short"))           ok = json_string(r, &p->summary, 1);
        else if (KEY("author"))          ok = json_string(r, &p->author, 1);
        else if (KEY("category"))        ok = json_string(r, &p->category, 1);
        else if (KEY("install"))         ok = json_string(r, &p->install, 1);
        else if (KEY("screenshots")) {
            char **l; int n;
            ok = json_strings(r, &l, &n);
            if (ok && n > 0) { json_free_list(p->shots, p->nshots); p->shots = l; p->nshots = n; }
            else if (ok) json_free_list(l, n);
        }
        else if (KEY("screenshot") && p->nshots == 0) {
            /* Older feeds: one picture. "screenshots", when present, wins. */
            char *s;
            ok = json_string(r, &s, 1);
            if (ok && s) {
                p->shots = (char **)malloc(sizeof(char *));
                if (p->shots) { p->shots[0] = s; p->nshots = 1; } else free(s);
            }
        }
        else ok = json_skip(r, 2);
        free(key);
        if (!ok) return 0;
        if (json_take(r, ',')) continue;
        if (json_take(r, '}')) return 1;
        return json_fail(r, "a ',' or '}' was expected in a port");
    }
}


void port_free(Port *p)
{
    free(p->id); free(p->origin); free(p->title); free(p->year); free(p->display); free(p->state);
    free(p->fit); free(p->game_data); free(p->version); free(p->release_url);
    free(p->port_repo); free(p->download_url); free(p->download_sha256);
    free(p->drawer); free(p->program); free(p->cpu);
    free(p->source); free(p->kind); free(p->summary); free(p->author); free(p->category); free(p->install);
    json_free_list(p->requires, p->nrequires);
    json_free_list(p->modules, p->nmodules);
    json_free_list(p->shots, p->nshots);
    memset(p, 0, sizeof *p);
}

void feed_free(Feed *f)
{
    int i;
    if (!f) return;
    for (i = 0; i < f->count; i++) port_free(&f->ports[i]);
    free(f->ports);
    free(f->generated);
    free(f);
}

static int ports(JsonReader *r, Feed *f)
{
    int cap = 0;
    if (!json_take(r, '[')) return json_fail(r, "\"ports\" should be a list");
    if (json_take(r, ']')) return 1;
    for (;;) {
        if (f->count == cap) {
            Port *n;
            cap = cap ? cap * 2 : 64;
            n = (Port *)realloc(f->ports, (size_t)cap * sizeof(Port));
            if (!n) return json_fail(r, "out of memory");
            f->ports = n;
        }
        if (!port(r, &f->ports[f->count])) {
            port_free(&f->ports[f->count]);
            return 0;
        }
        if (!f->ports[f->count].title) {
            port_free(&f->ports[f->count]);  /* a port with no name is left out */
        } else {
            f->count++;
        }
        if (json_take(r, ',')) continue;
        if (json_take(r, ']')) return 1;
        return json_fail(r, "a ',' or ']' was expected in the ports list");
    }
}

Feed *feed_parse(const char *text, long len, const char **why)
{
    JsonReader r;
    Feed *f = (Feed *)calloc(1, sizeof(Feed));
    int ok = 1;
    if (!f) { if (why) *why = "out of memory"; return NULL; }
    json_begin(&r, text, len);
    if (!json_take(&r, '{')) ok = json_fail(&r, "this is not a Port Atlas feed");
    else if (!json_take(&r, '}')) {
        for (;;) {
            char *key;
            if (!json_string(&r, &key, 0)) { ok = 0; break; }
            if (!json_take(&r, ':')) { free(key); ok = json_fail(&r, "a ':' was expected"); break; }
            if (strcmp(key, "schema_version") == 0) ok = json_number(&r, &f->schema);
            else if (strcmp(key, "generated") == 0) ok = json_string(&r, &f->generated, 1);
            else if (strcmp(key, "ports") == 0) ok = ports(&r, f);
            else ok = json_skip(&r, 1);
            free(key);
            if (!ok) break;
            if (json_take(&r, ',')) continue;
            if (json_take(&r, '}')) break;
            ok = json_fail(&r, "a ',' or '}' was expected");
            break;
        }
    }
    if (ok && f->schema != FEED_SCHEMA) ok = json_fail(&r, "the feed is a newer kind than this AmiPortAtlas reads: please update AmiPortAtlas");
    if (!ok) {
        if (why) *why = r.why ? r.why : "the feed could not be read";
        feed_free(f);
        return NULL;
    }
    return f;
}

static char *dup2(const char *a, const char *b)
{
    size_t n = strlen(a), m = b ? strlen(b) : 0;
    char *d = (char *)malloc(n + m + 2);
    if (!d) return NULL;
    memcpy(d, a, n);
    if (b) { d[n] = '@'; memcpy(d + n + 1, b, m); d[n + 1 + m] = 0; }
    else d[n] = 0;
    return d;
}

Feed *feed_merge(Feed **feeds, const char *const *source_ids, int n)
{
    Feed *m = (Feed *)calloc(1, sizeof(Feed));
    long total = 0;
    int i, j, ok = m != NULL;
    for (i = 0; i < n; i++) if (feeds[i]) total += feeds[i]->count;
    if (ok) {
        m->schema = FEED_SCHEMA;
        m->ports = (Port *)malloc((size_t)(total ? total : 1) * sizeof(Port));
        ok = m->ports != NULL;
    }
    for (i = 0; i < n; i++) {
        Feed *f = feeds[i];
        int atlas = source_ids[i] && strcmp(source_ids[i], ATLAS_SOURCE) == 0;
        if (!f) continue;
        if (ok && f->generated && (!m->generated || atlas)) {
            free(m->generated);
            m->generated = f->generated;
            f->generated = NULL;
        }
        for (j = 0; ok && j < f->count; j++) {
            Port *p = &m->ports[m->count];
            *p = f->ports[j];
            memset(&f->ports[j], 0, sizeof(Port));
            p->source = dup2(source_ids[i] ? source_ids[i] : "", NULL);
            if (p->id && !atlas) {
                char *id = dup2(p->id, source_ids[i] ? source_ids[i] : "");
                free(p->id);
                p->id = id;
            }
            m->count++;
            if (!p->source) ok = 0;
        }
        feed_free(f);
        feeds[i] = NULL;
    }
    if (!ok) { feed_free(m); return NULL; }
    return m;
}

int port_unpack_only(const Port *p)
{
    return p->install && strcmp(p->install, "unpack") == 0;
}

/* A name made into a path under T: or the drawer picked: one plain name, never
 * "" (the parent itself, which Remove would delete) nor one with ':' or '/'. */
int port_plain_name(const char *s)
{
    if (!s || !*s || strlen(s) > 100) return 0;
    for (; *s; s++) if (*s == ':' || *s == '/' || *s == '"' || (unsigned char)*s < ' ') return 0;
    return 1;
}

void port_picture_name(const char *url, char *out, int max)
{
    static const char digits[] = "0123456789abcdef";
    unsigned char d[32];
    char ext[8];
    const char *base, *dot = NULL, *q;
    int i, n = 0;
    Sha256 s;
    sha256_init(&s);
    sha256_update(&s, url, (unsigned long)strlen(url));
    sha256_final(&s, d);
    /* the extension of the URL's last name, before any ?query or #part */
    base = strrchr(url, '/');
    base = base ? base + 1 : url;
    for (q = base; *q && *q != '?' && *q != '#'; q++) if (*q == '.') dot = q;
    ext[0] = 0;
    if (dot) {
        for (q = dot + 1; *q && *q != '?' && *q != '#'; q++) {
            char c = *q;
            if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
            if (n >= 4 || !((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))) { n = -1; break; }
            ext[1 + n++] = c;
        }
        if (n > 0) { ext[0] = '.'; ext[1 + n] = 0; } else ext[0] = 0;
    }
    if (max < 1) return;
    for (i = 0; i < 8 && 2 * i + 1 < max - 1; i++) {
        out[2 * i] = digits[d[i] >> 4];
        out[2 * i + 1] = digits[d[i] & 15];
    }
    out[2 * i] = 0;
    if ((int)(strlen(out) + strlen(ext)) < max) strcat(out, ext);
}

int port_installable(const Port *p)
{
    return p->download_url && p->download_url[0] && p->download_sha256 && strlen(p->download_sha256) == 64
        && port_plain_name(p->id) && port_plain_name(p->drawer) && port_plain_name(p->program);
}

int port_is_original(const Port *p)
{
    return p->origin && strcmp(p->origin, "original") == 0;
}

const char *port_state_text(const Port *p)
{
    const char *s = p->state ? p->state : "";
    if (strcmp(s, "released") == 0) return "Released";
    if (strcmp(s, "testing") == 0) return "Testing";
    if (strcmp(s, "in_progress") == 0) return "In progress";
    return "Not started";
}

static int state_rank(const Port *p)
{
    const char *s = p->state ? p->state : "";
    if (strcmp(s, "released") == 0) return 0;
    if (strcmp(s, "testing") == 0) return 1;
    if (strcmp(s, "in_progress") == 0) return 2;
    return 3;
}

static int fit_rank(const Port *p)
{
    const char *s = p->fit ? p->fit : "";
    if (strcmp(s, "040") == 0) return 0;
    if (strcmp(s, "060") == 0) return 1;
    if (strcmp(s, "GPU") == 0) return 2;
    return 3;
}

static long year_of(const Port *p)
{
    const char *s = p->year;
    long y = 0;
    int i;
    if (!s) return 9999;
    for (i = 0; i < 4; i++) {
        if (s[i] < '0' || s[i] > '9') return 9999;
        y = y * 10 + (s[i] - '0');
    }
    return y;
}

static int lower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

static int name_cmp(const char *a, const char *b)
{
    while (*a && lower((unsigned char)*a) == lower((unsigned char)*b)) { a++; b++; }
    return lower((unsigned char)*a) - lower((unsigned char)*b);
}

static int sort_key, sort_desc;

static int cmp(const void *x, const void *y)
{
    const Port *a = *(const Port * const *)x, *b = *(const Port * const *)y;
    long d = 0;
    switch (sort_key) {
    case SORT_NAME:  d = 0; break;
    case SORT_YEAR:  d = year_of(a) - year_of(b); break;
    case SORT_STATE: d = state_rank(a) - state_rank(b); break;
    case SORT_FIT:   d = fit_rank(a) - fit_rank(b); break;
    default:
        /* entries from sources without numbers come after the Atlas's */
        if ((a->number > 0) != (b->number > 0)) d = a->number > 0 ? -1 : 1;
        else d = a->number - b->number;
        break;
    }
    if (d == 0 && sort_key != SORT_NUMBER) d = name_cmp(a->title, b->title);
    if (d == 0) d = a->number - b->number;
    if (sort_desc) d = -d;
    return d < 0 ? -1 : d > 0 ? 1 : 0;
}

void feed_sort(Port **list, int n, int key, int descending)
{
    sort_key = key;
    sort_desc = descending;
    qsort(list, (size_t)n, sizeof(Port *), cmp);
}
