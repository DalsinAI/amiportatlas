/* AmiPortAtlas: checking, installing, starting and removing a game.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * Every port ships an archive with, at its root, the game's drawer, its
 * icon and an Installer script (Install-DRAWER) made by amicradle's
 * scripts/port_installer.py. AmiPortAtlas unpacks it into T: and runs that
 * script, so an install from here and one by hand do the same thing. The
 * drawer the player picked is handed to the script in ENV:AmiPortAtlas/Parent.
 */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/var.h>
#include <dos/dosextens.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <proto/icon.h>

#include "install.h"
#include "match.h"

#define WORK      "T:AmiPortAtlas"
#define LIST_ENV  "AmiPortAtlas/Installed"
#define MAX_GAMES 512

/* path is the game's drawer; program the file in it that starts the game,
 * when it isn't the feed's; found: on the disk already, not installed by us */
typedef struct { char *id, *version, *path, *program; int found; } Installed;
static Installed games[MAX_GAMES];
static int ngames;

static void say(char *why, int whylen, const char *text)
{
    if (whylen > 0) { strncpy(why, text, (size_t)whylen - 1); why[whylen - 1] = 0; }
}

static char *dup(const char *s)
{
    char *d = (char *)malloc(strlen(s) + 1);
    if (d) strcpy(d, s);
    return d;
}

static int exists(const char *path)
{
    BPTR l = Lock((STRPTR)path, ACCESS_READ);
    if (!l) return 0;
    UnLock(l);
    return 1;
}

static void forget_all(void)
{
    int i;
    for (i = 0; i < ngames; i++) {
        free(games[i].id); free(games[i].version); free(games[i].path); free(games[i].program);
    }
    ngames = 0;
}

void installed_load(void)
{
    char *buf;
    LONG n;
    forget_all();
    buf = (char *)malloc(16384);
    if (!buf) return;
    n = GetVar((STRPTR)LIST_ENV, (STRPTR)buf, 16384, GVF_GLOBAL_ONLY | GVF_BINARY_VAR);
    if (n > 0) {
        char *line = buf;
        buf[n < 16383 ? n : 16383] = 0;
        while (*line && ngames < MAX_GAMES) {
            /* id, version, drawer; then, from 0.2, the program and "found" */
            char *end = strchr(line, '\n'), *t1, *t2, *t3, *t4;
            if (end) *end = 0;
            t1 = strchr(line, '\t');
            t2 = t1 ? strchr(t1 + 1, '\t') : NULL;
            t3 = t2 ? strchr(t2 + 1, '\t') : NULL;
            t4 = t3 ? strchr(t3 + 1, '\t') : NULL;
            if (t1 && t2) {
                Installed *g = &games[ngames];
                *t1 = *t2 = 0;
                if (t3) *t3 = 0;
                if (t4) *t4 = 0;
                g->id = dup(line);
                g->version = dup(t1 + 1);
                g->path = dup(t2 + 1);
                g->program = t3 && t3[1] ? dup(t3 + 1) : NULL;
                g->found = t4 && strcmp(t4 + 1, "found") == 0;
                if (g->id && g->version && g->path) ngames++;
            }
            if (!end) break;
            line = end + 1;
        }
    }
    free(buf);
}

static int installed_save(void)
{
    char *buf;
    long len = 0;
    int i, ok;
    buf = (char *)malloc(16384);
    if (!buf) return 0;
    buf[0] = 0;
    for (i = 0; i < ngames; i++)
        if (len < 16000)
            len += snprintf(buf + len, (size_t)(16384 - len), "%s\t%s\t%s\t%s\t%s\n", games[i].id, games[i].version,
                            games[i].path, games[i].program ? games[i].program : "", games[i].found ? "found" : "");
    /* SetVar with GVF_SAVE_VAR writes ENVARC: as well as ENV: (3.2, 4.1, AROS). */
    ok = SetVar((STRPTR)LIST_ENV, (STRPTR)buf, len, GVF_GLOBAL_ONLY | GVF_SAVE_VAR) != 0;
    free(buf);
    return ok;
}

static Installed *find(const Port *p)
{
    int i;
    if (!p->id) return NULL;
    for (i = 0; i < ngames; i++) if (strcmp(games[i].id, p->id) == 0) return &games[i];
    return NULL;
}

const char *installed_version(const Port *p) { Installed *g = find(p); return g ? g->version : NULL; }
const char *installed_path(const Port *p)    { Installed *g = find(p); return g ? g->path : NULL; }
int installed_found(const Port *p)          { Installed *g = find(p); return g && g->found; }

static int remember(const Port *p, const char *path)
{
    Installed *g = find(p);
    if (!g) {
        if (ngames == MAX_GAMES) return 0;
        g = &games[ngames++];
        g->id = dup(p->id);
        g->version = g->path = g->program = NULL;
    } else {
        free(g->version);
        free(g->path);
        free(g->program);
    }
    g->version = dup(p->version ? p->version : "?");
    g->path = dup(path);
    g->program = NULL;
    g->found = 0;
    return installed_save();
}

static int cpu_number(const char *cpu)
{
    return cpu ? atoi(cpu) : 0;  /* "68040" */
}

int install_check(const Port *p, const char *parent, char *why, int whylen)
{
    char path[300];
    long need_kb, free_kb;
    int i;
    if (!port_installable(p)) { say(why, whylen, "There is no release of this game to install yet."); return 0; }
    if (!AP_RUNS_68K_PORTS) {
        say(why, whylen, "This game is built for AmigaOS 3 on a 68k. A build for " AP_SYSTEM " isn't in the Port Atlas yet.");
        return 0;
    }
    if (cpu_number(p->cpu) > platform_cpu()) {
        snprintf(why, (size_t)whylen, "%s needs a %s or faster; this Amiga has a %d.", p->title, p->cpu, platform_cpu());
        return 0;
    }
    if (p->fpu && !platform_has_fpu()) {
        snprintf(why, (size_t)whylen, "%s needs an FPU, and this Amiga has none.", p->title);
        return 0;
    }
    for (i = 0; i < p->nmodules; i++) {
        snprintf(path, sizeof path, "LIBS:GFOpenGPU/%s", p->modules[i]);
        if (exists(path)) continue;
        snprintf(path, sizeof path, "LIBS:OpenGPU/%s", p->modules[i]);  /* older instances: the script copies it */
        if (exists(path)) continue;
        snprintf(why, (size_t)whylen, "%s needs %s, which is part of GFOpenUp. Install or update GFOpenUp first.", p->title, p->modules[i]);
        return 0;
    }
    if (!exists(parent)) { snprintf(why, (size_t)whylen, "%s is not there.", parent); return 0; }
    /* The space the game needs, and the archive and its unpacked copy in T:. */
    need_kb = (p->space + 2 * p->download_size) / 1024 + 1;
    free_kb = platform_free_kb(parent);
    if (free_kb >= 0 && free_kb < (p->space / 1024 + 1)) {
        snprintf(why, (size_t)whylen, "%s needs %ld KB free on %s; there are %ld KB.", p->title, p->space / 1024 + 1, parent, free_kb);
        return 0;
    }
    free_kb = platform_free_kb("T:");
    if (free_kb >= 0 && free_kb < need_kb - p->space / 1024) {
        snprintf(why, (size_t)whylen, "T: needs %ld KB free to unpack %s.", need_kb - p->space / 1024, p->title);
        return 0;
    }
    return 1;
}

/* AmigaDOS paths joined: "Work:" + "Games" and "Work:Games" + "Kobo". */
static void join(char *out, int max, const char *a, const char *b)
{
    size_t n = strlen(a);
    if (n && a[n - 1] != ':' && a[n - 1] != '/') snprintf(out, (size_t)max, "%s/%s", a, b);
    else snprintf(out, (size_t)max, "%s%s", a, b);
}

static int unpack(const char *archive, const char *dest)
{
    char cmd[700];
    if (exists("SYS:Utilities/OpenCompress")) {
        snprintf(cmd, sizeof cmd, "SYS:Utilities/OpenCompress x \"%s\" \"%s/\"", archive, dest);
        if (platform_run(cmd) == 0) return 1;
    }
    /* AmigaOS 4.1 and many 3.x systems have LhA in C:. */
    snprintf(cmd, sizeof cmd, "LhA -q x \"%s\" \"%s/\"", archive, dest);
    return platform_run(cmd) == 0;
}

static void tidy(const char *dir, const char *archive)
{
    char cmd[400];
    snprintf(cmd, sizeof cmd, "C:Delete \"%s\" ALL QUIET FORCE", dir);
    if (exists(dir)) platform_run(cmd);
    if (archive && archive[0]) DeleteFile((STRPTR)archive);
}

int install_run(const Port *p, const char *parent, StepFn step, HttpSink progress,
                void *user, char *why, int whylen)
{
    char archive[300], dir[300], script[300], dest[300], cmd[1400];
    BPTR l;
    LONG rc;

    if (!install_check(p, parent, why, whylen)) return 0;
    l = CreateDir((STRPTR)WORK);
    if (l) UnLock(l);
    snprintf(archive, sizeof archive, WORK "/%s.lha", p->id);
    snprintf(dir, sizeof dir, WORK "/%s", p->id);

    step(user, STEP_DOWNLOAD, 0, p->download_size);
    /* The checksum is checked as the file comes in; the step is shown after. */
    if (!http_get_file(p->download_url, archive, p->download_sha256, progress, user, why, whylen)) return 0;
    step(user, STEP_CHECK, 0, 0);

    step(user, STEP_UNPACK, 0, 0);
    tidy(dir, NULL);
    l = CreateDir((STRPTR)dir);
    if (l) UnLock(l);
    if (!unpack(archive, dir)) {
        say(why, whylen, "The archive could not be unpacked (SYS:Utilities/OpenCompress or C:LhA is needed).");
        tidy(dir, archive);
        return 0;
    }
    join(script, sizeof script, dir, "Install-");
    strncat(script, p->drawer, sizeof script - strlen(script) - 1);
    if (!exists(script)) {
        snprintf(why, (size_t)whylen, "The archive has no Install-%s script.", p->drawer);
        tidy(dir, archive);
        return 0;
    }

    step(user, STEP_INSTALL, 0, 0);
    SetVar((STRPTR)"AmiPortAtlas/Parent", (STRPTR)parent, -1, GVF_GLOBAL_ONLY);
    /* The script looks for the game's drawer beside itself, so it runs from
     * the unpacked drawer. Novice: no questions; the drawer is the one picked. */
    snprintf(cmd, sizeof cmd, "CD \"%s\"\nSYS:System/Installer \"%s\" APPNAME \"%s\" MINUSER NOVICE DEFUSER NOVICE",
             dir, script, p->title);
    rc = platform_run(cmd);
    DeleteVar((STRPTR)"AmiPortAtlas/Parent", GVF_GLOBAL_ONLY);
    join(dest, sizeof dest, parent, p->drawer);
    join(cmd, sizeof cmd, dest, p->program);
    if (rc != 0 || !exists(cmd)) {
        if (rc == -1) say(why, whylen, "SYS:System/Installer could not be started.");
        else snprintf(why, (size_t)whylen, "The installer stopped before %s was installed.", p->title);
        tidy(dir, archive);
        return 0;
    }

    step(user, STEP_TIDY, 0, 0);
    tidy(dir, archive);
    if (!remember(p, dest)) {
        snprintf(why, (size_t)whylen, "%s is installed in %s, but ENVARC:" LIST_ENV " could not be written.", p->title, dest);
        return 0;
    }
    return 1;
}

int install_launch(const Port *p, char *why, int whylen)
{
    char path[300];
    const char *drawer = installed_path(p);
    if (!drawer) { say(why, whylen, "This game isn't installed."); return 0; }
    {
        Installed *g = find(p);
        join(path, sizeof path, drawer, g && g->program ? g->program : p->program ? p->program : p->drawer);
    }
    if (!exists(path)) {
        snprintf(why, (size_t)whylen, "%s is no longer there. Was it moved?", path);
        return 0;
    }
    if (!platform_launch(path)) { snprintf(why, (size_t)whylen, "%s could not be started.", path); return 0; }
    return 1;
}

int install_remove(const Port *p, char *why, int whylen)
{
    char cmd[400], info[310];
    Installed *g = find(p);
    if (!g) { say(why, whylen, "This game isn't installed."); return 0; }
    if (g->found) {
        snprintf(why, (size_t)whylen, "AmiPortAtlas didn't install the copy in %s, so it leaves it alone. "
                 "Delete it from Workbench if you want it gone.", g->path);
        return 0;
    }
    snprintf(cmd, sizeof cmd, "C:Delete \"%s\" ALL QUIET FORCE", g->path);
    if (exists(g->path) && platform_run(cmd) != 0) {
        snprintf(why, (size_t)whylen, "%s could not be deleted. Is the game running?", g->path);
        return 0;
    }
    snprintf(info, sizeof info, "%s.info", g->path);
    DeleteFile((STRPTR)info);
    free(g->id); free(g->version); free(g->path); free(g->program);
    *g = games[--ngames];
    return installed_save();
}

/* ---- finding games already on the disks, and their icons ---- */

/* Drawers at a disk's top that are the system's, never a game's */
static int system_drawer(const char *name)
{
    static const char *const sys[] = { "C", "L", "S", "Libs", "Devs", "Fonts", "Prefs", "Locale", "Classes",
                                       "Storage", "System", "Utilities", "Tools", "Rexxc", "Expansion",
                                       "Trashcan", "T", "Env", "Envarc", "SDK", "Include", "Development" };
    unsigned i;
    char key[MATCH_NAME], k2[MATCH_NAME];
    match_key(name, key, sizeof key);
    for (i = 0; i < sizeof sys / sizeof sys[0]; i++) {
        match_key(sys[i], k2, sizeof k2);
        if (strcmp(key, k2) == 0) return 1;
    }
    return 0;
}

/* A program the system can start: an AmigaOS hunk file, or ELF (OS 4, AROS) */
static int runnable(const char *path)
{
    unsigned char h[4];
    BPTR fh = Open((STRPTR)path, MODE_OLDFILE);
    LONG n;
    if (!fh) return 0;
    n = Read(fh, h, 4);
    Close(fh);
    if (n != 4) return 0;
    return (h[0] == 0 && h[1] == 0 && h[2] == 3 && h[3] == 0xF3) || (h[0] == 0x7F && h[1] == 'E' && h[2] == 'L' && h[3] == 'F');
}

/* The version in path's $VER string, read in pieces, or "found" */
static void read_version(const char *path, char *out, int max)
{
    char *buf = (char *)malloc(8192 + 64);
    BPTR fh;
    LONG n, keep = 0;
    snprintf(out, (size_t)max, "found");
    if (!buf) return;
    fh = Open((STRPTR)path, MODE_OLDFILE);
    if (fh) {
        while ((n = Read(fh, buf + keep, 8192)) > 0) {
            LONG i, total = keep + n;
            for (i = 0; i + 5 <= total; i++)
                if (buf[i] == '$' && memcmp(buf + i, "$VER:", 5) == 0) {
                    char line[64];
                    LONG j = 0;
                    if (total - i < 48 && n == 8192) break;     /* cut by the piece: read on */
                    while (j < 63 && i + j < total && buf[i + j] >= ' ') { line[j] = buf[i + j]; j++; }
                    line[j] = 0;
                    if (!match_version(line, out, max)) snprintf(out, (size_t)max, "found");
                    Close(fh);
                    free(buf);
                    return;
                }
            keep = total < 64 ? total : 64;                     /* the tail, in case "$VER:" was cut */
            memmove(buf, buf + total - keep, (size_t)keep);
        }
        Close(fh);
    }
    free(buf);
}

typedef struct {
    const Matcher *m;
    ScanFn tell;
    void *user;
    int found, stop;
    long drawers;
} Scan;

/* In a matched drawer: its best program; the game remembered */
static void take(Scan *sc, Port *p, int strength, const char *drawer, const char *name)
{
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    char best[108], file[400], ver[32];
    int best_score = 0;
    Installed *g;
    BPTR l;
    if (!fib) return;
    best[0] = 0;
    l = Lock((STRPTR)drawer, ACCESS_READ);
    if (l && Examine(l, fib)) {
        while (ExNext(l, fib)) {
            int score;
            if (fib->fib_DirEntryType > 0) continue;
            score = match_program(p, name, (const char *)fib->fib_FileName);
            if (score <= best_score) continue;
            join(file, sizeof file, drawer, (const char *)fib->fib_FileName);
            if (!runnable(file)) continue;
            best_score = score;
            strncpy(best, (const char *)fib->fib_FileName, sizeof best - 1);
            best[sizeof best - 1] = 0;
        }
    }
    if (l) UnLock(l);
    FreeDosObject(DOS_FIB, fib);
    if (!best[0]) return;
    g = find(p);
    if (g) return;              /* ours from Install, kept as it is; or found once already, the first kept */
    (void)strength;
    join(file, sizeof file, drawer, best);
    read_version(file, ver, (int)sizeof ver);
    if (ngames == MAX_GAMES) return;
    g = &games[ngames++];
    g->id = dup(p->id);
    g->version = dup(ver);
    g->path = dup(drawer);
    g->program = dup(best);
    g->found = 1;
    if (!g->id || !g->version || !g->path) { free(g->id); free(g->version); free(g->path); free(g->program); ngames--; return; }
    sc->found++;
}

/* The drawers in dir, to depth more levels: each a game, or looked in */
static void walk(Scan *sc, const char *dir, int depth, int top)
{
    struct FileInfoBlock *fib;
    char path[400];
    BPTR l;
    if (sc->stop) return;
    fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    if (!fib) return;
    l = Lock((STRPTR)dir, ACCESS_READ);
    if (l && Examine(l, fib) && fib->fib_DirEntryType > 0) {
        while (!sc->stop && ExNext(l, fib)) {
            const char *name = (const char *)fib->fib_FileName;
            Port *p;
            int strength = 0;
            if (fib->fib_DirEntryType <= 0 || fib->fib_DirEntryType == ST_SOFTLINK) continue;
            if (top && system_drawer(name)) continue;
            join(path, sizeof path, dir, name);
            if (++sc->drawers % 25 == 0 && sc->tell && !sc->tell(sc->user, path, sc->found)) sc->stop = 1;
            p = sc->m ? matcher_find(sc->m, name, &strength) : NULL;
            if (p) take(sc, p, strength, path, name);
            else if (depth > 0) walk(sc, path, depth - 1, 0);
        }
    }
    if (l) UnLock(l);
    FreeDosObject(DOS_FIB, fib);
}

int install_scan(Port *const *ports, int n, ScanFn tell, void *user)
{
    Scan sc;
    Matcher *m = matcher_new(ports, n);
    char disk[8];
    int i;
    struct Process *me = (struct Process *)FindTask(NULL);
    APTR win = me->pr_WindowPtr;
    if (!m) return -1;
    /* what was found before is looked for again: it may have moved or gone */
    for (i = 0; i < ngames; ) {
        if (games[i].found) {
            free(games[i].id); free(games[i].version); free(games[i].path); free(games[i].program);
            games[i] = games[--ngames];
        } else {
            i++;
        }
    }
    memset(&sc, 0, sizeof sc);
    sc.m = m;
    sc.tell = tell;
    sc.user = user;
    me->pr_WindowPtr = (APTR)-1;                    /* no "insert volume DH5:" requesters */
    for (i = 0; i < 16 && !sc.stop; i++) {
        BPTR l;
        snprintf(disk, sizeof disk, "DH%d:", i);
        l = Lock((STRPTR)disk, ACCESS_READ);
        if (!l) continue;
        UnLock(l);
        if (tell && !tell(user, disk, sc.found)) break;
        walk(&sc, disk, 2, 1);                       /* DH1:Games/Shooters/Quake at most */
    }
    me->pr_WindowPtr = win;
    matcher_free(m);
    installed_save();
    return sc.found;
}

/* A default icon for path when it has none: WBDRAWER or WBTOOL. 1 when one was made. */
static int give_icon(const char *path, LONG type)
{
    struct DiskObject *d = GetDiskObject((STRPTR)path);
    if (d) { FreeDiskObject(d); return 0; }         /* an icon already: never touched */
    d = GetDefDiskObject(type);
    if (!d) return 0;
    d->do_CurrentX = d->do_CurrentY = NO_ICON_POSITION;
    if (type == WBTOOL && d->do_StackSize < 65536) d->do_StackSize = 65536;   /* ports want a big stack */
    if (!PutDiskObject((STRPTR)path, d)) { FreeDiskObject(d); return 0; }
    FreeDiskObject(d);
    return 1;
}

int install_missing_icons(int give)
{
    char prog[400];
    int i, games_short = 0;
    for (i = 0; i < ngames; i++) {
        Installed *g = &games[i];
        struct DiskObject *d;
        int lacks = 0;
        if (!g->program) continue;
        join(prog, sizeof prog, g->path, g->program);
        if ((d = GetDiskObject((STRPTR)g->path)) != NULL) FreeDiskObject(d); else lacks = 1;
        if ((d = GetDiskObject((STRPTR)prog)) != NULL) FreeDiskObject(d); else lacks = 1;
        if (!lacks) continue;
        if (!give) { games_short++; continue; }
        if (give_icon(g->path, WBDRAWER) | give_icon(prog, WBTOOL)) games_short++;
    }
    return games_short;
}
