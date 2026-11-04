/* AmiPortAtlas: the window, in GFOpenLayout UI.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * The layout is the approved mockup: the selected game's pictures top left,
 * cycling through that game's own screenshots, its facts under them; the
 * whole Atlas in a list on the right; the buttons bottom right, read last.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui.h"
#include "match.h"

#define CYCLE_TICKS 50   /* ui_tick comes every 100 ms: a picture every 5 s */
#define NCOLS 7

static const char markup[] =
"<column id=root>"
" <row id=top>"
"  <label for=find>Find</label><input id=find placeholder='a game' style='flex:1'>"
"  <label for=show>Show</label>"
"  <select id=show><option>All</option><option>Available</option><option>Our games</option>"
"   <option>Released</option><option>Testing</option><option>In progress</option>"
"   <option>Installed</option><option>Updates</option></select>"
"  <label for=fit>Fit</label>"
"  <select id=fit><option>Any</option><option>040</option><option>060</option><option>GPU</option></select>"
"  <label for=sort>Sort by</label>"
"  <select id=sort><option>Number</option><option>Name</option><option>Year</option>"
"   <option>State</option><option>Fit</option></select>"
"  <button id=scan title='Look on DH0: to DH15: for games that are already installed'>Find installed</button>"
"  <button id=updates title='Read the Port Atlas from www.amicradle.com again'>Check for updates</button>"
" </row>"
" <row id=middle>"
"  <fieldset id=game><legend id=gname>AmiPortAtlas</legend>"
"   <div id=shot selectable title='Double-click to play an installed game'></div>"
"   <row id=shotbar><button id=prev title='The picture before'>&lt;</button>"
"    <span id=shotcap></span><button id=next title='The next picture'>&gt;</button></row>"
"   <column id=facts>"
"    <row class=fact id=r_version><span class=k>Available</span><span class=v id=f_version></span></row>"
"    <row class=fact id=r_installed><span class=k>Installed</span><span class=v id=f_installed></span></row>"
"    <row class=fact id=r_year><span class=k>Year</span><span class=v id=f_year></span></row>"
"    <row class=fact id=r_display><span class=k>Display</span><span class=v id=f_display></span></row>"
"    <row class=fact id=r_runs><span class=k>Runs on</span><span class=v id=f_runs></span></row>"
"    <row class=fact id=r_tested><span class=k>Tested on</span><span class=v id=f_tested></span></row>"
"    <row class=fact id=r_needs><span class=k>Needs</span><span class=v id=f_needs></span></row>"
"    <row class=fact id=r_size><span class=k>Size</span><span class=v id=f_size></span></row>"
"    <row class=fact id=r_data><span class=k>Game data</span><span class=v id=f_data></span></row>"
"    <row class=fact id=r_release><span class=k>Release</span><span class=v id=f_release></span></row>"
"   </column>"
"  </fieldset>"
"  <column id=right>"
"   <row id=heads>"
"    <button class=th id=h0 style='width:44px'>#</button><button class=th id=h1 style='width:176px'>Name</button>"
"    <button class=th id=h2 style='width:44px'>Year</button><button class=th id=h3 style='width:84px'>State</button>"
"    <button class=th id=h4 style='width:36px'>Fit</button><button class=th id=h5 style='width:76px'>Available</button>"
"    <button class=th id=h6 style='flex:1'>Installed</button>"
"   </row>"
"   <vlist id=list columns='44 176 44 84 36 76 76'></vlist>"
"   <row id=actions><spacer></spacer>"
"    <button id=install class=default disabled>Install...</button>"
"    <button id=release disabled>Release page</button>"
"    <button id=remove disabled>Remove</button></row>"
"  </column>"
" </row>"
" <row id=bottom><span id=status style='flex:1'>Starting...</span><progress id=gauge max=100 hidden></progress></row>"
"</column>";

/* After the theme's CSS: only shapes and sizes, so every theme's colours hold. */
static const char css[] =
"#root{flex:1;padding:8px;gap:8px}"
"#top{gap:6px;align-items:center}"
"#middle{flex:1;gap:10px}"
"#game{width:470px;gap:8px}"
"#shot{height:300px;background:#000;border:1px solid #9aa6b3}"
"#shotbar{gap:8px;justify-content:center;align-items:center}"
"#shotcap{font-size:0.9em}"
"#facts{gap:2px}"
".fact .k{width:110px;font-weight:bold}"
".fact .v{flex:1}"
"#right{flex:1;gap:6px}"
"#heads{gap:0}"
".th{border-radius:0;text-align:left;padding:2px 6px}"
"#list{flex:1}"
"#actions{gap:8px}"
"#actions button{width:130px}"
"#bottom{gap:10px;align-items:center}"
"#gauge{width:180px}";

static olui *U;
static UiHost *H;
static Feed *F;
static Port **shown;
static int nshown, show_mode, fit_mode, sort_key, sort_desc;
static char find_text[96];
static Port *sel;
static int shot, ticks;

static const int column_sort[NCOLS] = { SORT_NUMBER, SORT_NAME, SORT_YEAR, SORT_STATE, SORT_FIT, -1, -1 };
static const char *const column_name[NCOLS] = { "#", "Name", "Year", "State", "Fit", "Available", "Installed" };
static const char *const fit_name[] = { "", "040", "060", "GPU" };

static olui_el *el(const char *id) { return olui_find(U, id); }

static void set_text(const char *id, const char *text)
{
    olui_el *e = el(id);
    if (e) olui_set_text(U, e, text ? text : "");
}

static const char *installed(const Port *p) { return H && H->installed ? H->installed(H->data, p) : NULL; }

static int is_update(const Port *p)
{
    const char *v = installed(p);
    /* by number ("1.9" before "1.10"); a found copy with no version can't tell */
    return v && p->version && port_installable(p) && version_cmp(v, p->version) < 0;
}

/* ---- the list ---- */

static int contains(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
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

static int state_is(const Port *p, const char *s) { return p->state && strcmp(p->state, s) == 0; }

static int passes(const Port *p)
{
    if (find_text[0] && !contains(p->title, find_text)) return 0;
    if (fit_mode && (!p->fit || strcmp(p->fit, fit_name[fit_mode]) != 0)) return 0;
    switch (show_mode) {
    case 1: return port_installable(p);
    case 2: return port_is_original(p);
    case 3: return state_is(p, "released");
    case 4: return state_is(p, "testing");
    case 5: return state_is(p, "in_progress");
    case 6: return installed(p) != NULL;
    case 7: return is_update(p);
    default: return 1;
    }
}

/* A cell's text, as the list draws it. */
static const char *cell(void *data, int row, int column, int *image)
{
    static char buf[4][24];
    static int next;
    char *b = buf[next++ & 3];
    Port *p;
    const char *v;
    (void)data;
    if (row < 0 || row >= nshown) return NULL;
    p = shown[row];
    switch (column) {
    case 0:
        *image = installed(p) ? (is_update(p) ? MARK_UPDATE : MARK_INSTALLED)
               : port_installable(p) ? MARK_AVAILABLE : port_is_original(p) ? MARK_ORIGINAL : MARK_COMING;
        snprintf(b, 24, "%ld", p->number);
        return b;
    case 1: return p->title;
    case 2: snprintf(b, 24, "%.4s", p->year ? p->year : ""); return b;
    case 3: return is_update(p) ? "Update" : port_state_text(p);
    case 4: return p->fit ? p->fit : "";
    case 5: return p->version && state_is(p, "released") ? p->version : "";
    case 6: v = installed(p); return v ? v : "";
    default: return NULL;
    }
}

static void headings(void)
{
    char t[32];
    int c;
    for (c = 0; c < NCOLS; c++) {
        char id[4];
        snprintf(id, sizeof id, "h%d", c);
        if (column_sort[c] == sort_key) snprintf(t, sizeof t, "%s %s", column_name[c], sort_desc ? "v" : "^");
        else snprintf(t, sizeof t, "%s", column_name[c]);
        set_text(id, t);
    }
}

static void fill(void)
{
    char line[160];
    int i, available = 0, keep = -1;
    nshown = 0;
    if (F) {
        Port **n = (Port **)realloc(shown, (size_t)(F->count ? F->count : 1) * sizeof(Port *));
        if (!n) return;
        shown = n;
        for (i = 0; i < F->count; i++) {
            if (port_installable(&F->ports[i])) available++;
            if (passes(&F->ports[i])) shown[nshown++] = &F->ports[i];
        }
        feed_sort(shown, nshown, sort_key, sort_desc);
        for (i = 0; i < nshown; i++) if (shown[i] == sel) keep = i;
    }
    olui_vlist_set(U, el("list"), nshown, cell, NULL);
    if (keep >= 0) {
        olui_vlist_select(U, el("list"), keep, 1);
        olui_vlist_set_current(U, el("list"), keep);
    }
    headings();
    if (F) {
        snprintf(line, sizeof line, "Port Atlas of %s: %d of %d shown, %d to install",
                 F->generated ? F->generated : "?", nshown, F->count, available);
        ui_status(line);
    }
}

void ui_refill(void) { fill(); ui_select_row(-2); }

/* ---- the selected game ---- */

static void fact(const char *id, const char *text)
{
    char rid[16], fid[16];
    snprintf(rid, sizeof rid, "r_%s", id);
    snprintf(fid, sizeof fid, "f_%s", id);
    set_text(fid, text);
    olui_set_hidden(U, el(rid), !(text && text[0]));
}

static void join(char *out, size_t max, char **list, int n)
{
    int i;
    out[0] = 0;
    for (i = 0; i < n; i++) {
        if (i) strncat(out, ", ", max - strlen(out) - 1);
        strncat(out, list[i], max - strlen(out) - 1);
    }
}

static void show_shot(int k)
{
    char cap[32];
    if (!sel || sel->nshots == 0) {
        shot = 0;
        set_text("shotcap", sel ? "No pictures yet" : "");
    } else {
        shot = ((k % sel->nshots) + sel->nshots) % sel->nshots;
        snprintf(cap, sizeof cap, "%d of %d", shot + 1, sel->nshots);
        set_text("shotcap", cap);
    }
    olui_set_disabled(U, el("prev"), !sel || sel->nshots < 2);
    olui_set_disabled(U, el("next"), !sel || sel->nshots < 2);
    ticks = 0;
    if (H && H->picture) H->picture(H->data, sel && sel->nshots ? sel : NULL, shot);
}

static void select_port(Port *p)
{
    char line[200];
    const char *inst;
    sel = p;
    set_text("gname", p ? p->title : "AmiPortAtlas");
    inst = p ? installed(p) : NULL;
    if (p) {
        if (p->version && state_is(p, "released")) snprintf(line, sizeof line, "%s", p->version);
        else snprintf(line, sizeof line, "Not released yet (%s)", port_state_text(p));
        fact("version", line);
        fact("installed", inst);
        fact("year", p->year);
        fact("display", p->display);
        if (p->cpu) snprintf(line, sizeof line, "%s%s (Fit %s)", p->cpu, p->fpu ? " with FPU" : "", p->fit ? p->fit : "?");
        else snprintf(line, sizeof line, "%s", p->fit ? p->fit : "");
        fact("runs", line);
        join(line, sizeof line, p->requires, p->nrequires);
        fact("tested", line[0] ? line : "Not yet tested");
        join(line, sizeof line, p->modules, p->nmodules);
        fact("needs", line);
        if (p->download_size > 0 || p->space > 0)
            snprintf(line, sizeof line, "%ld KB download, %ld KB installed",
                     (p->download_size + 1023) / 1024, (p->space + 1023) / 1024);
        else line[0] = 0;
        fact("size", line);
        fact("data", p->game_data);
        fact("release", p->release_url ? p->release_url : p->port_repo);
    } else {
        static const char *const ids[] = { "version", "installed", "year", "display", "runs", "tested",
                                           "needs", "size", "data", "release" };
        int i;
        for (i = 0; i < 10; i++) fact(ids[i], NULL);
    }
    set_text("install", p && is_update(p) ? "Update..." : inst ? "Play" : "Install...");
    olui_set_disabled(U, el("install"), !(p && (inst || port_installable(p))));
    olui_set_disabled(U, el("remove"), !inst);
    olui_set_disabled(U, el("release"), !(p && (p->release_url || p->port_repo)));
    show_shot(0);
}

void ui_select_row(int row)
{
    olui_el *list = el("list");
    if (row == -2) {            /* keep the game, wherever it now is */
        int i;
        for (i = 0; i < nshown; i++) if (shown[i] == sel) { select_port(sel); return; }
        row = nshown ? 0 : -1;
    }
    if (row < 0 || row >= nshown) { select_port(NULL); return; }
    {
        int i;  /* one game at a time */
        for (i = 0; i < nshown; i++) if (i != row && olui_vlist_selected(list, i)) olui_vlist_select(U, list, i, 0);
    }
    olui_vlist_select(U, list, row, 1);
    olui_vlist_set_current(U, list, row);
    select_port(shown[row]);
}

Port *ui_selected(void) { return sel; }
int   ui_rows(void) { return nshown; }

/* ---- events ---- */

static void on_find(olui *ui, const olui_event *ev, void *d)
{
    (void)ui; (void)d;
    snprintf(find_text, sizeof find_text, "%s", ev->value ? ev->value : "");
    fill();
}

static void on_choice(olui *ui, const olui_event *ev, void *d)
{
    (void)ui; (void)d;
    if (strcmp(ev->id, "show") == 0) show_mode = ev->index;
    else if (strcmp(ev->id, "fit") == 0) fit_mode = ev->index;
    else { sort_key = ev->index; sort_desc = 0; }
    fill();
}

static void on_heading(olui *ui, const olui_event *ev, void *d)
{
    int c = ev->id[1] - '0';
    (void)d;
    if (c < 0 || c >= NCOLS || column_sort[c] < 0) return;
    if (sort_key == column_sort[c]) sort_desc = !sort_desc;
    else { sort_key = column_sort[c]; sort_desc = 0; }
    olui_select_set(ui, el("sort"), sort_key);
    fill();
}

static void on_list(olui *ui, const olui_event *ev, void *d)
{
    (void)ui; (void)d;
    if (ev->index >= 0 && ev->index < nshown && shown[ev->index] != sel) select_port(shown[ev->index]);
    if (ev->type == OLUI_ACTIVATE && sel && installed(sel) && H->launch) H->launch(H->data, sel);
}

static void on_shot(olui *ui, const olui_event *ev, void *d)
{
    (void)ui; (void)ev; (void)d;
    if (sel && installed(sel) && H->launch) H->launch(H->data, sel);
}

static void on_button(olui *ui, const olui_event *ev, void *d)
{
    (void)ui; (void)d;
    if (strcmp(ev->id, "prev") == 0) show_shot(shot - 1);
    else if (strcmp(ev->id, "next") == 0) show_shot(shot + 1);
    else if (strcmp(ev->id, "updates") == 0) { if (H->refresh) H->refresh(H->data); }
    else if (strcmp(ev->id, "scan") == 0) { if (H->scan) H->scan(H->data); }
    else if (!sel) return;
    else if (strcmp(ev->id, "install") == 0) {
        if (installed(sel) && !is_update(sel)) { if (H->launch) H->launch(H->data, sel); }
        else if (H->install) H->install(H->data, sel);
    }
    else if (strcmp(ev->id, "release") == 0) {
        if (H->open_url) H->open_url(H->data, sel->release_url ? sel->release_url : sel->port_repo);
    }
    else if (strcmp(ev->id, "remove") == 0) { if (H->remove) H->remove(H->data, sel); }
}

int ui_build(olui *ui, UiHost *host, char *err, size_t errlen)
{
    int c;
    U = ui;
    H = host;
    if (!olui_load(ui, markup, css, err, errlen)) return 0;
    olui_on(ui, "find", OLUI_CHANGE, on_find, NULL);
    olui_on(ui, "show", OLUI_CHANGE, on_choice, NULL);
    olui_on(ui, "fit", OLUI_CHANGE, on_choice, NULL);
    olui_on(ui, "sort", OLUI_CHANGE, on_choice, NULL);
    for (c = 0; c < NCOLS; c++) {
        static char ids[NCOLS][4];
        snprintf(ids[c], sizeof ids[c], "h%d", c);
        olui_on(ui, ids[c], OLUI_CLICK, on_heading, NULL);
    }
    olui_on(ui, "list", OLUI_SELECT, on_list, NULL);
    olui_on(ui, "list", OLUI_ACTIVATE, on_list, NULL);
    olui_on(ui, "shot", OLUI_ACTIVATE, on_shot, NULL);
    olui_on(ui, "prev", OLUI_CLICK, on_button, NULL);
    olui_on(ui, "next", OLUI_CLICK, on_button, NULL);
    olui_on(ui, "updates", OLUI_CLICK, on_button, NULL);
    olui_on(ui, "scan", OLUI_CLICK, on_button, NULL);
    olui_on(ui, "install", OLUI_CLICK, on_button, NULL);
    olui_on(ui, "release", OLUI_CLICK, on_button, NULL);
    olui_on(ui, "remove", OLUI_CLICK, on_button, NULL);
    select_port(NULL);
    return 1;
}

void ui_set_feed(Feed *feed)
{
    F = feed;
    sel = NULL;
    fill();
    ui_select_row(nshown ? 0 : -1);
}

void ui_status(const char *text) { set_text("status", text); }

void ui_progress(int percent)
{
    olui_el *g = el("gauge");
    olui_set_hidden(U, g, percent < 0);
    if (percent >= 0) olui_set_progress(U, g, percent);
}

void ui_busy(int busy)
{
    static const char *const ids[] = { "install", "release", "remove", "updates", "scan", "list", "find" };
    int i;
    if (!busy) { select_port(sel); olui_set_disabled(U, el("updates"), 0); olui_set_disabled(U, el("scan"), 0);
                 olui_set_disabled(U, el("list"), 0); olui_set_disabled(U, el("find"), 0); return; }
    for (i = 0; i < (int)(sizeof ids / sizeof ids[0]); i++) olui_set_disabled(U, el(ids[i]), 1);
}

void ui_tick(void)
{
    if (sel && sel->nshots > 1 && ++ticks >= CYCLE_TICKS) show_shot(shot + 1);
}
