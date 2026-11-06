/* AmiPortAtlas: the master list of sources and the player's own lines.
 * MIT, (c) 2026 The AmiCradle Team.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "sources.h"
#include "json.h"

#define KEY(name) (strcmp(key, name) == 0)

static char *dup(const char *s)
{
    char *d;
    if (!s) return NULL;
    d = (char *)malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

int source_id_ok(const char *id)
{
    int n = 0;
    if (!id) return 0;
    for (; *id; id++, n++)
        if (!((*id >= 'a' && *id <= 'z') || (*id >= '0' && *id <= '9') || *id == '-')) return 0;
    return n >= 1 && n <= 24;
}

/* An index's address: https://, or http:// (some Amiga sites have no TLS),
 * with no spaces or quotes, which go into command lines and the file. */
static int url_ok(const char *u)
{
    if (!u) return 0;
    if (strncmp(u, "https://", 8) != 0 && strncmp(u, "http://", 7) != 0) return 0;
    if (strlen(u) > 400) return 0;
    for (; *u; u++) if ((unsigned char)*u <= ' ' || *u == '"') return 0;
    return 1;
}

static void source_free(Source *s)
{
    free(s->id); free(s->name); free(s->index); free(s->home); free(s->note);
    json_free_list(s->systems, s->nsystems);
}

void sources_free(Sources *s)
{
    int i;
    if (!s) return;
    for (i = 0; i < s->count; i++) source_free(&s->list[i]);
    free(s->list);
    free(s->generated);
    free(s);
}

Source *sources_find(Sources *s, const char *id)
{
    int i;
    if (!s || !id) return NULL;
    for (i = 0; i < s->count; i++) if (strcmp(s->list[i].id, id) == 0) return &s->list[i];
    return NULL;
}

static Source *grow(Sources *s)
{
    Source *n = (Source *)realloc(s->list, (size_t)(s->count + 1) * sizeof(Source));
    if (!n) return NULL;
    s->list = n;
    memset(&n[s->count], 0, sizeof(Source));
    return &n[s->count];
}

static int one(JsonReader *r, Source *x)
{
    memset(x, 0, sizeof *x);
    x->on = 1;
    if (!json_take(r, '{')) return json_fail(r, "a source should be an object");
    if (json_take(r, '}')) return 1;
    for (;;) {
        char *key;
        int ok;
        if (!json_string(r, &key, 0)) return 0;
        if (!json_take(r, ':')) { free(key); return json_fail(r, "a ':' was expected"); }
        if      (KEY("id"))       ok = json_string(r, &x->id, 1);
        else if (KEY("name"))     ok = json_string(r, &x->name, 1);
        else if (KEY("index"))    ok = json_string(r, &x->index, 1);
        else if (KEY("home"))     ok = json_string(r, &x->home, 1);
        else if (KEY("note"))     ok = json_string(r, &x->note, 1);
        else if (KEY("on"))       ok = json_boolean(r, &x->on);
        else if (KEY("checked"))  ok = json_boolean(r, &x->checked);
        else if (KEY("pictures")) ok = json_boolean(r, &x->pictures);
        else if (KEY("systems"))  ok = json_strings(r, &x->systems, &x->nsystems);
        else ok = json_skip(r, 2);
        free(key);
        if (!ok) return 0;
        if (json_take(r, ',')) continue;
        if (json_take(r, '}')) return 1;
        return json_fail(r, "a ',' or '}' was expected in a source");
    }
}

static int list(JsonReader *r, Sources *s)
{
    if (!json_take(r, '[')) return json_fail(r, "\"sources\" should be a list");
    if (json_take(r, ']')) return 1;
    for (;;) {
        Source *x = grow(s);
        if (!x) return json_fail(r, "out of memory");
        if (!one(r, x)) { source_free(x); return 0; }
        /* a bad id, a second of the same id or a bad index is left out */
        if (!source_id_ok(x->id) || sources_find(s, x->id) || (x->index && !url_ok(x->index))) source_free(x);
        else {
            if (!x->name) x->name = dup(x->id);
            if (!x->name) { source_free(x); return json_fail(r, "out of memory"); }
            if (x->index) x->state = SRC_UNREAD; else x->state = SRC_NO_INDEX;
            s->count++;
        }
        if (json_take(r, ',')) continue;
        if (json_take(r, ']')) return 1;
        return json_fail(r, "a ',' or ']' was expected in the sources");
    }
}

Sources *sources_parse(const char *text, long len, const char **why)
{
    JsonReader r;
    Sources *s = (Sources *)calloc(1, sizeof(Sources));
    int ok = 1;
    if (!s) { if (why) *why = "out of memory"; return NULL; }
    json_begin(&r, text, len);
    if (!json_take(&r, '{')) ok = json_fail(&r, "this is not a list of sources");
    else if (!json_take(&r, '}')) {
        for (;;) {
            char *key;
            if (!json_string(&r, &key, 0)) { ok = 0; break; }
            if (!json_take(&r, ':')) { free(key); ok = json_fail(&r, "a ':' was expected"); break; }
            if (strcmp(key, "schema_version") == 0) ok = json_number(&r, &s->schema);
            else if (strcmp(key, "generated") == 0) ok = json_string(&r, &s->generated, 1);
            else if (strcmp(key, "sources") == 0) ok = list(&r, s);
            else ok = json_skip(&r, 1);
            free(key);
            if (!ok) break;
            if (json_take(&r, ',')) continue;
            if (json_take(&r, '}')) break;
            ok = json_fail(&r, "a ',' or '}' was expected");
            break;
        }
    }
    if (ok && s->schema != SOURCES_SCHEMA)
        ok = json_fail(&r, "the list of sources is a newer kind than this AmiPortAtlas reads: please update AmiPortAtlas");
    if (ok && s->count == 0) ok = json_fail(&r, "the list of sources is empty");
    if (!ok) {
        if (why) *why = r.why ? r.why : "the list of sources could not be read";
        sources_free(s);
        return NULL;
    }
    return s;
}

Sources *sources_builtin(void)
{
    static const char text[] =
        "{\"schema_version\":1,\"sources\":[{\"id\":\"amicradle\",\"name\":\"AmiCradle Port Atlas\","
        "\"index\":\"https://www.amicradle.com/ports/ports.json\",\"home\":\"https://www.amicradle.com/ports/\","
        "\"on\":true,\"checked\":true,\"pictures\":true}]}";
    return sources_parse(text, (long)sizeof text - 1, NULL);
}

/* The next word of a line: *p moves past it; out is 0-terminated. */
static int word(const char **p, char *out, int max)
{
    int n = 0;
    while (**p == ' ' || **p == '\t') (*p)++;
    while (**p && **p != ' ' && **p != '\t' && **p != '\n' && **p != '\r') {
        if (n < max - 1) out[n++] = **p;
        (*p)++;
    }
    out[n] = 0;
    return n > 0;
}

static void rest(const char *p, char *out, int max)
{
    int n = 0;
    while (*p == ' ' || *p == '\t') p++;
    while (*p && *p != '\n' && *p != '\r' && n < max - 1) out[n++] = *p++;
    while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\t')) n--;
    out[n] = 0;
}

int sources_apply(Sources *s, const char *t)
{
    int used = 0;
    if (!s || !t) return 0;
    while (*t) {
        const char *p = t, *nl = strchr(t, '\n');
        char verb[8], id[32], index[420], name[120];
        t = nl ? nl + 1 : t + strlen(t);
        if (!word(&p, verb, sizeof verb) || verb[0] == ';' || verb[0] == '#') continue;
        if (strcmp(verb, "on") != 0 && strcmp(verb, "off") != 0) continue;
        if (!word(&p, id, sizeof id) || !source_id_ok(id)) continue;
        word(&p, index, sizeof index);
        rest(p, name, sizeof name);
        {
            Source *x = sources_find(s, id);
            if (x) {
                /* a master source only switches: its index stays the Team's */
                x->on = verb[1] == 'n';
                used++;
                continue;
            }
            if (!url_ok(index)) continue;
            x = grow(s);
            if (!x) return used;
            x->id = dup(id);
            x->name = dup(name[0] ? name : id);
            x->index = dup(index);
            if (!x->id || !x->name || !x->index) { source_free(x); return used; }
            x->on = verb[1] == 'n';
            x->user = 1;
            x->state = SRC_UNREAD;
            s->count++;
            used++;
        }
    }
    return used;
}

char *sources_edit(const char *t, const char *verb, const char *id, const char *index, const char *name)
{
    size_t room;
    char *out, *o, line[600], keep_index[420] = "", keep_name[120] = "";
    int add = strcmp(verb, "add") == 0, drop = strcmp(verb, "drop") == 0;
    int on = strcmp(verb, "on") == 0;
    if (!source_id_ok(id)) return NULL;
    if (!add && !drop && !on && strcmp(verb, "off") != 0) return NULL;
    if (add && !url_ok(index)) return NULL;
    if (!t) t = "";
    room = strlen(t) + 700;
    out = o = (char *)malloc(room);
    if (!out) return NULL;
    *o = 0;
    /* every line but id's is kept as it was */
    while (*t) {
        const char *p = t, *nl = strchr(t, '\n');
        size_t n = nl ? (size_t)(nl - t) + 1 : strlen(t);
        char v[8], w[32];
        t += n;
        if (word(&p, v, sizeof v) && (strcmp(v, "on") == 0 || strcmp(v, "off") == 0)
            && word(&p, w, sizeof w) && strcmp(w, id) == 0) {
            word(&p, keep_index, sizeof keep_index);
            rest(p, keep_name, sizeof keep_name);
            continue;
        }
        memcpy(o, t - n, n);
        o += n;
        if (n && o[-1] != '\n') *o++ = '\n';
        *o = 0;
    }
    if (o == out) o += sprintf(o, "; AmiPortAtlas sources: on|off ID [INDEX-URL [NAME]]\n");
    if (drop) return out;
    if (add) { snprintf(keep_index, sizeof keep_index, "%s", index); snprintf(keep_name, sizeof keep_name, "%s", name ? name : ""); }
    if (keep_index[0]) snprintf(line, sizeof line, "%s %s %s%s%s\n", add || on ? "on" : "off", id, keep_index,
                                keep_name[0] ? " " : "", keep_name);
    else snprintf(line, sizeof line, "%s %s\n", on ? "on" : "off", id);
    strcpy(o, line);
    return out;
}
