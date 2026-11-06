/* AmiPortAtlas: the program, on GFOpenLayout UI in an Intuition window.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * ui.c describes the window; this file gives it what needs the system:
 * the window and its event loop (olui's Amiga glue), the pictures (through
 * datatypes, scaled into the picture box, drawn by the program), the list's
 * state marks, the sources (repo.c) and installing.
 */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/var.h>
#include <intuition/intuition.h>
#include <graphics/gfx.h>
#include <datatypes/datatypes.h>
#include <datatypes/pictureclass.h>
#include <libraries/asl.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/datatypes.h>
#include <proto/asl.h>
#include <clib/alib_protos.h>

#include "gfopenlayout_ui_amiga.h"
#include "feed.h"
#include "http.h"
#include "install.h"
#include "repo.h"
#include "ui.h"

#define CACHE      "PROGDIR:Cache"
#define PICTURES   CACHE "/Pictures"   /* kept until the Atlas stops naming them */

static const char vers[] = "$VER: AmiPortAtlas 0.2.1 (06.11.2026) The AmiCradle Team";

static struct {
    struct Window *win;
    olui *ui;
    olui_amiga *glue;
    Feed *feed;
    Sources *sources;
    const char **from_ids, **from_names;   /* the sources switched on, for From */
    LONG pens[MARK_ORIGINAL + 1];
    Object *dto;                 /* the current picture */
    ULONG src_w, src_h;          /* its own size */
    struct BitMap *pic;          /* the datatype's bitmap, scaled to the box */
    int pic_w, pic_h, box_w, box_h;
    const Port *pic_port;        /* whose picture it is, and which */
    int pic_shot;
    char failed[8][200];         /* pictures that didn't come: not asked for again until a refresh */
    int nfailed;
    int offline;                 /* no network for pictures: none fetched until a refresh */
} A;

static void drop_old_pictures(void);
static int all_fresh(void);

/* ---- the pictures ---- */

/* picture.datatype V44 scales its own picture. graphics.library's
 * BitMapScale() draws garbage from an RTG bitmap (seen on OpenBench),
 * so the datatype does it, and the program only blits the result. */
#ifndef PDTM_SCALE
#define PDTM_SCALE (DTM_Dummy + 0x62)
struct pdtScale { ULONG MethodID; ULONG ps_NewWidth; ULONG ps_NewHeight; ULONG ps_Flags; };
#endif

static void drop_picture(void)
{
    if (A.dto) { WaitBlit(); DisposeDTObject(A.dto); A.dto = NULL; }
    A.pic = NULL;
    A.src_w = A.src_h = 0;
    A.box_w = A.box_h = 0;
    A.pic_port = NULL;
}

static int failed_before(const char *url)
{
    int i;
    for (i = 0; i < A.nfailed && i < 8; i++) if (strncmp(A.failed[i], url, sizeof A.failed[i] - 1) == 0) return 1;
    return 0;
}

static void failed_now(const char *url)
{
    snprintf(A.failed[A.nfailed++ % 8], sizeof A.failed[0], "%s", url);
}

/* The picture scaled to fit w x h, keeping its shape. When the datatype
 * can't scale, the picture is drawn at its own size, centred and clipped. */
static void scale_to(int w, int h)
{
    ULONG dw, dh;
    struct pdtScale ps;
    if (!A.dto || !A.src_w || !A.src_h || w <= 0 || h <= 0) return;
    if (A.pic && A.box_w == w && A.box_h == h) return;
    A.box_w = w;
    A.box_h = h;
    dw = (ULONG)w;
    dh = A.src_h * dw / A.src_w;
    if (dh > (ULONG)h) { dh = (ULONG)h; dw = A.src_w * dh / A.src_h; }
    if (dw < 1) dw = 1;
    if (dh < 1) dh = 1;
    A.pic = NULL;
    ps.MethodID = PDTM_SCALE;
    ps.ps_NewWidth = dw;
    ps.ps_NewHeight = dh;
    ps.ps_Flags = 0;
    if (DoMethodA(A.dto, (Msg)&ps) && DoDTMethod(A.dto, NULL, NULL, DTM_PROCLAYOUT, NULL, TRUE)) {
        A.pic_w = (int)dw;
        A.pic_h = (int)dh;
    } else {
        DoDTMethod(A.dto, NULL, NULL, DTM_PROCLAYOUT, NULL, TRUE);
        A.pic_w = (int)A.src_w;
        A.pic_h = (int)A.src_h;
    }
    GetDTAttrs(A.dto, PDTA_DestBitMap, (ULONG)&A.pic, TAG_DONE);
}

/* olui_amiga_own: the picture box, drawn by us in the window's pixels. */
static void draw_shot(void *data, const olui_el *el, int x, int y, int w, int h)
{
    struct RastPort *rp = A.win->RPort;
    (void)data; (void)el;
    scale_to(w, h);
    SetAPen(rp, 1);
    RectFill(rp, x, y, x + w - 1, y + h - 1);
    if (A.pic) {
        /* Centred; a picture bigger than the box shows its middle. */
        int bw = A.pic_w < w ? A.pic_w : w, bh = A.pic_h < h ? A.pic_h : h;
        BltBitMapRastPort(A.pic, (A.pic_w - bw) / 2, (A.pic_h - bh) / 2, rp,
                          x + (w - bw) / 2, y + (h - bh) / 2, bw, bh, 0xC0);
    }
}

static void redraw_shot(void)
{
    olui_rect r;
    olui_el *shot = olui_find(A.ui, "shot");
    if (shot && olui_box(A.ui, shot, &r))
        draw_shot(NULL, shot, A.win->BorderLeft + r.x, A.win->BorderTop + r.y, r.w, r.h);
}

static void cache_name(const char *url, char *out, int max)
{
    char name[32];
    port_picture_name(url, name, (int)sizeof name);
    snprintf(out, (size_t)max, PICTURES "/%s", name);
}

static void host_picture(void *data, const Port *p, int shot)
{
    char path[300], why[160];
    struct BitMapHeader *bmh = NULL;
    BPTR l;
    (void)data;
    /* the same picture again (the window was busy, say): only drawn */
    if (A.dto && p && A.pic_port == p && A.pic_shot == shot) { redraw_shot(); return; }
    drop_picture();
    if (p && shot < p->nshots) {
        cache_name(p->shots[shot], path, (int)sizeof path);
        l = Lock((STRPTR)path, ACCESS_READ);
        if (l) UnLock(l);
        else if (A.offline || failed_before(p->shots[shot])) path[0] = 0;   /* don't stall every 5 s */
        else if (!http_get_file(p->shots[shot], path, NULL, NULL, NULL, why, (int)sizeof why)) {
            failed_now(p->shots[shot]);
            if (strstr(why, "network") || strstr(why, "did not answer") || strstr(why, "TCP/IP")) A.offline = 1;
            path[0] = 0;
        }
        if (path[0])
            A.dto = NewDTObject((APTR)path, DTA_GroupID, GID_PICTURE, PDTA_Remap, TRUE,
                                PDTA_Screen, (ULONG)A.win->WScreen, PDTA_DestMode, PMODE_V43,
                                OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        if (A.dto && GetDTAttrs(A.dto, PDTA_BitMapHeader, (ULONG)&bmh, TAG_DONE) && bmh) {
            A.src_w = bmh->bmh_Width;
            A.src_h = bmh->bmh_Height;
            A.pic_port = p;
            A.pic_shot = shot;
        } else if (A.dto) {
            DisposeDTObject(A.dto);
            A.dto = NULL;
        }
    }
    redraw_shot();
}

/* olui_amiga_images: the list's state marks, a small square in the row. */
static void draw_mark(void *data, struct RastPort *rp, const olui_el *el, int image, int x, int y, int w, int h)
{
    int s = w < h ? w : h;
    (void)data; (void)el;
    if (s > 8) s = 8;
    if (image < MARK_AVAILABLE || image > MARK_ORIGINAL || A.pens[image] < 0) return;
    SetAPen(rp, (ULONG)A.pens[image]);
    RectFill(rp, x + (w - s) / 2, y + (h - s) / 2, x + (w - s) / 2 + s - 1, y + (h - s) / 2 + s - 1);
}

static void get_pens(void)
{
    static const ULONG rgb[MARK_ORIGINAL + 1] = { 0, 0x2E9D5B, 0x3A6EA8, 0xE0A030, 0xB5BEC9, 0xD9A520 };
    struct ColorMap *cm = A.win->WScreen->ViewPort.ColorMap;
    int i;
    A.pens[0] = -1;
    for (i = 1; i <= MARK_ORIGINAL; i++) {
        ULONG r = (rgb[i] >> 16) & 255, g = (rgb[i] >> 8) & 255, b = rgb[i] & 255;
        A.pens[i] = ObtainBestPen(cm, r * 0x01010101UL, g * 0x01010101UL, b * 0x01010101UL,
                                  OBP_Precision, PRECISION_GUI, TAG_DONE);
    }
}

static void free_pens(void)
{
    int i;
    for (i = 1; i <= MARK_ORIGINAL; i++)
        if (A.pens[i] >= 0) ReleasePen(A.win->WScreen->ViewPort.ColorMap, (ULONG)A.pens[i]);
}

/* ---- small things ---- */

static void busy(int on)
{
    SetWindowPointer(A.win, WA_BusyPointer, on ? TRUE : FALSE, TAG_DONE);
    ui_busy(on);
    olui_amiga_draw(A.glue);
}

static LONG ask(const char *text, const char *buttons)
{
    struct EasyStruct es;
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (STRPTR)"AmiPortAtlas";
    es.es_TextFormat = (STRPTR)"%s";
    es.es_GadgetFormat = (STRPTR)buttons;
    return EasyRequest(A.win, &es, NULL, (ULONG)text);
}

static void status(const char *text)
{
    ui_status(text);
    olui_amiga_draw(A.glue);
}

/* ---- the feed ---- */

/* The window's cache drawer is the one Atlas uses too. */
static void share_cache(void)
{
    char path[200];
    BPTR l = Lock((STRPTR)CACHE, ACCESS_READ);
    if (!l) l = CreateDir((STRPTR)CACHE);
    if (!l) return;
    if (NameFromLock(l, (STRPTR)path, sizeof path))
        SetVar((STRPTR)"AmiPortAtlas/Cache", (STRPTR)path, -1, GVF_GLOBAL_ONLY | GVF_SAVE_VAR);
    UnLock(l);
    l = Lock((STRPTR)PICTURES, ACCESS_READ);
    if (!l) {
        /* AmiPortAtlas 0.2.0 and older kept pictures beside the sources'
         * copies, under the URL's last name: those go, once */
        drop_old_pictures();
        l = CreateDir((STRPTR)PICTURES);
    }
    if (l) UnLock(l);
}

static void repo_tell(void *user, const char *line) { (void)user; status(line); }

static const char *host_source_name(void *data, const char *id) { (void)data; return repo_source_name(A.sources, id); }

/* From lists the sources switched on that gave entries. */
static void set_sources(Sources *s)
{
    int i, n = 0;
    const char **ids = s ? (const char **)malloc(sizeof(char *) * (size_t)(s->count + 1)) : NULL;
    const char **names = s ? (const char **)malloc(sizeof(char *) * (size_t)(s->count + 1)) : NULL;
    if (ids && names)
        for (i = 0; i < s->count; i++)
            if (s->list[i].on && s->list[i].count > 0) { ids[n] = s->list[i].id; names[n] = s->list[i].name; n++; }
    ui_set_sources(ids, names, ids && names ? n : 0);
    free(A.from_ids);
    free(A.from_names);
    A.from_ids = ids;
    A.from_names = names;
    sources_free(A.sources);
    A.sources = s;
}

/* The saved pictures deleted, the sources' indexes (.json) kept. In this process, as only it
 * knows where PROGDIR: is; the names first, as ExNext can't run past deletes. */
static int json_name(const char *name)
{
    size_t n = strlen(name);
    return n > 5 && strcmp(name + n - 5, ".json") == 0;
}

/* The names of the files in a drawer, in *names (malloc'd, *n of them). */
static int list_files(const char *dir, char (**names)[40], int *n)
{
    struct FileInfoBlock *fib = (struct FileInfoBlock *)AllocDosObject(DOS_FIB, NULL);
    int cap = 0;
    BPTR l;
    *names = NULL;
    *n = 0;
    if (!fib) return 0;
    if ((l = Lock((STRPTR)dir, ACCESS_READ)) != 0) {
        if (Examine(l, fib))
            while (ExNext(l, fib)) {
                if (fib->fib_DirEntryType >= 0 || strlen((const char *)fib->fib_FileName) >= 40) continue;
                if (*n == cap) {
                    char (*more)[40] = (char (*)[40])realloc(*names, (size_t)(cap ? cap * 2 : 256) * 40);
                    if (!more) break;
                    *names = more;
                    cap = cap ? cap * 2 : 256;
                }
                snprintf((*names)[(*n)++], 40, "%s", (const char *)fib->fib_FileName);
            }
        UnLock(l);
    }
    FreeDosObject(DOS_FIB, fib);
    return 1;
}

static void drop_old_pictures(void)
{
    char (*names)[40], path[160];
    int n, i;
    list_files(CACHE, &names, &n);
    for (i = 0; i < n; i++)
        if (!json_name(names[i])) {
            snprintf(path, sizeof path, CACHE "/%s", names[i]);
            DeleteFile((STRPTR)path);
        }
    free(names);
}

static int by_name(const void *a, const void *b) { return strcmp((const char *)a, (const char *)b); }

/* Saved pictures stay from one Atlas to the next. The ones no source names
 * any more go, so the drawer doesn't grow for ever. Only called when every
 * source was just read, or a source that failed would lose its pictures. */
static void prune_pictures(const Feed *f)
{
    char (*names)[40], (*keep)[40] = NULL, path[160];
    long want = 0, k = 0;
    int n, i, j;
    for (i = 0; i < f->count; i++) want += f->ports[i].nshots;
    if (want > 0 && (keep = (char (*)[40])malloc((size_t)want * 40)) == NULL) return;
    for (i = 0; i < f->count; i++)
        for (j = 0; j < f->ports[i].nshots; j++)
            port_picture_name(f->ports[i].shots[j], keep[k++], 40);
    if (k > 1) qsort(keep, (size_t)k, 40, by_name);
    list_files(PICTURES, &names, &n);
    for (i = 0; i < n; i++) {
        size_t len = strlen(names[i]);
        /* a .part is a fetch that never finished */
        if ((len > 5 && strcmp(names[i] + len - 5, ".part") == 0)
            || !k || !bsearch(names[i], keep, (size_t)k, 40, by_name)) {
            snprintf(path, sizeof path, PICTURES "/%s", names[i]);
            DeleteFile((STRPTR)path);
        }
    }
    free(names);
    free(keep);
}

/* Every source read again (fetch), or the saved copies (at start, before
 * the network is asked). */
static void read_sources(int fetch)
{
    char why[200], line[300];
    Feed *f, *old = A.feed;
    Sources *s = NULL;
    busy(1);
    f = repo_read(fetch, &s, repo_tell, NULL, why, (int)sizeof why);
    installed_load();
    if (f) {
        A.nfailed = 0;
        A.offline = 0;
        drop_picture();         /* its Port goes with the old feed */
        A.feed = f;
        set_sources(s);
        if (fetch && all_fresh()) prune_pictures(f);
        ui_set_feed(f);
        feed_free(old);
        repo_summary(A.sources, line, (int)sizeof line);
        ui_status(line);
    } else {
        sources_free(s);
        ui_status(why);
    }
    busy(0);
}

/* 1 when every source switched on was just read from the network (the
 * first start, with nothing saved yet), so there's no need to again. */
static int all_fresh(void)
{
    int i;
    if (!A.sources) return 0;
    for (i = 0; i < A.sources->count; i++) {
        const Source *x = &A.sources->list[i];
        if (x->on && x->index && x->state != SRC_FRESH) return 0;
    }
    return 1;
}

static void host_refresh(void *data)
{
    (void)data;
    read_sources(1);
}

/* ---- installing ---- */

static const char *host_installed(void *data, const Port *p) { (void)data; return installed_version(p); }

static int progress(void *user, const char *d, long len, long done, long total)
{
    (void)user; (void)d; (void)len;
    if (total > 0) {
        ui_progress((int)(done / (total / 100 + 1)));
        olui_amiga_draw(A.glue);
    }
    return 1;
}

static void step(void *user, int s, long done, long total)
{
    static const char *const text[STEP_COUNT] = { "Downloading %s...", "Checked %s.", "Unpacking %s...",
                                                  "Installing %s...", "Tidying up after %s..." };
    char line[200];
    Port *p = ui_selected();
    (void)user; (void)done; (void)total;
    snprintf(line, sizeof line, text[s], p ? p->title : "");
    status(line);
}

static void default_drawer(char *out, int max)
{
    BPTR l;
    if (GetVar((STRPTR)"AmiPortAtlas/Drawer", (STRPTR)out, max, GVF_GLOBAL_ONLY) > 0) return;
    l = Lock((STRPTR)"Work:Games", ACCESS_READ);
    if (l) { UnLock(l); snprintf(out, (size_t)max, "Work:Games"); return; }
    l = Lock((STRPTR)"Work:", ACCESS_READ);
    if (l) { UnLock(l); snprintf(out, (size_t)max, "Work:"); return; }
    snprintf(out, (size_t)max, "SYS:");
}

static int choose_drawer(char *drawer, int max)
{
    struct FileRequester *fr;
    int ok = 0;
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest, ASLFR_Window, (ULONG)A.win,
            ASLFR_TitleText, (ULONG)"Install into which drawer?", ASLFR_DrawersOnly, TRUE,
            ASLFR_InitialDrawer, (ULONG)drawer, TAG_DONE);
    if (!fr) return 0;
    if (AslRequest(fr, NULL)) {
        /* fr_Drawer may be relative ("" or "Games"): the full name is kept */
        BPTR l = Lock(fr->fr_Drawer, ACCESS_READ);
        snprintf(drawer, (size_t)max, "%s", (const char *)fr->fr_Drawer);
        if (l) { NameFromLock(l, (STRPTR)drawer, max); UnLock(l); }
        SetVar((STRPTR)"AmiPortAtlas/Drawer", (STRPTR)drawer, -1, GVF_GLOBAL_ONLY | GVF_SAVE_VAR);
        ok = 1;
    }
    FreeAslRequest(fr);
    return ok;
}

static void host_install(void *data, Port *p)
{
    char drawer[256], why[240], text[600];
    LONG answer;
    Source *from = sources_find(A.sources, p->source);
    (void)data;
    /* the Team checks what the Port Atlas offers; a source the player added, nobody has */
    if (from && !from->checked) {
        snprintf(text, sizeof text, "%s comes from %s, which The AmiCradle Team doesn't check.\n"
                 "Its installer runs on this Amiga. Install it only if you trust %s.", p->title, from->name, from->name);
        if (ask(text, "Go on|Cancel") != 1) return;
    }
    default_drawer(drawer, (int)sizeof drawer);
    for (;;) {
        if (!install_check(p, drawer, why, (int)sizeof why)) { ask(why, "OK"); return; }
        snprintf(text, sizeof text, "Install %s %s into %s?\n\nThis Amiga has what it needs:\nthe CPU, the modules and the room.",
                 p->title, p->version ? p->version : "", drawer);
        answer = ask(text, "Install|Choose drawer...|Cancel");
        if (answer == 0) return;
        if (answer == 1) break;
        if (!choose_drawer(drawer, (int)sizeof drawer)) return;
    }
    busy(1);
    ui_progress(0);
    if (install_run(p, drawer, step, progress, NULL, why, (int)sizeof why)) {
        snprintf(text, sizeof text, "%s is in %s. Double-click it here or start it from its icon there.", p->title, drawer);
        ui_progress(-1);
        busy(0);
        ui_refill();
        status(text);
        ask(text, "OK");
    } else {
        ui_progress(-1);
        busy(0);
        status(why);
        ask(why, "OK");
    }
}

/* ---- games already on the disks ---- */

static int scan_tell(void *user, const char *where, int found)
{
    char line[200];
    (void)user;
    snprintf(line, sizeof line, "Looking for installed games: %.150s (%d found)", where, found);
    status(line);
    return 1;
}

static void host_scan(void *data)
{
    char line[200];
    Port **list;
    int i, found, bare;
    (void)data;
    if (!A.feed || !A.feed->count) return;
    list = (Port **)malloc(sizeof(Port *) * (size_t)A.feed->count);
    if (!list) return;
    for (i = 0; i < A.feed->count; i++) list[i] = &A.feed->ports[i];
    busy(1);
    found = install_scan(list, A.feed->count, scan_tell, NULL);
    free(list);
    SetVar((STRPTR)"AmiPortAtlas/Scanned", (STRPTR)"1", 1, GVF_GLOBAL_ONLY | GVF_SAVE_VAR);
    busy(0);
    ui_refill();
    if (found < 0) { status("There wasn't the memory to look for installed games."); return; }
    bare = install_missing_icons(0);
    snprintf(line, sizeof line, "Found %d installed game%s on DH0: to DH15:.", found, found == 1 ? "" : "s");
    status(line);
    if (bare > 0) {
        snprintf(line, sizeof line, "%d installed game%s no icon for the drawer or the game.\n"
                 "Give %s Workbench's default icons? Icons already there are left as they are.",
                 bare, bare == 1 ? " has" : "s have", bare == 1 ? "it" : "them");
        if (ask(line, "Give icons|Not now") == 1) bare = install_missing_icons(1);
        else bare = -1;
    }
    if (bare > 0) snprintf(line, sizeof line, "Found %d installed game%s on DH0: to DH15:, and gave %d of them icons.",
                           found, found == 1 ? "" : "s", bare);
    else snprintf(line, sizeof line, "Found %d installed game%s on DH0: to DH15:.", found, found == 1 ? "" : "s");
    status(line);
}

static void host_launch(void *data, Port *p)
{
    char why[240];
    (void)data;
    if (!install_launch(p, why, (int)sizeof why)) ask(why, "OK");
}

static void host_remove(void *data, Port *p)
{
    char why[240], text[300];
    (void)data;
    if (!installed_path(p)) return;
    if (installed_found(p)) {
        snprintf(text, sizeof text, "AmiPortAtlas didn't install the copy in %s, so it leaves it alone.\n"
                 "Delete it from Workbench if you want it gone.", installed_path(p));
        ask(text, "OK");
        return;
    }
    snprintf(text, sizeof text, "Delete %s and everything in it?\nSaved games and settings in it go too.", installed_path(p));
    if (ask(text, "Delete|Cancel") != 1) return;
    if (!install_remove(p, why, (int)sizeof why)) ask(why, "OK");
    ui_refill();
}

static void host_open_url(void *data, const char *url) { (void)data; platform_open_url(url); }

/* ---- the program ---- */

int main(void)
{
    const char *missing = NULL;
    char err[200];
    int done = 0, rc = 20;
    UiHost host;

    memset(&A, 0, sizeof A);
    { int i; for (i = 0; i <= MARK_ORIGINAL; i++) A.pens[i] = -1; }
    feed_utf8 = 1;  /* olui takes UTF-8 */
    if (!platform_open(&missing)) { printf("AmiPortAtlas needs %s.\n", missing); goto out; }
    A.win = OpenWindowTags(NULL, WA_Title, (ULONG)"AmiPortAtlas", WA_ScreenTitle, (ULONG)(vers + 6),
                           WA_Width, 1100, WA_Height, 680, WA_MinWidth, 760, WA_MinHeight, 460,
                           WA_MaxWidth, ~0UL, WA_MaxHeight, ~0UL, WA_AutoAdjust, TRUE,
                           WA_DragBar, TRUE, WA_DepthGadget, TRUE, WA_CloseGadget, TRUE, WA_SizeGadget, TRUE,
                           WA_Activate, TRUE, WA_SimpleRefresh, TRUE, WA_ReportMouse, TRUE,
                           WA_IDCMP, olui_amiga_idcmp(), TAG_DONE);
    if (!A.win) { printf("AmiPortAtlas could not open its window.\n"); goto out; }
    A.ui = olui_new();
    A.glue = A.ui ? olui_amiga_new(A.ui, A.win) : NULL;
    if (!A.glue) { printf("AmiPortAtlas: out of memory.\n"); goto out; }
    olui_amiga_theme(A.glue, NULL);  /* the theme GFOpenPrefs Look chose */

    memset(&host, 0, sizeof host);
    host.installed = host_installed;
    host.picture = host_picture;
    host.install = host_install;
    host.launch = host_launch;
    host.remove = host_remove;
    host.open_url = host_open_url;
    host.refresh = host_refresh;
    host.scan = host_scan;
    host.source_name = host_source_name;
    if (!ui_build(A.ui, &host, err, sizeof err)) { printf("AmiPortAtlas: %s\n", err); goto out; }
    get_pens();
    olui_amiga_own(A.glue, olui_find(A.ui, "shot"), draw_shot, NULL);
    olui_amiga_images(A.glue, draw_mark, NULL);
    olui_amiga_layout(A.glue);

    share_cache();
    installed_load();
    read_sources(0);            /* the saved copies show at once */
    if (!all_fresh()) read_sources(1);
    {
        /* the first start looks for games already installed */
        char seen[4];
        if (GetVar((STRPTR)"AmiPortAtlas/Scanned", (STRPTR)seen, sizeof seen, GVF_GLOBAL_ONLY) < 0) host_scan(NULL);
    }

    while (!done) {
        struct IntuiMessage *m;
        ULONG sig = Wait((1UL << A.win->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C);
        if (sig & SIGBREAKF_CTRL_C) break;
        while ((m = (struct IntuiMessage *)GetMsg(A.win->UserPort)) != NULL) {
            ULONG cls = m->Class;
            int r = olui_amiga_event(A.glue, m);
            ReplyMsg((struct Message *)m);
            if (r == OLUI_AMIGA_CLOSE) done = 1;
            if (cls == IDCMP_INTUITICKS) ui_tick();
        }
        olui_amiga_draw(A.glue);
    }
    rc = 0;
out:
    drop_picture();
    if (A.glue) olui_amiga_free(A.glue);
    if (A.ui) olui_free(A.ui);
    if (A.win) { free_pens(); CloseWindow(A.win); }  /* pens never obtained are -1 */
    feed_free(A.feed);
    sources_free(A.sources);
    free(A.from_ids);
    free(A.from_names);
    platform_close();
    return rc;
}
