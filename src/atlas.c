/* Atlas: AmiPortAtlas from the Shell, as APT is on Linux.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 *   Atlas UPDATE                     read every source again
 *   Atlas LIST [ALL|INSTALLED|UPDATES] [SOURCE=id]
 *   Atlas SEARCH words...            by name, line and id
 *   Atlas SHOW name                  what the sources say about it
 *   Atlas INSTALL names... [TO=drawer] [YES]
 *   Atlas UPGRADE [names...] [YES]   every update, or those named
 *   Atlas REMOVE names... [YES]
 *   Atlas SOURCES                    the sources, on and off
 *   Atlas SOURCE ON|OFF|DROP id      switch or take away a source
 *   Atlas SOURCE ADD id URL=index [NAME=name]
 *   Atlas PUBLISH archive.lha TO=id [UPLOAD=ftp(s)://user@host/drawer/] [README=..] [TITLE=..]
 *         [DRAWER=..] [PROGRAM=..] [VERSION=..] [KIND=..] [CPU=..] [FPU] [PICTURES=a.png,b.png] [INSTALLER]
 *
 * It reads the same sources, saved copies and installed list as the window
 * (repo.c, install.c), so the two always agree. docs/SOURCES.md has more.
 */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/var.h>
#include <dos/rdargs.h>

#include "feed.h"
#include "sources.h"
#include "repo.h"
#include "install.h"
#include "match.h"
#include "http.h"
#include "ftp.h"
#include "index.h"
#include "sha256.h"

const char atlas_version[] __attribute__((used)) = "$VER: Atlas 0.2 (05.11.2026) The AmiCradle Team";
#if !defined(__amigaos4__) && !defined(__AROS__)
unsigned long __stack = 65536;   /* libnix swaps to this from the Shell's 4 KB */
#endif

#define TEMPLATE "COMMAND/A,NAMES/M,TO/K,SOURCE/K,URL/K,NAME/K,UPLOAD/K,README/K,TITLE/K,DRAWER/K," \
                 "PROGRAM/K,VERSION/K,KIND/K,CPU/K,PICTURES/K,FPU/S,INSTALLER/S,YES/S"
enum { A_COMMAND, A_NAMES, A_TO, A_SOURCE, A_URL, A_NAME, A_UPLOAD, A_README, A_TITLE, A_DRAWER,
       A_PROGRAM, A_VERSION, A_KIND, A_CPU, A_PICTURES, A_FPU, A_INSTALLER, A_YES, A_COUNT };

#define PUBLISH_VAR "AmiPortAtlas/Publish"   /* "id upload-address" lines */

static LONG args[A_COUNT];
static Feed *F;
static Sources *S;

static const char *arg(int i) { return (const char *)args[i]; }
static char **names(void) { return (char **)args[A_NAMES]; }
static int nnames(void) { int n = 0; char **l = names(); while (l && l[n]) n++; return n; }

static int is(const char *a, const char *b)
{
    while (*a && *b) {
        char x = *a++, y = *b++;
        if (x >= 'a' && x <= 'z') x -= 32;
        if (y >= 'a' && y <= 'z') y -= 32;
        if (x != y) return 0;
    }
    return *a == *b;
}

static int contains(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
    if (!hay) return 0;
    for (; *hay; hay++) {
        size_t i;
        for (i = 0; i < n; i++) {
            char a = hay[i], b = needle[i];
            if (!a) return 0;
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) break;
        }
        if (i == n) return 1;
    }
    return n == 0;
}

static int stopped(void) { return (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0; }

static int yes(const char *question)
{
    char line[16];
    if (args[A_YES]) return 1;
    printf("%s [y/N] ", question);
    fflush(stdout);
    if (!fgets(line, sizeof line, stdin)) return 0;
    return line[0] == 'y' || line[0] == 'Y';
}

/* ---- reading ---- */

static void tell(void *user, const char *line) { (void)user; printf("%s\n", line); }

/* The saved copies (with fetch, the network first). */
static int load(int fetch)
{
    char why[200];
    F = repo_read(fetch, &S, fetch ? tell : NULL, NULL, why, (int)sizeof why);
    if (!F) { printf("Atlas: %s\n", why); return 0; }
    installed_load();
    return 1;
}

static int updatable(const Port *p)
{
    const char *v = installed_version(p);
    return v && p->version && port_installable(p) && version_cmp(v, p->version) < 0;
}

/* A name as the player typed it: an id ("g-kobo", "kobo@aminet-game"), a
 * title, or a unique part of one. The Atlas's entry wins a tie on the title. */
static Port *find(const char *name, int quiet)
{
    Port *hit = NULL;
    int i, n = 0;
    for (i = 0; i < F->count; i++) if (F->ports[i].id && is(F->ports[i].id, name)) return &F->ports[i];
    for (i = 0; i < F->count; i++) {
        Port *p = &F->ports[i];
        const char *at = p->id ? strchr(p->id, '@') : NULL;
        if (at && (size_t)(at - p->id) == strlen(name) && strncmp(p->id, name, strlen(name)) == 0) { hit = p; n++; }
    }
    if (n == 1) return hit;
    n = 0;
    for (i = 0; i < F->count; i++)
        if (is(F->ports[i].title, name)) {
            if (!hit || n == 0 || strcmp(F->ports[i].source, ATLAS_SOURCE) == 0) hit = &F->ports[i];
            n++;
        }
    if (n >= 1) return hit;
    for (i = 0; i < F->count; i++) if (contains(F->ports[i].title, name)) { hit = &F->ports[i]; n++; }
    if (n == 1) return hit;
    if (!quiet) {
        if (n == 0) printf("Atlas: nothing is called %s. Atlas SEARCH finds things by part of a name.\n", name);
        else {
            printf("Atlas: %d things are called %s; name one by its id:\n", n, name);
            for (i = 0; i < F->count; i++)
                if (contains(F->ports[i].title, name)) printf("  %-28s %s\n", F->ports[i].id ? F->ports[i].id : "?", F->ports[i].title);
        }
    }
    return NULL;
}

static void row(const Port *p)
{
    const char *v = installed_version(p);
    printf("%-30.30s %-26.26s %-10.10s %s%s\n", p->id ? p->id : "?", p->title,
           p->version ? p->version : "-", updatable(p) ? "update from " : v ? "installed " : "",
           updatable(p) || v ? v : (port_installable(p) ? "" : port_state_text(p)));
}

/* ---- the commands ---- */

static int cmd_update(void)
{
    char line[400];
    int i, updates = 0;
    if (!load(1)) return RETURN_ERROR;
    for (i = 0; i < S->count; i++) {
        const Source *x = &S->list[i];
        if (!x->on) continue;
        if (x->state == SRC_NO_INDEX) printf("  %-24s no index yet\n", x->name);
        else if (x->state == SRC_FAILED) printf("  %-24s not read: %s\n", x->name, x->why);
        else if (x->state == SRC_CACHED) printf("  %-24s %d (the saved copy: %s)\n", x->name, x->count, x->why);
        else printf("  %-24s %d\n", x->name, x->count);
    }
    for (i = 0; i < F->count; i++) if (updatable(&F->ports[i])) updates++;
    repo_summary(S, line, (int)sizeof line);
    printf("%d entries. %d update%s", F->count, updates, updates == 1 ? "" : "s");
    printf(updates ? ": Atlas UPGRADE installs %s.\n" : ".\n", updates == 1 ? "it" : "them");
    return RETURN_OK;
}

static int cmd_list(void)
{
    const char *what = nnames() ? names()[0] : "AVAILABLE";
    const char *src = arg(A_SOURCE);
    int i, n = 0, mode;
    if (is(what, "ALL")) mode = 0;
    else if (is(what, "INSTALLED")) mode = 2;
    else if (is(what, "UPDATES") || is(what, "UPGRADABLE")) mode = 3;
    else if (is(what, "AVAILABLE")) mode = 1;
    else { printf("Atlas LIST takes ALL, AVAILABLE, INSTALLED or UPDATES.\n"); return RETURN_ERROR; }
    if (!load(0)) return RETURN_ERROR;
    for (i = 0; i < F->count; i++) {
        const Port *p = &F->ports[i];
        if (src && !(p->source && strcmp(p->source, src) == 0)) continue;
        if (mode == 1 && !port_installable(p)) continue;
        if (mode == 2 && !installed_version(p)) continue;
        if (mode == 3 && !updatable(p)) continue;
        row(p);
        n++;
        if (stopped()) { printf("***Break\n"); return RETURN_WARN; }
    }
    printf("%d shown.\n", n);
    return RETURN_OK;
}

static int cmd_search(void)
{
    char **w = names();
    int i, j, n = 0;
    if (!nnames()) { printf("Atlas SEARCH needs a word to look for.\n"); return RETURN_ERROR; }
    if (!load(0)) return RETURN_ERROR;
    for (i = 0; i < F->count; i++) {
        const Port *p = &F->ports[i];
        for (j = 0; w[j]; j++)
            if (!contains(p->title, w[j]) && !contains(p->summary, w[j]) && !contains(p->id, w[j])) break;
        if (w[j]) continue;
        row(p);
        if (p->summary) printf("    %s\n", p->summary);
        n++;
    }
    printf("%d found.\n", n);
    return n ? RETURN_OK : RETURN_WARN;
}

static void line(const char *k, const char *v) { if (v && v[0]) printf("%-11s %s\n", k, v); }

static int cmd_show(void)
{
    int i, j;
    if (!nnames()) { printf("Atlas SHOW needs a name.\n"); return RETURN_ERROR; }
    if (!load(0)) return RETURN_ERROR;
    for (i = 0; names()[i]; i++) {
        Port *p = find(names()[i], 0);
        char buf[300];
        if (!p) return RETURN_WARN;
        line("Name", p->title);
        line("Id", p->id);
        line("About", p->summary);
        line("By", p->author);
        line("Source", repo_source_name(S, p->source));
        line("Kind", p->kind ? p->kind : "game");
        line("Category", p->category);
        line("State", port_state_text(p));
        line("Version", p->version);
        line("Installed", installed_version(p));
        line("In", installed_path(p));
        line("Year", p->year);
        if (p->cpu) { snprintf(buf, sizeof buf, "%s%s", p->cpu, p->fpu ? " with FPU" : ""); line("CPU", buf); }
        line("Fit", p->fit);
        buf[0] = 0;
        for (j = 0; j < p->nmodules; j++) snprintf(buf + strlen(buf), sizeof buf - strlen(buf), "%s%s", j ? ", " : "", p->modules[j]);
        line("Needs", buf);
        if (p->download_size) { snprintf(buf, sizeof buf, "%ld KB", (p->download_size + 1023) / 1024); line("Download", buf); }
        line("Page", p->release_url ? p->release_url : p->port_repo);
        line("Installs", !port_installable(p) ? "not yet" : port_unpack_only(p) ? "by copying its drawer" : "with its Installer script");
        if (names()[i + 1]) printf("\n");
    }
    return RETURN_OK;
}

static int progress(void *user, const char *d, long len, long done, long total)
{
    (void)user; (void)d; (void)len;
    if (total > 0) { printf("\r  %ld%% of %ld KB ", done * 100 / total, total / 1024); fflush(stdout); }
    return !stopped();
}

static void step(void *user, int s, long done, long total)
{
    static const char *const text[STEP_COUNT] = { "Downloading", "Checked", "Unpacking", "Installing", "Tidying up" };
    (void)done; (void)total;
    if (s > STEP_DOWNLOAD) printf("\r");
    printf("  %s %s\n", text[s], ((Port *)user)->title);
}

static void default_drawer(char *out, int max)
{
    BPTR l;
    if (GetVar((STRPTR)"AmiPortAtlas/Drawer", (STRPTR)out, max, GVF_GLOBAL_ONLY) > 0) return;
    l = Lock((STRPTR)"Work:Games", ACCESS_READ);
    if (l) { UnLock(l); snprintf(out, (size_t)max, "Work:Games"); return; }
    snprintf(out, (size_t)max, "SYS:");
}

/* One install into parent, with the checks and the questions. */
static int install_one(Port *p, const char *parent)
{
    char why[240], q[300];
    Source *x = sources_find(S, p->source);
    if (!install_check(p, parent, why, (int)sizeof why)) { printf("Atlas: %s\n", why); return 0; }
    if (x && !x->checked) {
        snprintf(q, sizeof q, "%s comes from %s, which The AmiCradle Team doesn't check. Install it?", p->title, x->name);
        if (!yes(q)) return 0;
    }
    printf("%s %s into %s\n", p->title, p->version ? p->version : "", parent);
    if (!install_run(p, parent, step, progress, p, why, (int)sizeof why)) { printf("\nAtlas: %s\n", why); return 0; }
    printf("  Done: %s is in %s.\n", p->title, installed_path(p) ? installed_path(p) : parent);
    return 1;
}

static int cmd_install(void)
{
    char parent[256];
    int i, bad = 0;
    if (!nnames()) { printf("Atlas INSTALL needs a name.\n"); return RETURN_ERROR; }
    if (!load(0)) return RETURN_ERROR;
    if (arg(A_TO)) snprintf(parent, sizeof parent, "%s", arg(A_TO));
    else default_drawer(parent, (int)sizeof parent);
    for (i = 0; names()[i]; i++) {
        Port *p = find(names()[i], 0);
        if (!p) { bad++; continue; }
        if (installed_version(p) && !updatable(p)) { printf("%s %s is installed already.\n", p->title, installed_version(p)); continue; }
        if (!install_one(p, parent)) bad++;
        if (stopped()) { printf("***Break\n"); return RETURN_WARN; }
    }
    return bad ? RETURN_WARN : RETURN_OK;
}

/* An update goes where the game is: the drawer above its own. */
static void parent_of(const char *path, char *out, int max)
{
    const char *end = path + strlen(path);
    while (end > path && end[-1] != '/' && end[-1] != ':') end--;
    if (end > path && end[-1] == '/') end--;
    snprintf(out, (size_t)max, "%.*s", (int)(end - path), path);
}

static int cmd_upgrade(void)
{
    char parent[256];
    int i, n = 0, bad = 0;
    if (!load(0)) return RETURN_ERROR;
    for (i = 0; i < F->count; i++) {
        Port *p = &F->ports[i];
        int j, named = !nnames();
        for (j = 0; !named && names()[j]; j++) named = find(names()[j], 1) == p;
        if (!named || !updatable(p)) continue;
        if (installed_found(p)) { printf("%s in %s wasn't installed by AmiPortAtlas, so Atlas leaves it alone.\n", p->title, installed_path(p)); continue; }
        parent_of(installed_path(p), parent, (int)sizeof parent);
        printf("%s %s to %s\n", p->title, installed_version(p), p->version);
        if (install_one(p, parent)) n++; else bad++;
        if (stopped()) { printf("***Break\n"); return RETURN_WARN; }
    }
    if (!n && !bad) printf("Everything is up to date.\n");
    return bad ? RETURN_WARN : RETURN_OK;
}

static int cmd_remove(void)
{
    char why[240], q[300];
    int i, bad = 0;
    if (!nnames()) { printf("Atlas REMOVE needs a name.\n"); return RETURN_ERROR; }
    if (!load(0)) return RETURN_ERROR;
    for (i = 0; names()[i]; i++) {
        Port *p = find(names()[i], 0);
        if (!p) { bad++; continue; }
        if (!installed_path(p)) { printf("%s isn't installed.\n", p->title); continue; }
        if (installed_found(p)) { printf("AmiPortAtlas didn't install the copy in %s, so Atlas leaves it alone.\n", installed_path(p)); continue; }
        snprintf(q, sizeof q, "Delete %s and everything in it, saved games too?", installed_path(p));
        if (!yes(q)) continue;
        if (!install_remove(p, why, (int)sizeof why)) { printf("Atlas: %s\n", why); bad++; }
        else printf("Removed %s.\n", p->title);
    }
    return bad ? RETURN_WARN : RETURN_OK;
}

static int cmd_sources(void)
{
    int i;
    if (!load(0)) return RETURN_ERROR;
    for (i = 0; i < S->count; i++) {
        const Source *x = &S->list[i];
        char state[64];
        if (!x->on) snprintf(state, sizeof state, "off");
        else if (x->state == SRC_NO_INDEX) snprintf(state, sizeof state, "on, no index yet");
        else if (x->state == SRC_FAILED) snprintf(state, sizeof state, "on, not read");
        else snprintf(state, sizeof state, "on, %d entries", x->count);
        printf("%-16s %-24s %s%s\n", x->id, x->name, state, x->user ? ", yours" : x->checked ? ", checked by the Team" : "");
        if (x->index) printf("                 %s\n", x->index);
    }
    printf("Atlas SOURCE ON, OFF, ADD and DROP change them; Atlas UPDATE reads them again.\n");
    return RETURN_OK;
}

static int cmd_source(void)
{
    char **w = names();
    char *lines, *edited;
    const char *verb;
    if (nnames() < 2) { printf("Atlas SOURCE ON|OFF|ADD|DROP id [URL=index] [NAME=name]\n"); return RETURN_ERROR; }
    if (is(w[0], "ON")) verb = "on";
    else if (is(w[0], "OFF")) verb = "off";
    else if (is(w[0], "ADD")) verb = "add";
    else if (is(w[0], "DROP")) verb = "drop";
    else { printf("Atlas SOURCE takes ON, OFF, ADD or DROP.\n"); return RETURN_ERROR; }
    if (strcmp(verb, "add") == 0 && !arg(A_URL)) { printf("Atlas SOURCE ADD needs URL=, the address of its index.\n"); return RETURN_ERROR; }
    if (!load(0)) return RETURN_ERROR;
    if (strcmp(verb, "add") == 0 && sources_find(S, w[1]) && !sources_find(S, w[1])->user) {
        printf("%s is in the master list already: Atlas SOURCE ON %s switches it on.\n", w[1], w[1]);
        return RETURN_WARN;
    }
    if ((verb[0] == 'o') && !sources_find(S, w[1])) { printf("There is no source called %s. Atlas SOURCES lists them.\n", w[1]); return RETURN_WARN; }
    lines = repo_user_lines();
    if (!lines) { printf("Atlas: out of memory.\n"); return RETURN_FAIL; }
    edited = sources_edit(lines, verb, w[1], arg(A_URL), arg(A_NAME));
    free(lines);
    if (!edited) { printf("Atlas: an id is 1 to 24 of a-z, 0-9 and -, and an index's URL starts https:// or http://.\n"); return RETURN_ERROR; }
    if (!repo_user_save(edited)) { free(edited); printf("Atlas: ENVARC:" SOURCES_VAR " could not be written.\n"); return RETURN_ERROR; }
    free(edited);
    printf("Done. Atlas UPDATE reads the sources again.\n");
    return RETURN_OK;
}

/* ---- publishing ---- */

static char *dup(const char *s)
{
    char *d;
    if (!s) return NULL;
    d = (char *)malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

static char *utf8(const char *s)
{
    char buf[400];
    if (!s) return NULL;
    latin1_to_utf8(s, buf, (int)sizeof buf);
    return dup(buf);
}

static char *slurp(const char *path, long max, long *len)
{
    BPTR fh = Open((STRPTR)path, MODE_OLDFILE);
    char *b;
    LONG n;
    if (!fh) return NULL;
    b = (char *)malloc((size_t)max + 1);
    if (!b) { Close(fh); return NULL; }
    n = Read(fh, b, max);
    Close(fh);
    if (n < 0) { free(b); return NULL; }
    b[n] = 0;
    *len = n;
    return b;
}

/* The file's size and SHA-256 in hex. */
static int hash_file(const char *path, long *size, char hex[65])
{
    BPTR fh = Open((STRPTR)path, MODE_OLDFILE);
    char *buf;
    unsigned char d[32];
    Sha256 s;
    LONG n;
    int i;
    if (!fh) return 0;
    buf = (char *)malloc(16384);
    if (!buf) { Close(fh); return 0; }
    sha256_init(&s);
    *size = 0;
    while ((n = Read(fh, buf, 16384)) > 0) { sha256_update(&s, buf, (unsigned long)n); *size += n; }
    Close(fh);
    free(buf);
    if (n < 0) return 0;
    sha256_final(&s, d);
    for (i = 0; i < 32; i++) sprintf(hex + 2 * i, "%02x", d[i]);
    return 1;
}

static const char *base_name(const char *path)
{
    const char *b = path, *s;
    for (s = path; *s; s++) if (*s == ':' || *s == '/') b = s + 1;
    return b;
}

/* The upload address for id, from ENVARC:AmiPortAtlas/Publish (UPLOAD= sets it). */
static int upload_address(const char *id, char *out, int max)
{
    char *buf = (char *)malloc(8192), *t, *o;
    LONG n;
    int found = 0;
    if (!buf) return 0;
    n = GetVar((STRPTR)PUBLISH_VAR, (STRPTR)buf, 8192, GVF_GLOBAL_ONLY | GVF_BINARY_VAR);
    buf[n > 0 ? (n < 8191 ? n : 8191) : 0] = 0;
    if (arg(A_UPLOAD)) {
        /* kept for next time: every other line, then this one */
        char *keep = (char *)malloc(8192 + 600);
        if (keep) {
            size_t idn = strlen(id);
            o = keep;
            *o = 0;
            for (t = buf; *t; ) {
                char *nl = strchr(t, '\n');
                size_t len = nl ? (size_t)(nl - t) + 1 : strlen(t);
                if (!(strncmp(t, id, idn) == 0 && (t[idn] == ' ' || t[idn] == '\t'))) { memcpy(o, t, len); o += len; }
                t += len;
            }
            o += sprintf(o, "%s %s\n", id, arg(A_UPLOAD));
            SetVar((STRPTR)PUBLISH_VAR, (STRPTR)keep, (LONG)(o - keep), GVF_GLOBAL_ONLY | GVF_SAVE_VAR);
            free(keep);
        }
        snprintf(out, (size_t)max, "%s", arg(A_UPLOAD));
        free(buf);
        return 1;
    }
    for (t = buf; *t && !found; ) {
        char *nl = strchr(t, '\n');
        size_t idn = strlen(id);
        if (nl) *nl = 0;
        if (strncmp(t, id, idn) == 0 && (t[idn] == ' ' || t[idn] == '\t')) {
            const char *u = t + idn;
            while (*u == ' ' || *u == '\t') u++;
            snprintf(out, (size_t)max, "%s", u);
            n = (LONG)strlen(out);
            while (n > 0 && (out[n - 1] == ' ' || out[n - 1] == '\r')) out[--n] = 0;
            found = n > 0;
        }
        t = nl ? nl + 1 : t + strlen(t);
    }
    free(buf);
    return found;
}

/* The password, typed with nothing shown. */
static void password(const char *who, char *out, int max)
{
    BPTR in = Input();
    int n = 0;
    char c;
    printf("Password for %s: ", who);
    fflush(stdout);
    SetMode(in, 1);
    while (Read(in, &c, 1) == 1 && c != '\r' && c != '\n' && c != 3) {
        if ((c == 8 || c == 127) && n) n--;
        else if ((unsigned char)c >= ' ' && n < max - 1) out[n++] = c;
    }
    SetMode(in, 0);
    out[n] = 0;
    printf("\n");
}

/* Today as YYYY-MM-DD, from the system clock (days since 1 January 1978). */
static void today_iso(char *out, int max)
{
    static const int mdays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    struct DateStamp ds;
    long d, y = 1978;
    int m = 0;
    DateStamp(&ds);
    d = ds.ds_Days;
    for (;;) {
        int leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
        if (d < 365 + leap) break;
        d -= 365 + leap;
        y++;
    }
    for (m = 0; m < 12; m++) {
        int n = mdays[m] + (m == 1 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0));
        if (d < n) break;
        d -= n;
    }
    snprintf(out, (size_t)max, "%04ld-%02d-%02ld", y, m + 1, d + 1);
}

static void add_shot(Port *p, const char *url)
{
    char **n = (char **)realloc(p->shots, (size_t)(p->nshots + 1) * sizeof(char *));
    if (!n) return;
    p->shots = n;
    p->shots[p->nshots] = dup(url);
    if (p->shots[p->nshots]) p->nshots++;
}

static int cmd_publish(void)
{
    char why[240], upload[420], user[128], host[256], path[300], pass[128], hex[65], readme_path[300];
    char base[420], url[600], id[64], title[120], index_name[80], *p_dot;
    const char *archive, *to = arg(A_TO);
    char *readme_text = NULL, *index_text = NULL, *out = NULL;
    long size = 0, len = 0, rlen = 0;
    int secure, port, missing = 0, rc = RETURN_ERROR;
    Source *x;
    Readme r;
    Port e;
    Feed *f = NULL;
    Ftp *ftp = NULL;
    const char *bad;

    memset(&e, 0, sizeof e);
    memset(&r, 0, sizeof r);
    if (nnames() != 1 || !to) { printf("Atlas PUBLISH archive.lha TO=source [UPLOAD=ftp(s)://user@host/drawer/] ...\n"); return RETURN_ERROR; }
    archive = names()[0];
    if (!load(0)) return RETURN_ERROR;
    x = sources_find(S, to);
    if (!upload_address(to, upload, (int)sizeof upload)) {
        printf("Atlas: where %s's files go isn't known yet. Give it once with UPLOAD=ftps://user@host/drawer/.\n", to);
        return RETURN_ERROR;
    }
    if (!ftp_url(upload, &secure, user, (int)sizeof user, host, (int)sizeof host, &port, path, (int)sizeof path)) {
        printf("Atlas: %s is not an ftp:// or ftps:// address.\n", upload);
        return RETURN_ERROR;
    }
    if (!hash_file(archive, &size, hex)) { printf("Atlas: %s could not be read.\n", archive); return RETURN_ERROR; }

    /* the readme beside it: Game.lha has Game.readme */
    if (arg(A_README)) snprintf(readme_path, sizeof readme_path, "%s", arg(A_README));
    else {
        snprintf(readme_path, sizeof readme_path, "%s", archive);
        p_dot = strrchr(readme_path, '.');
        if (p_dot && p_dot > (char *)base_name(readme_path)) *p_dot = 0;
        strncat(readme_path, ".readme", sizeof readme_path - strlen(readme_path) - 1);
    }
    readme_text = slurp(readme_path, 32768, &rlen);
    if (readme_text) readme_parse(readme_text, rlen, &r);
    else readme_path[0] = 0;
    if (!x || !x->index) {
        /* no index of ours there: the archive and its readme only, as Aminet takes them */
        if (!readme_text || !r.shortdesc[0] || !r.type[0]) {
            printf("Atlas: %s takes an archive with a readme that has Short: and Type:; %s has none.\n", to, archive);
            goto out;
        }
    }

    feed_utf8 = 1;    /* an index is UTF-8, kept as it is */
    if (x && x->index) {
        const char *slash = strrchr(x->index, '/');
        snprintf(base, sizeof base, "%.*s", slash ? (int)(slash - x->index) + 1 : 0, x->index);
        snprintf(index_name, sizeof index_name, "%s", slash ? slash + 1 : "index.json");
        index_id(archive, id, (int)sizeof id);
        snprintf(title, sizeof title, "%s", base_name(archive));
        if ((p_dot = strrchr(title, '.')) != NULL) *p_dot = 0;
        if (arg(A_TITLE)) snprintf(title, sizeof title, "%s", arg(A_TITLE));
        e.id = dup(id);
        e.title = utf8(title);
        e.version = utf8(arg(A_VERSION) ? arg(A_VERSION) : r.version[0] ? r.version : NULL);
        e.summary = r.shortdesc[0] ? dup(r.shortdesc) : NULL;
        e.author = r.author[0] ? dup(r.author) : NULL;
        e.category = r.type[0] ? dup(r.type) : NULL;
        e.kind = dup(arg(A_KIND) ? arg(A_KIND) : readme_kind(r.type[0] ? r.type : NULL));
        e.state = dup("released");
        e.drawer = utf8(arg(A_DRAWER) ? arg(A_DRAWER) : title);
        e.program = utf8(arg(A_PROGRAM) ? arg(A_PROGRAM) : arg(A_DRAWER) ? arg(A_DRAWER) : title);
        e.install = dup(args[A_INSTALLER] ? "script" : "unpack");
        e.cpu = dup(arg(A_CPU));
        e.fpu = args[A_FPU] != 0;
        snprintf(url, sizeof url, "%s%s", base, base_name(archive));
        e.download_url = dup(url);
        e.download_size = size;
        e.download_sha256 = dup(hex);
        e.release_url = dup(x->home);
        if (arg(A_PICTURES)) {
            char list[400], *s, *c;
            snprintf(list, sizeof list, "%s", arg(A_PICTURES));
            for (s = list; s && *s; s = c) {
                c = strchr(s, ',');
                if (c) *c++ = 0;
                snprintf(url, sizeof url, "%s%s", base, base_name(s));
                add_shot(&e, url);
            }
        }
        if (!e.id || !e.id[0] || !port_plain_name(e.drawer) || !port_plain_name(e.program)) {
            printf("Atlas: the id, DRAWER= and PROGRAM= must be plain names (no : or /).\n");
            goto out;
        }
    }

    if (strcmp(user, "anonymous") == 0 || strcmp(user, "ftp") == 0) snprintf(pass, sizeof pass, "amiportatlas@");
    else { char who[400]; snprintf(who, sizeof who, "%s@%s", user, host); password(who, pass, (int)sizeof pass); }
    printf("Connecting to %s%s...\n", host, secure ? " (TLS)" : "");
    ftp = ftp_open(upload, pass, why, (int)sizeof why);
    memset(pass, 0, sizeof pass);
    if (!ftp) { printf("Atlas: %s\n", why); goto out; }

    if (x && x->index) {
        if (ftp_get_mem(ftp, index_name, REPO_INDEX_MAX, &index_text, &len, &missing, why, (int)sizeof why)) {
            f = feed_parse(index_text, len, &bad);
            if (!f) { printf("Atlas: %s's %s could not be read (%s), so it is left as it is.\n", to, index_name, bad); goto out; }
        } else if (missing) {
            printf("%s has no %s yet: a new one is made.\n", to, index_name);
            f = (Feed *)calloc(1, sizeof(Feed));
            if (!f) goto out;
            f->schema = FEED_SCHEMA;
        } else { printf("Atlas: %s\n", why); goto out; }
    }

    /* the files first, the index last, so it never names one that isn't there */
    printf("Uploading %s (%ld KB)\n", base_name(archive), (size + 1023) / 1024);
    if (!ftp_put_file(ftp, archive, base_name(archive), f != NULL, progress, NULL, why, (int)sizeof why)) {
        printf("\nAtlas: %s\n", why);
        goto out;
    }
    printf("\n");
    if (readme_path[0]) {
        printf("Uploading %s\n", base_name(readme_path));
        if (!ftp_put_file(ftp, readme_path, base_name(readme_path), f != NULL, NULL, NULL, why, (int)sizeof why)) {
            printf("Atlas: %s\n", why);
            goto out;
        }
    }
    if (f && arg(A_PICTURES)) {
        char list[400], *s, *c;
        snprintf(list, sizeof list, "%s", arg(A_PICTURES));
        for (s = list; s && *s; s = c) {
            c = strchr(s, ',');
            if (c) *c++ = 0;
            printf("Uploading %s\n", base_name(s));
            if (!ftp_put_file(ftp, s, base_name(s), 1, NULL, NULL, why, (int)sizeof why)) { printf("Atlas: %s\n", why); goto out; }
        }
    }
    if (f) {
        char today[32];
        today_iso(today, (int)sizeof today);
        if (!index_put(f, &e)) { printf("Atlas: out of memory.\n"); goto out; }
        out = feed_write(f, x->name, today, &len);
        if (!out) { printf("Atlas: out of memory.\n"); goto out; }
        printf("Uploading %s (%d entries)\n", index_name, f->count);
        if (!ftp_put_mem(ftp, out, len, index_name, 1, why, (int)sizeof why)) { printf("Atlas: %s\n", why); goto out; }
        printf("Published %s to %s: %s%s\n", title, x->name, base, base_name(archive));
    } else {
        printf("Uploaded %s and its readme to %s.\n", base_name(archive), to);
    }
    rc = RETURN_OK;
out:
    if (ftp) ftp_close(ftp);
    feed_free(f);
    free(out);
    free(index_text);
    free(readme_text);
    port_free(&e);   /* whatever wasn't moved into the index */
    feed_utf8 = 0;
    return rc;
}

/* ---- the program ---- */

int main(void)
{
    struct RDArgs *rd;
    const char *missing = NULL, *c;
    int rc = RETURN_FAIL;
    if (!platform_open(&missing)) { printf("Atlas needs %s.\n", missing); return RETURN_FAIL; }
    rd = ReadArgs((STRPTR)TEMPLATE, args, NULL);
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"Atlas");
        printf("Atlas UPDATE | LIST | SEARCH | SHOW | INSTALL | UPGRADE | REMOVE | SOURCES | SOURCE | PUBLISH\n");
        platform_close();
        return RETURN_ERROR;
    }
    c = arg(A_COMMAND);
    if (is(c, "UPDATE")) rc = cmd_update();
    else if (is(c, "LIST")) rc = cmd_list();
    else if (is(c, "SEARCH")) rc = cmd_search();
    else if (is(c, "SHOW") || is(c, "INFO")) rc = cmd_show();
    else if (is(c, "INSTALL")) rc = cmd_install();
    else if (is(c, "UPGRADE")) rc = cmd_upgrade();
    else if (is(c, "REMOVE")) rc = cmd_remove();
    else if (is(c, "SOURCES")) rc = cmd_sources();
    else if (is(c, "SOURCE")) rc = cmd_source();
    else if (is(c, "PUBLISH")) rc = cmd_publish();
    else {
        printf("Atlas: %s isn't a command. Atlas UPDATE | LIST | SEARCH | SHOW | INSTALL | UPGRADE | REMOVE |\n"
               "SOURCES | SOURCE | PUBLISH; docs/SOURCES.md has them all.\n", c);
        rc = RETURN_ERROR;
    }
    FreeArgs(rd);
    feed_free(F);
    sources_free(S);
    platform_close();
    return rc;
}
