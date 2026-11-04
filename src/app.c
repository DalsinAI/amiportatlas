/* AmiPortAtlas: the window. MIT, (c) 2026 The AmiCradle Team.
 *
 * ReAction (window.class, layout.gadget and friends): built into AmigaOS
 * 3.2.3 and 4.1, GFOpenReAction (GFOpenLayout and the other open classes) on
 * GFOS and AROS. The layout follows the approved mockup: the selected game's
 * pictures on the left, cycling; the Atlas list on the right; the buttons
 * bottom right. docs/DESIGN.md has the details.
 */
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/var.h>
#include <intuition/intuition.h>
#include <intuition/gadgetclass.h>
#include <intuition/icclass.h>
#include <graphics/gfx.h>
#include <graphics/scale.h>
#include <datatypes/pictureclass.h>
#include <libraries/asl.h>
#include <classes/window.h>
#include <gadgets/layout.h>
#include <gadgets/listbrowser.h>
#include <gadgets/chooser.h>
#include <gadgets/string.h>
#include <gadgets/button.h>
#include <gadgets/fuelgauge.h>
#include <gadgets/space.h>
#include <images/label.h>
#include <reaction/reaction_macros.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/datatypes.h>
#include <proto/asl.h>
#include <proto/listbrowser.h>

#include "feed.h"
#include "http.h"
#include "install.h"

#define FEED_URL   "https://www.amicradle.com/ports/ports.json"
#define CACHE      "PROGDIR:Cache"
#define FEED_CACHE CACHE "/ports.json"
#define FEED_MAX   (4L * 1024 * 1024)
#define CYCLE_TICKS 50            /* IntuiTicks come ten a second: 5 s a picture */

static const char vers[] = "$VER: AmiPortAtlas 0.1 (04.11.2026) The AmiCradle Team";

/* ---- classes ---- */

struct Library *WindowBase, *LayoutBase, *ListBrowserBase, *ChooserBase, *StringBase,
               *ButtonBase, *LabelBase, *FuelGaugeBase, *SpaceBase;
#ifdef __amigaos4__
struct ListBrowserIFace *IListBrowser;
#endif

static int open_classes(const char **missing)
{
#define CL(base, name) if (!(base = OpenLibrary(name, 44))) { *missing = name; return 0; }
    CL(WindowBase, "window.class");
    CL(LayoutBase, "gadgets/layout.gadget");
    CL(ListBrowserBase, "gadgets/listbrowser.gadget");
    CL(ChooserBase, "gadgets/chooser.gadget");
    CL(StringBase, "gadgets/string.gadget");
    CL(ButtonBase, "gadgets/button.gadget");
    CL(LabelBase, "images/label.image");
    CL(FuelGaugeBase, "gadgets/fuelgauge.gadget");
    CL(SpaceBase, "gadgets/space.gadget");
#undef CL
#ifdef __amigaos4__
    IListBrowser = (struct ListBrowserIFace *)GetInterface(ListBrowserBase, "main", 1, NULL);
    if (!IListBrowser) { *missing = "listbrowser.gadget interface"; return 0; }
#endif
    return 1;
}

static void close_classes(void)
{
#ifdef __amigaos4__
    if (IListBrowser) DropInterface((struct Interface *)IListBrowser);
#endif
    if (SpaceBase) CloseLibrary(SpaceBase);
    if (FuelGaugeBase) CloseLibrary(FuelGaugeBase);
    if (LabelBase) CloseLibrary(LabelBase);
    if (ButtonBase) CloseLibrary(ButtonBase);
    if (StringBase) CloseLibrary(StringBase);
    if (ChooserBase) CloseLibrary(ChooserBase);
    if (ListBrowserBase) CloseLibrary(ListBrowserBase);
    if (LayoutBase) CloseLibrary(LayoutBase);
    if (WindowBase) CloseLibrary(WindowBase);
}

/* ---- state ---- */

enum { G_FIND = 1, G_SHOW, G_FIT, G_SORT, G_UPDATES, G_LIST, G_PICTURE, G_PREV, G_NEXT,
       G_INSTALL, G_RELEASE, G_REMOVE, G_STATUS, G_GAUGE, G_FACTS };

enum { SHOW_ALL, SHOW_AVAILABLE, SHOW_OURS, SHOW_RELEASED, SHOW_TESTING, SHOW_PROGRESS,
       SHOW_INSTALLED, SHOW_UPDATES };
static STRPTR show_labels[] = { (STRPTR)"All ports", (STRPTR)"Available", (STRPTR)"Our games",
    (STRPTR)"Released", (STRPTR)"Testing", (STRPTR)"In progress", (STRPTR)"Installed",
    (STRPTR)"Updates", NULL };
static STRPTR fit_labels[] = { (STRPTR)"Any", (STRPTR)"040", (STRPTR)"060", (STRPTR)"GPU", NULL };
static STRPTR sort_labels[] = { (STRPTR)"Number", (STRPTR)"Name", (STRPTR)"Year", (STRPTR)"State",
    (STRPTR)"Fit", NULL };

/* List columns, and the sort key a title click on each gives (-1: none). */
static struct ColumnInfo columns[] = {
    { 6, (STRPTR)"#", 0 }, { 40, (STRPTR)"Name", 0 }, { 8, (STRPTR)"Year", 0 },
    { 14, (STRPTR)"State", 0 }, { 6, (STRPTR)"Fit", 0 }, { 12, (STRPTR)"Game data", 0 },
    { 10, (STRPTR)"Installed", 0 }, { -1, (STRPTR)~0, (ULONG)-1 }
};
static struct ColumnInfo fact_columns[] = {
    { 30, (STRPTR)"", 0 }, { 70, (STRPTR)"", 0 }, { -1, (STRPTR)~0, (ULONG)-1 }
};
static const int column_sort[] = { SORT_NUMBER, SORT_NAME, SORT_YEAR, SORT_STATE, SORT_FIT, -1, -1 };
static char column_title[7][20];

static struct {
    Object *win;
    struct Window *w;
    struct Screen *scr;
    Object *g[G_FACTS + 1];
    struct List rows, facts;
    LONG grey_pen;
    Feed *feed;
    Port **shown;
    int nshown;
    int show, fit, sort, sort_desc;
    char find[64];
    Port *sel;
    int shot, ticks;
    struct BitMap *pic;          /* the current picture, scaled to the box */
    UWORD pic_w, pic_h;
    struct Hook render_hook;
    ULONG click_s, click_m;      /* last click on the picture, for double-clicks */
    char status[160];
} A;

/* ---- small helpers ---- */

static void set(int id, ULONG tag, ULONG value)
{
    if (A.g[id]) SetGadgetAttrs((struct Gadget *)A.g[id], A.w, NULL, tag, value, TAG_DONE);
}

static void status(const char *text)
{
    strncpy(A.status, text, sizeof A.status - 1);
    A.status[sizeof A.status - 1] = 0;
    set(G_STATUS, GA_Text, (ULONG)A.status);
}

static void busy(int on)
{
    if (A.win) SetAttrs(A.win, WA_BusyPointer, on ? TRUE : FALSE, TAG_DONE);
}

static void tell(const char *title, const char *text)
{
    struct EasyStruct es;
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (STRPTR)title;
    es.es_TextFormat = (STRPTR)"%s";
    es.es_GadgetFormat = (STRPTR)"OK";
    EasyRequest(A.w, &es, NULL, (ULONG)text);
}

static void cache_name(const char *url, char *out, int max)
{
    const char *base = strrchr(url, '/');
    snprintf(out, (size_t)max, CACHE "/%s", base ? base + 1 : url);
}

static int is_update(const Port *p)
{
    const char *v = installed_version(p);
    return v && p->version && port_installable(p) && strcmp(v, p->version) != 0;
}

/* ---- the feed ---- */

static Feed *load_feed_file(const char *path, const char **why)
{
    BPTR fh = Open((STRPTR)path, MODE_OLDFILE);
    char *buf;
    LONG len;
    Feed *f = NULL;
    if (!fh) { *why = "no copy of the Port Atlas is saved yet"; return NULL; }
    buf = (char *)malloc(FEED_MAX);
    if (buf) {
        len = Read(fh, buf, FEED_MAX);
        if (len > 0) f = feed_parse(buf, len, why);
        free(buf);
    }
    Close(fh);
    return f;
}

static void fetch_feed(void)
{
    char why[200];
    char *text;
    long len;
    const char *bad;
    Feed *f;
    BPTR l = CreateDir((STRPTR)CACHE);
    if (l) UnLock(l);
    busy(1);
    status("Reading the Port Atlas from www.amicradle.com...");
    if (http_get_mem(FEED_URL, FEED_MAX, &text, &len, why, (int)sizeof why)) {
        f = feed_parse(text, len, &bad);
        if (f) {
            BPTR fh = Open((STRPTR)FEED_CACHE, MODE_NEWFILE);
            if (fh) { Write(fh, text, len); Close(fh); }
            feed_free(A.feed);
            A.feed = f;
        } else {
            snprintf(why, sizeof why, "The Port Atlas could not be read: %s", bad);
        }
        free(text);
    }
    if (!A.feed) {
        A.feed = load_feed_file(FEED_CACHE, &bad);
        if (A.feed) status("Offline: showing the last saved copy of the Port Atlas.");
        else status(why);
    }
    busy(0);
}

/* ---- the list ---- */

static int passes(const Port *p)
{
    if (A.find[0]) {
        /* case-blind substring */
        const char *t = p->title;
        size_t n = strlen(A.find);
        int hit = 0;
        for (; *t && !hit; t++) {
            size_t i;
            for (i = 0; i < n; i++) {
                char a = t[i], b = A.find[i];
                if (!a) break;
                if (a >= 'A' && a <= 'Z') a += 32;
                if (b >= 'A' && b <= 'Z') b += 32;
                if (a != b) break;
            }
            hit = i == n;
        }
        if (!hit) return 0;
    }
    if (A.fit && (!p->fit || strcmp(p->fit, (const char *)fit_labels[A.fit]) != 0)) return 0;
    switch (A.show) {
    case SHOW_AVAILABLE: return port_installable(p);
    case SHOW_OURS:      return port_is_original(p);
    case SHOW_RELEASED:  return p->state && strcmp(p->state, "released") == 0;
    case SHOW_TESTING:   return p->state && strcmp(p->state, "testing") == 0;
    case SHOW_PROGRESS:  return p->state && strcmp(p->state, "in_progress") == 0;
    case SHOW_INSTALLED: return installed_version(p) != NULL;
    case SHOW_UPDATES:   return is_update(p);
    default:             return 1;
    }
}

static void titles(void)
{
    int c;
    for (c = 0; c < 7; c++) {
        static const char *base[] = { "#", "Name", "Year", "State", "Fit", "Game data", "Installed" };
        if (column_sort[c] == A.sort)
            snprintf(column_title[c], sizeof column_title[c], "%s %s", base[c], A.sort_desc ? "v" : "^");
        else
            snprintf(column_title[c], sizeof column_title[c], "%s", base[c]);
        columns[c].ci_Title = (STRPTR)column_title[c];
    }
}

static void fill_list(void)
{
    struct Node *n;
    int i, available = 0;
    char num[12], year[8], title[160];

    set(G_LIST, LISTBROWSER_Labels, (ULONG)~0);
    FreeListBrowserList(&A.rows);
    free(A.shown);
    A.shown = NULL;
    A.nshown = 0;
    if (!A.feed) { set(G_LIST, LISTBROWSER_Labels, (ULONG)&A.rows); return; }

    A.shown = (Port **)malloc((size_t)A.feed->count * sizeof(Port *));
    if (!A.shown) return;
    for (i = 0; i < A.feed->count; i++)
        if (passes(&A.feed->ports[i])) A.shown[A.nshown++] = &A.feed->ports[i];
    feed_sort(A.shown, A.nshown, A.sort, A.sort_desc);

    for (i = 0; i < A.nshown; i++) {
        Port *p = A.shown[i];
        const char *inst = installed_version(p);
        int can = port_installable(p) || inst;
        ULONG pen = can ? (ULONG)~0 : (ULONG)A.grey_pen;
        if (port_installable(p)) available++;
        snprintf(num, sizeof num, "%ld", p->number);
        snprintf(year, sizeof year, "%.4s", p->year ? p->year : "");
        snprintf(title, sizeof title, "%s%s", p->title, port_is_original(p) ? " (AmiCradle)" : "");
        /* Each cell copies its text, so the buffers can be reused. */
        n = AllocListBrowserNode(7,
            LBNA_UserData, (ULONG)p,
            LBNA_Flags, (ULONG)(can || A.grey_pen < 0 ? 0 : LBFLG_CUSTOMPENS),
            LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, (ULONG)num, LBNCA_Justification, LCJ_RIGHT, LBNCA_FGPen, pen,
            LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, (ULONG)title, LBNCA_FGPen, pen,
            LBNA_Column, 2, LBNCA_CopyText, TRUE, LBNCA_Text, (ULONG)year, LBNCA_FGPen, pen,
            LBNA_Column, 3, LBNCA_Text, (ULONG)(is_update(p) ? "Update" : port_state_text(p)), LBNCA_FGPen, pen,
            LBNA_Column, 4, LBNCA_CopyText, TRUE, LBNCA_Text, (ULONG)(p->fit ? p->fit : ""), LBNCA_FGPen, pen,
            LBNA_Column, 5, LBNCA_CopyText, TRUE, LBNCA_Text, (ULONG)(p->game_data ? p->game_data : ""), LBNCA_FGPen, pen,
            LBNA_Column, 6, LBNCA_CopyText, TRUE, LBNCA_Text, (ULONG)(inst ? inst : ""), LBNCA_FGPen, pen,
            TAG_DONE);
        if (n) AddTail(&A.rows, n);
    }
    titles();
    set(G_LIST, LISTBROWSER_ColumnInfo, (ULONG)columns);
    set(G_LIST, LISTBROWSER_Labels, (ULONG)&A.rows);
    snprintf(title, sizeof title, "Port Atlas of %s: %d of %d shown, %d to install",
             A.feed->generated ? A.feed->generated : "?", A.nshown, A.feed->count, available);
    status(title);
}

/* ---- the picture ---- */

static void drop_picture(void)
{
    if (A.pic) { WaitBlit(); FreeBitMap(A.pic); }
    A.pic = NULL;
}

/* Loads picture k of the selected game (fetching it into the cache once),
 * scaled to fit the picture box. */
static void load_picture(int k)
{
    char path[300], why[160];
    Object *o;
    struct BitMap *bm = NULL;
    struct BitMapHeader *bmh = NULL;
    struct IBox *box = NULL;
    BPTR l;

    drop_picture();
    if (!A.sel || A.sel->nshots == 0) return;
    A.shot = k % A.sel->nshots;
    cache_name(A.sel->shots[A.shot], path, (int)sizeof path);
    l = Lock((STRPTR)path, ACCESS_READ);
    if (l) UnLock(l);
    else if (!http_get_file(A.sel->shots[A.shot], path, NULL, NULL, NULL, why, (int)sizeof why)) return;

    o = NewDTObject((APTR)path, DTA_GroupID, GID_PICTURE, PDTA_Remap, TRUE, PDTA_Screen, (ULONG)A.scr,
                    PDTA_DestMode, PMODE_V43, OBP_Precision, PRECISION_IMAGE, TAG_DONE);
    if (!o) return;
    if (DoDTMethod(o, NULL, NULL, DTM_PROCLAYOUT, NULL, TRUE))
        GetDTAttrs(o, PDTA_DestBitMap, (ULONG)&bm, PDTA_BitMapHeader, (ULONG)&bmh, TAG_DONE);
    GetAttr(SPACE_AreaBox, A.g[G_PICTURE], (ULONG *)&box);
    if (bm && bmh && box && box->Width > 0 && box->Height > 0) {
        /* Fit inside the box, keeping the shape. */
        ULONG sw = bmh->bmh_Width, sh = bmh->bmh_Height, dw = (ULONG)box->Width, dh = sh * dw / sw;
        if (dh > (ULONG)box->Height) { dh = (ULONG)box->Height; dw = sw * dh / sh; }
        A.pic = AllocBitMap(dw, dh, GetBitMapAttr(bm, BMA_DEPTH), BMF_CLEAR, A.scr->RastPort.BitMap);
        if (A.pic) {
            struct BitScaleArgs bsa;
            memset(&bsa, 0, sizeof bsa);
            bsa.bsa_SrcWidth = (UWORD)sw;   bsa.bsa_SrcHeight = (UWORD)sh;
            bsa.bsa_XSrcFactor = (UWORD)sw; bsa.bsa_YSrcFactor = (UWORD)sh;
            bsa.bsa_XDestFactor = (UWORD)dw; bsa.bsa_YDestFactor = (UWORD)dh;
            bsa.bsa_SrcBitMap = bm;
            bsa.bsa_DestBitMap = A.pic;
            BitMapScale(&bsa);
            A.pic_w = (UWORD)dw;
            A.pic_h = (UWORD)dh;
        }
    }
    DisposeDTObject(o);
}

static ULONG render_picture(struct Hook *h, APTR obj, APTR msg)
{
    struct gpRender *gpr = (struct gpRender *)msg;
    struct IBox *box = NULL;
    struct RastPort *rp = gpr->gpr_RPort;
    (void)h;
    GetAttr(SPACE_AreaBox, (Object *)obj, (ULONG *)&box);
    if (!box || !rp) return 0;
    SetAPen(rp, 1);  /* black behind the picture, as in the mockup */
    RectFill(rp, box->Left, box->Top, box->Left + box->Width - 1, box->Top + box->Height - 1);
    if (A.pic)
        BltBitMapRastPort(A.pic, 0, 0, rp, box->Left + (box->Width - A.pic_w) / 2,
                          box->Top + (box->Height - A.pic_h) / 2, A.pic_w, A.pic_h, 0xC0);
    return 0;
}

static void show_picture(int k)
{
    load_picture(k);
    if (A.w) RefreshGList((struct Gadget *)A.g[G_PICTURE], A.w, NULL, 1);
}

/* ---- the selected game ---- */

static void add_fact(const char *what, const char *text)
{
    struct Node *n;
    if (!text || !text[0]) return;
    n = AllocListBrowserNode(2, LBNA_Column, 0, LBNCA_CopyText, TRUE, LBNCA_Text, (ULONG)what,
                             LBNA_Column, 1, LBNCA_CopyText, TRUE, LBNCA_Text, (ULONG)text, TAG_DONE);
    if (n) AddTail(&A.facts, n);
}

static void select_port(Port *p)
{
    char line[200];
    const char *inst;
    int i;
    A.sel = p;
    A.ticks = 0;
    set(G_FACTS, LISTBROWSER_Labels, (ULONG)~0);
    FreeListBrowserList(&A.facts);
    if (p) {
        inst = installed_version(p);
        if (p->version) { snprintf(line, sizeof line, "%s  (%s)", p->version, port_state_text(p)); add_fact("Version", line); }
        else add_fact("State", port_state_text(p));
        if (inst) add_fact("Installed", inst);
        add_fact("Year", p->year);
        add_fact("Display", p->display);
        if (p->cpu) { snprintf(line, sizeof line, "%s%s (Fit %s)", p->cpu, p->fpu ? " with FPU" : "", p->fit ? p->fit : "?"); add_fact("Runs on", line); }
        else if (p->fit) add_fact("Fit", p->fit);
        line[0] = 0;
        for (i = 0; i < p->nrequires; i++) {
            if (i) strncat(line, ", ", sizeof line - strlen(line) - 1);
            strncat(line, p->requires[i], sizeof line - strlen(line) - 1);
        }
        add_fact("Tested on", line[0] ? line : "Not yet tested");
        line[0] = 0;
        for (i = 0; i < p->nmodules; i++) {
            if (i) strncat(line, ", ", sizeof line - strlen(line) - 1);
            strncat(line, p->modules[i], sizeof line - strlen(line) - 1);
        }
        add_fact("Needs", line);
        if (p->download_size > 0 || p->space > 0) {
            snprintf(line, sizeof line, "%ld KB download, %ld KB installed", (p->download_size + 1023) / 1024, (p->space + 1023) / 1024);
            add_fact("Size", line);
        }
        add_fact("Game data", p->game_data);
        add_fact("Release", p->release_url ? p->release_url : p->port_repo);
    }
    set(G_FACTS, LISTBROWSER_Labels, (ULONG)&A.facts);
    if (p) {
        inst = installed_version(p);
        set(G_INSTALL, GA_Text, (ULONG)(is_update(p) ? "Update..." : inst ? "Play" : "Install..."));
        set(G_INSTALL, GA_Disabled, (ULONG)!(inst || port_installable(p)));
        set(G_REMOVE, GA_Disabled, (ULONG)(inst == NULL));
        set(G_RELEASE, GA_Disabled, (ULONG)!(p->release_url || p->port_repo));
        set(G_PREV, GA_Disabled, (ULONG)(p->nshots < 2));
        set(G_NEXT, GA_Disabled, (ULONG)(p->nshots < 2));
        SetAttrs(A.win, WA_Title, (ULONG)p->title, TAG_DONE);
    }
    show_picture(0);
}

/* ---- actions ---- */

static int progress(void *user, const char *data, long len, long done, long total)
{
    (void)user; (void)data; (void)len;
    if (total > 0) {
        set(G_GAUGE, FUELGAUGE_Max, (ULONG)total);
        set(G_GAUGE, FUELGAUGE_Level, (ULONG)done);
    }
    return 1;
}

static void step(void *user, int s, long done, long total)
{
    static const char *text[STEP_COUNT] = { "Downloading %s...", "Checking %s...", "Unpacking %s...",
                                            "Installing %s...", "Tidying up after %s..." };
    char line[160];
    (void)user; (void)done; (void)total;
    snprintf(line, sizeof line, text[s], A.sel->title);
    status(line);
    if (s != STEP_DOWNLOAD) set(G_GAUGE, FUELGAUGE_Level, 0);
}

static void default_parent(char *out, int max)
{
    if (GetVar((STRPTR)"AmiPortAtlas/Drawer", (STRPTR)out, max, GVF_GLOBAL_ONLY) > 0) return;
    {
        BPTR l = Lock((STRPTR)"Work:Games", ACCESS_READ);
        if (l) { UnLock(l); snprintf(out, (size_t)max, "Work:Games"); return; }
        l = Lock((STRPTR)"Work:", ACCESS_READ);
        if (l) { UnLock(l); snprintf(out, (size_t)max, "Work:"); return; }
    }
    snprintf(out, (size_t)max, "SYS:");
}

static int choose_drawer(char *parent, int max)
{
    struct FileRequester *fr;
    int ok = 0;
    fr = (struct FileRequester *)AllocAslRequestTags(ASL_FileRequest, ASLFR_Window, (ULONG)A.w,
            ASLFR_TitleText, (ULONG)"Install into which drawer?", ASLFR_DrawersOnly, TRUE,
            ASLFR_InitialDrawer, (ULONG)parent, TAG_DONE);
    if (!fr) return 0;
    if (AslRequest(fr, NULL)) {
        snprintf(parent, (size_t)max, "%s", fr->fr_Drawer);
        SetVar((STRPTR)"AmiPortAtlas/Drawer", (STRPTR)parent, -1, GVF_GLOBAL_ONLY | GVF_SAVE_VAR);
        ok = 1;
    }
    FreeAslRequest(fr);
    return ok;
}

static void do_install(void)
{
    char parent[256], why[240], text[600];
    struct EasyStruct es;
    LONG answer;
    Port *p = A.sel;
    if (!p) return;
    if (installed_version(p) && !is_update(p)) {
        if (!install_launch(p, why, (int)sizeof why)) tell("AmiPortAtlas", why);
        return;
    }
    default_parent(parent, (int)sizeof parent);
    for (;;) {
        if (!install_check(p, parent, why, (int)sizeof why)) { tell("AmiPortAtlas", why); return; }
        snprintf(text, sizeof text, "Install %s %s?\n\nIt checked out: the CPU, the modules it needs\nand the room on %s.\n\nInto: %s",
                 p->title, p->version ? p->version : "", parent, parent);
        es.es_StructSize = sizeof es;
        es.es_Flags = 0;
        es.es_Title = (STRPTR)"AmiPortAtlas";
        es.es_TextFormat = (STRPTR)"%s";
        es.es_GadgetFormat = (STRPTR)"Install|Choose drawer...|Cancel";
        answer = EasyRequest(A.w, &es, NULL, (ULONG)text);
        if (answer == 0) return;
        if (answer == 1) break;
        if (!choose_drawer(parent, (int)sizeof parent)) return;
    }
    busy(1);
    if (install_run(p, parent, step, progress, NULL, why, (int)sizeof why)) {
        busy(0);
        snprintf(text, sizeof text, "%s is in %s. Start it from its icon there, or double-click it here.", p->title, parent);
        status(text);
        tell("AmiPortAtlas", text);
        fill_list();
        select_port(p);
    } else {
        busy(0);
        status(why);
        tell("AmiPortAtlas", why);
    }
    set(G_GAUGE, FUELGAUGE_Level, 0);
}

static void do_remove(void)
{
    char why[240], text[300];
    struct EasyStruct es;
    Port *p = A.sel;
    if (!p || !installed_path(p)) return;
    snprintf(text, sizeof text, "Delete %s and everything in it?\nSaved games and settings in it go too.", installed_path(p));
    es.es_StructSize = sizeof es;
    es.es_Flags = 0;
    es.es_Title = (STRPTR)"AmiPortAtlas";
    es.es_TextFormat = (STRPTR)"%s";
    es.es_GadgetFormat = (STRPTR)"Delete|Cancel";
    if (EasyRequest(A.w, &es, NULL, (ULONG)text) != 1) return;
    if (!install_remove(p, why, (int)sizeof why)) tell("AmiPortAtlas", why);
    fill_list();
    select_port(p);
}

static void launch_selected(void)
{
    char why[240];
    if (A.sel && installed_version(A.sel) && !install_launch(A.sel, why, (int)sizeof why))
        tell("AmiPortAtlas", why);
}

/* ---- the window ---- */

static Object *make_window(void)
{
    platform_hook(&A.render_hook, render_picture, NULL);

    return NewObject(NULL, "window.class",
        WA_Title, (ULONG)"AmiPortAtlas",
        WA_ScreenTitle, (ULONG)(vers + 6),
        WA_Activate, TRUE, WA_DragBar, TRUE, WA_CloseGadget, TRUE, WA_DepthGadget, TRUE,
        WA_SizeGadget, TRUE, WA_Width, 900, WA_Height, 560,
        WA_IDCMP, IDCMP_INTUITICKS,
        WINDOW_Position, WPOS_CENTERSCREEN,
        WINDOW_IconifyGadget, TRUE,
        WINDOW_Layout, (ULONG)NewObject(NULL, "layout.gadget",
            LAYOUT_Orientation, LAYOUT_ORIENT_VERT, LAYOUT_SpaceOuter, TRUE, LAYOUT_DeferLayout, TRUE,

            /* the top row: find, show, fit, sort and the update check */
            LAYOUT_AddChild, (ULONG)NewObject(NULL, "layout.gadget", LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                LAYOUT_AddChild, (ULONG)(A.g[G_FIND] = NewObject(NULL, "string.gadget", GA_ID, G_FIND,
                    GA_RelVerify, TRUE, STRINGA_MaxChars, sizeof A.find - 1, TAG_DONE)),
                CHILD_Label, (ULONG)NewObject(NULL, "label.image", LABEL_Text, (ULONG)"_Find", TAG_DONE),
                CHILD_WeightedWidth, 30,
                LAYOUT_AddChild, (ULONG)(A.g[G_SHOW] = NewObject(NULL, "chooser.gadget", GA_ID, G_SHOW,
                    GA_RelVerify, TRUE, CHOOSER_PopUp, TRUE, CHOOSER_LabelArray, (ULONG)show_labels, TAG_DONE)),
                CHILD_Label, (ULONG)NewObject(NULL, "label.image", LABEL_Text, (ULONG)"_Show", TAG_DONE),
                LAYOUT_AddChild, (ULONG)(A.g[G_FIT] = NewObject(NULL, "chooser.gadget", GA_ID, G_FIT,
                    GA_RelVerify, TRUE, CHOOSER_PopUp, TRUE, CHOOSER_LabelArray, (ULONG)fit_labels, TAG_DONE)),
                CHILD_Label, (ULONG)NewObject(NULL, "label.image", LABEL_Text, (ULONG)"F_it", TAG_DONE),
                LAYOUT_AddChild, (ULONG)(A.g[G_SORT] = NewObject(NULL, "chooser.gadget", GA_ID, G_SORT,
                    GA_RelVerify, TRUE, CHOOSER_PopUp, TRUE, CHOOSER_LabelArray, (ULONG)sort_labels, TAG_DONE)),
                CHILD_Label, (ULONG)NewObject(NULL, "label.image", LABEL_Text, (ULONG)"S_ort by", TAG_DONE),
                LAYOUT_AddChild, (ULONG)(A.g[G_UPDATES] = NewObject(NULL, "button.gadget", GA_ID, G_UPDATES,
                    GA_RelVerify, TRUE, GA_Text, (ULONG)"Check for _updates", TAG_DONE)),
                CHILD_WeightedWidth, 0,
                TAG_DONE),
            CHILD_WeightedHeight, 0,

            /* the middle: pictures and facts on the left, the list on the right */
            LAYOUT_AddChild, (ULONG)NewObject(NULL, "layout.gadget", LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                LAYOUT_AddChild, (ULONG)NewObject(NULL, "layout.gadget", LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                    LAYOUT_BevelStyle, BVS_GROUP, LAYOUT_SpaceOuter, TRUE,
                    LAYOUT_AddChild, (ULONG)(A.g[G_PICTURE] = NewObject(NULL, "space.gadget", GA_ID, G_PICTURE,
                        GA_RelVerify, TRUE, SPACE_MinWidth, 320, SPACE_MinHeight, 214,
                        SPACE_RenderHook, (ULONG)&A.render_hook, TAG_DONE)),
                    LAYOUT_AddChild, (ULONG)NewObject(NULL, "layout.gadget", LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                        LAYOUT_HorizAlignment, LALIGN_CENTER,
                        LAYOUT_AddChild, (ULONG)(A.g[G_PREV] = NewObject(NULL, "button.gadget", GA_ID, G_PREV,
                            GA_RelVerify, TRUE, GA_Text, (ULONG)"<", TAG_DONE)),
                        CHILD_WeightedWidth, 0,
                        LAYOUT_AddChild, (ULONG)(A.g[G_NEXT] = NewObject(NULL, "button.gadget", GA_ID, G_NEXT,
                            GA_RelVerify, TRUE, GA_Text, (ULONG)">", TAG_DONE)),
                        CHILD_WeightedWidth, 0,
                        TAG_DONE),
                    CHILD_WeightedHeight, 0,
                    LAYOUT_AddChild, (ULONG)(A.g[G_FACTS] = NewObject(NULL, "listbrowser.gadget", GA_ID, G_FACTS,
                        GA_ReadOnly, TRUE, LISTBROWSER_ShowSelected, FALSE, LISTBROWSER_Labels, (ULONG)&A.facts,
                        LISTBROWSER_ColumnInfo, (ULONG)fact_columns, LISTBROWSER_AutoFit, TRUE,
                        LISTBROWSER_HorizontalProp, FALSE, TAG_DONE)),
                    TAG_DONE),
                CHILD_WeightedWidth, 40,
                LAYOUT_AddChild, (ULONG)NewObject(NULL, "layout.gadget", LAYOUT_Orientation, LAYOUT_ORIENT_VERT,
                    LAYOUT_AddChild, (ULONG)(A.g[G_LIST] = NewObject(NULL, "listbrowser.gadget", GA_ID, G_LIST,
                        GA_RelVerify, TRUE, LISTBROWSER_ColumnInfo, (ULONG)columns, LISTBROWSER_ColumnTitles, TRUE,
                        LISTBROWSER_TitleClickable, TRUE, LISTBROWSER_ShowSelected, TRUE,
                        LISTBROWSER_Labels, (ULONG)&A.rows, LISTBROWSER_Separators, TRUE, TAG_DONE)),
                    /* the buttons, bottom right: what is read last */
                    LAYOUT_AddChild, (ULONG)NewObject(NULL, "layout.gadget", LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                        LAYOUT_HorizAlignment, LALIGN_RIGHT, LAYOUT_EvenSize, TRUE,
                        LAYOUT_AddChild, (ULONG)(A.g[G_INSTALL] = NewObject(NULL, "button.gadget", GA_ID, G_INSTALL,
                            GA_RelVerify, TRUE, GA_Text, (ULONG)"_Install...", GA_Disabled, TRUE, TAG_DONE)),
                        LAYOUT_AddChild, (ULONG)(A.g[G_RELEASE] = NewObject(NULL, "button.gadget", GA_ID, G_RELEASE,
                            GA_RelVerify, TRUE, GA_Text, (ULONG)"_Release page", GA_Disabled, TRUE, TAG_DONE)),
                        LAYOUT_AddChild, (ULONG)(A.g[G_REMOVE] = NewObject(NULL, "button.gadget", GA_ID, G_REMOVE,
                            GA_RelVerify, TRUE, GA_Text, (ULONG)"Re_move", GA_Disabled, TRUE, TAG_DONE)),
                        TAG_DONE),
                    CHILD_WeightedHeight, 0,
                    TAG_DONE),
                CHILD_WeightedWidth, 60,
                TAG_DONE),

            /* the status line and the download gauge */
            LAYOUT_AddChild, (ULONG)NewObject(NULL, "layout.gadget", LAYOUT_Orientation, LAYOUT_ORIENT_HORIZ,
                LAYOUT_AddChild, (ULONG)(A.g[G_STATUS] = NewObject(NULL, "button.gadget", GA_ID, G_STATUS,
                    GA_ReadOnly, TRUE, GA_Text, (ULONG)"", BUTTON_BevelStyle, BVS_NONE,
                    BUTTON_Justification, BCJ_LEFT, TAG_DONE)),
                LAYOUT_AddChild, (ULONG)(A.g[G_GAUGE] = NewObject(NULL, "fuelgauge.gadget", GA_ID, G_GAUGE,
                    FUELGAUGE_Min, 0, FUELGAUGE_Max, 100, FUELGAUGE_Level, 0, FUELGAUGE_Percent, TRUE,
                    FUELGAUGE_Ticks, 0, TAG_DONE)),
                CHILD_WeightedWidth, 0, CHILD_MinWidth, 160,
                TAG_DONE),
            CHILD_WeightedHeight, 0,
            TAG_DONE),
        TAG_DONE);
}

static void list_event(void)
{
    ULONG event = 0, col = 0;
    struct Node *n = NULL;
    Port *p = NULL;
    GetAttr(LISTBROWSER_RelEvent, A.g[G_LIST], &event);
    if (event & LBRE_TITLECLICK) {
        GetAttr(LISTBROWSER_RelColumn, A.g[G_LIST], &col);
        if (col < 7 && column_sort[col] >= 0) {
            if (A.sort == column_sort[col]) A.sort_desc = !A.sort_desc;
            else { A.sort = column_sort[col]; A.sort_desc = 0; }
            set(G_SORT, CHOOSER_Selected, (ULONG)A.sort);
            fill_list();
        }
        return;
    }
    GetAttr(LISTBROWSER_SelectedNode, A.g[G_LIST], (ULONG *)&n);
    if (n) GetListBrowserNodeAttrs(n, LBNA_UserData, (ULONG)&p, TAG_DONE);
    if (p && p != A.sel) select_port(p);
    if (event & LBRE_DOUBLECLICK) launch_selected();
}

static void check_find(void)
{
    STRPTR text = NULL;
    GetAttr(STRINGA_TextVal, A.g[G_FIND], (ULONG *)&text);
    if (text && strcmp((const char *)text, A.find) != 0) {
        strncpy(A.find, (const char *)text, sizeof A.find - 1);
        A.find[sizeof A.find - 1] = 0;
        fill_list();
    }
}

static void tick(void)
{
    check_find();  /* the list follows the Find field as it is typed in */
    if (A.sel && A.sel->nshots > 1 && ++A.ticks >= CYCLE_TICKS) {
        A.ticks = 0;
        show_picture(A.shot + 1);
    }
}

static void run(void)
{
    ULONG sigmask = 0, result, code;
    int done = 0;
    GetAttr(WINDOW_SigMask, A.win, &sigmask);
    while (!done) {
        Wait(sigmask | SIGBREAKF_CTRL_C);
        while ((result = DoMethod(A.win, WM_HANDLEINPUT, (ULONG)&code)) != WMHI_LASTMSG) {
            switch (result & WMHI_CLASSMASK) {
            case WMHI_CLOSEWINDOW: done = 1; break;
            case WMHI_INTUITICK: tick(); break;
            case WMHI_ICONIFY:
                drop_picture();
                DoMethod(A.win, WM_ICONIFY);
                A.w = NULL;
                break;
            case WMHI_UNICONIFY:
                A.w = (struct Window *)DoMethod(A.win, WM_OPEN);
                if (!A.w) done = 1;
                else show_picture(A.shot);
                break;
            case WMHI_NEWSIZE: show_picture(A.shot); break;
            case WMHI_GADGETUP:
                switch (result & WMHI_GADGETMASK) {
                case G_FIND: check_find(); break;
                case G_SHOW: A.show = (int)code; fill_list(); break;
                case G_FIT: A.fit = (int)code; fill_list(); break;
                case G_SORT: A.sort = (int)code; A.sort_desc = 0; fill_list(); break;
                case G_UPDATES: fetch_feed(); installed_load(); fill_list(); select_port(NULL); break;
                case G_LIST: list_event(); break;
                case G_PREV: if (A.sel && A.sel->nshots) { A.ticks = 0; show_picture(A.shot + A.sel->nshots - 1); } break;
                case G_NEXT: A.ticks = 0; show_picture(A.shot + 1); break;
                case G_PICTURE: {
                    ULONG s, m;
                    CurrentTime(&s, &m);
                    if (DoubleClick(A.click_s, A.click_m, s, m)) launch_selected();
                    A.click_s = s;
                    A.click_m = m;
                    break;
                }
                case G_INSTALL: do_install(); break;
                case G_RELEASE:
                    if (A.sel) platform_open_url(A.sel->release_url ? A.sel->release_url : A.sel->port_repo);
                    break;
                case G_REMOVE: do_remove(); break;
                }
                break;
            }
        }
        if (SetSignal(0, 0) & SIGBREAKF_CTRL_C) done = 1;
    }
}

int main(void)
{
    const char *missing = NULL;
    int rc = 20;
    NewList(&A.rows);
    NewList(&A.facts);
    A.grey_pen = -1;
    if (!platform_open(&missing) || !open_classes(&missing)) {
        printf("AmiPortAtlas needs %s.\n", missing);
        goto out;
    }
    A.win = make_window();
    if (!A.win) { printf("AmiPortAtlas could not make its window.\n"); goto out; }
    A.w = (struct Window *)DoMethod(A.win, WM_OPEN);
    if (!A.w) { printf("AmiPortAtlas could not open its window.\n"); goto out; }
    A.scr = A.w->WScreen;
    A.grey_pen = ObtainBestPen(A.scr->ViewPort.ColorMap, 0x99999999UL, 0xA4A4A4A4UL, 0xB0B0B0B0UL,
                               OBP_Precision, PRECISION_GUI, TAG_DONE);
    installed_load();
    fetch_feed();
    fill_list();
    if (A.nshown) select_port(A.shown[0]);
    run();
    rc = 0;
out:
    drop_picture();
    if (A.win) DisposeObject(A.win);
    if (A.grey_pen >= 0 && A.scr) ReleasePen(A.scr->ViewPort.ColorMap, (ULONG)A.grey_pen);
    FreeListBrowserList(&A.rows);
    FreeListBrowserList(&A.facts);
    free(A.shown);
    feed_free(A.feed);
    close_classes();
    platform_close();
    return rc;
}
