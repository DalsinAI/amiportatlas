/* AmiPortAtlas: the program, on GFOpenLayout UI in an Intuition window.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * ui.c describes the window; this file gives it what needs the system:
 * the window and its event loop (olui's Amiga glue), the pictures (through
 * datatypes, scaled into the picture box, drawn by the program), the list's
 * state marks, the feed from the AmiCradle site and installing.
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
#include "ui.h"

#define FEED_URL   "https://www.amicradle.com/ports/ports.json"
#define CACHE      "PROGDIR:Cache"
#define FEED_CACHE CACHE "/ports.json"
#define FEED_MAX   (4L * 1024 * 1024)

static const char vers[] = "$VER: AmiPortAtlas 0.1 (04.11.2026) The AmiCradle Team";

static struct {
    struct Window *win;
    olui *ui;
    olui_amiga *glue;
    Feed *feed;
    LONG pens[MARK_ORIGINAL + 1];
    Object *dto;                 /* the current picture */
    ULONG src_w, src_h;          /* its own size */
    struct BitMap *pic;          /* the datatype's bitmap, scaled to the box */
    int pic_w, pic_h, box_w, box_h;
} A;

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
    const char *base = strrchr(url, '/');
    snprintf(out, (size_t)max, CACHE "/%s", base ? base + 1 : url);
}

static void host_picture(void *data, const Port *p, int shot)
{
    char path[300], why[160];
    struct BitMapHeader *bmh = NULL;
    BPTR l;
    (void)data;
    drop_picture();
    if (p && shot < p->nshots) {
        cache_name(p->shots[shot], path, (int)sizeof path);
        l = Lock((STRPTR)path, ACCESS_READ);
        if (l) UnLock(l);
        else if (!http_get_file(p->shots[shot], path, NULL, NULL, NULL, why, (int)sizeof why)) path[0] = 0;
        if (path[0])
            A.dto = NewDTObject((APTR)path, DTA_GroupID, GID_PICTURE, PDTA_Remap, TRUE,
                                PDTA_Screen, (ULONG)A.win->WScreen, PDTA_DestMode, PMODE_V43,
                                OBP_Precision, PRECISION_IMAGE, TAG_DONE);
        if (A.dto && GetDTAttrs(A.dto, PDTA_BitMapHeader, (ULONG)&bmh, TAG_DONE) && bmh) {
            A.src_w = bmh->bmh_Width;
            A.src_h = bmh->bmh_Height;
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

static Feed *load_cached_feed(void)
{
    BPTR fh = Open((STRPTR)FEED_CACHE, MODE_OLDFILE);
    char *buf;
    const char *why;
    LONG len;
    Feed *f = NULL;
    if (!fh) return NULL;
    buf = (char *)malloc(FEED_MAX);
    if (buf) {
        len = Read(fh, buf, FEED_MAX);
        if (len > 0) f = feed_parse(buf, len, &why);
        free(buf);
    }
    Close(fh);
    return f;
}

static void host_refresh(void *data)
{
    char why[200];
    char *text = NULL;
    long len;
    const char *bad;
    Feed *f = NULL, *old = A.feed;
    BPTR l = CreateDir((STRPTR)CACHE);
    (void)data;
    if (l) UnLock(l);
    busy(1);
    status("Reading the Port Atlas from www.amicradle.com...");
    if (http_get_mem(FEED_URL, FEED_MAX, &text, &len, why, (int)sizeof why)) {
        f = feed_parse(text, len, &bad);
        if (f) {
            BPTR fh = Open((STRPTR)FEED_CACHE, MODE_NEWFILE);
            if (fh) { Write(fh, text, len); Close(fh); }
        } else {
            snprintf(why, sizeof why, "The Port Atlas could not be read: %s", bad);
        }
        free(text);
    }
    installed_load();
    if (f) {
        A.feed = f;
        ui_set_feed(f);
        feed_free(old);
    } else if (!old) {
        A.feed = load_cached_feed();
        if (A.feed) {
            ui_set_feed(A.feed);
            ui_status("Offline: the last saved copy of the Port Atlas.");
        } else {
            ui_status(why);
        }
    } else {
        ui_status(why);
    }
    busy(0);
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
        snprintf(drawer, (size_t)max, "%s", (const char *)fr->fr_Drawer);
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
    (void)data;
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
                           WA_Width, 1000, WA_Height, 620, WA_MinWidth, 760, WA_MinHeight, 460,
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
    if (!ui_build(A.ui, &host, err, sizeof err)) { printf("AmiPortAtlas: %s\n", err); goto out; }
    get_pens();
    olui_amiga_own(A.glue, olui_find(A.ui, "shot"), draw_shot, NULL);
    olui_amiga_images(A.glue, draw_mark, NULL);
    olui_amiga_layout(A.glue);

    installed_load();
    host_refresh(NULL);

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
    platform_close();
    return rc;
}
