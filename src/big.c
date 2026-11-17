/* AmiPortAtlas's full screen mode (big.h). MIT, (c) 2026 The AmiCradle Team. */
#include <stdio.h>
#include <string.h>

#include "big.h"
#include "feed.h"
#include "gamesfolders.h"

#define BIG_SECS 12     /* sections shown along the top at once */

static const char markup[] =
"<column id=broot>"
" <row id=bsecs>"
"  <span class=bs id=bs0></span><span class=bs id=bs1></span><span class=bs id=bs2></span><span class=bs id=bs3></span>"
"  <span class=bs id=bs4></span><span class=bs id=bs5></span><span class=bs id=bs6></span><span class=bs id=bs7></span>"
"  <span class=bs id=bs8></span><span class=bs id=bs9></span><span class=bs id=bs10></span><span class=bs id=bs11></span>"
"  <spacer></spacer><span id=bcount></span>"
" </row>"
" <row id=bmid>"
"  <column id=bgames>"
"   <row class=bgr><span class=bg id=bg0></span></row><row class=bgr><span class=bg id=bg1></span></row><row class=bgr><span class=bg id=bg2></span></row><row class=bgr><span class=bg id=bg3></span></row>"
"   <row class=bgr><span class=bg id=bg4></span></row><row class=bgr><span class=bg id=bg5></span></row><row class=bgr><span class=bg id=bg6></span></row><row class=bgr><span class=bg id=bg7></span></row>"
"   <row class=bgr><span class=bg id=bg8></span></row><row class=bgr><span class=bg id=bg9></span></row><row class=bgr><span class=bg id=bg10></span></row>"
"  </column>"
"  <column id=bgame>"
"   <div id=shot></div>"
"   <span id=btitle></span>"
"   <span id=babout></span>"
"   <row class=bf><span class=bk>Year</span><span class=bv id=byear></span>"
"    <span class=bk>Runs on</span><span class=bv id=bruns></span></row>"
"   <row class=bf><span class=bk>By</span><span class=bv id=bby></span>"
"    <span class=bk>State</span><span class=bv id=bstate></span></row>"
"   <span id=bwhere></span>"
"  </column>"
" </row>"
" <row id=bhelp><span id=bfire></span><span>Fire 2 or Esc: back to the window</span>"
"  <span>Left, right: section</span><span>Up, down: game</span></row>"
"</column>";

/* Its own colours, over any theme: dark, the blue glass of the title bars for what is chosen. */
static const char css[] =
"#broot{flex:1;padding:24px 32px;gap:18px;background:#0d1622;color:#dfe8f2;font-size:1.4em}"
"#bsecs{gap:10px;align-items:center}"
".bs{padding:6px 16px;border-radius:14px;background:#1b2838;color:#9fb3c8}"
"#bcount{color:#7d90a6}"
"#bmid{flex:1;gap:32px}"
"#bgames{flex:1;gap:4px}"
".bgr{gap:0}.bg{flex:1;padding:6px 14px;border-radius:6px;color:#c7d4e2}"
"#bgame{flex:2;gap:10px}"
"#shot{flex:1;background:#000;border:2px solid #3a5878}"
"#btitle{font-size:1.6em;font-weight:bold;color:#ffffff}"
"#babout{color:#b6c6d6}"
".bf{gap:12px}.bk{width:110px;color:#7d90a6}.bv{flex:1}"
"#bwhere{color:#8fd19e}"
"#bhelp{gap:28px;color:#7d90a6;font-size:0.8em}"
"#bfire{color:#ffffff;font-weight:bold}";

static const char cur_sec[] = "background:#2f6fb5;color:#ffffff";
static const char other_sec[] = "background:#1b2838;color:#9fb3c8";
static const char cur_game[] = "background:#2f6fb5;color:#ffffff;font-weight:bold";
static const char other_game[] = "background:#0d1622;color:#c7d4e2;font-weight:normal";

static olui *B;
static UiHost *BH;

static olui_el *el(const char *id) { return olui_find(B, id); }
static void put(const char *id, const char *text) { olui_el *e = el(id); if (e) olui_set_text(B, e, text ? text : ""); }

int big_first_row(int current, int rows, int shown)
{
    int first;
    if (rows <= shown || current < 0) return 0;
    first = current - shown / 2;
    if (first < 0) first = 0;
    if (first > rows - shown) first = rows - shown;
    return first;
}

#define J_RIGHT (1UL << 24)
#define J_LEFT  (1UL << 25)
#define J_DOWN  (1UL << 26)
#define J_UP    (1UL << 27)
#define J_RED   (1UL << 22)
#define J_BLUE  (1UL << 23)

int big_stick(unsigned long now, unsigned long last, int *held, int delay, int repeat)
{
    unsigned long press = now & ~last;
    unsigned long dir = now & (J_RIGHT | J_LEFT | J_DOWN | J_UP);
    if (press & J_RED) { *held = 0; return BIG_FIRE; }
    if (press & J_BLUE) { *held = 0; return BIG_BACK; }
    if (!dir || (now & (J_RIGHT | J_LEFT | J_DOWN | J_UP)) != (last & (J_RIGHT | J_LEFT | J_DOWN | J_UP))) {
        *held = 0;
        if (press & J_UP) return BIG_UP;
        if (press & J_DOWN) return BIG_DOWN;
        if (press & J_LEFT) return BIG_LEFT;
        if (press & J_RIGHT) return BIG_RIGHT;
        return BIG_NONE;
    }
    /* held: up and down run through the list; left and right don't (a section at a time) */
    ++*held;
    if (*held >= delay && (*held - delay) % repeat == 0) {
        if (dir & J_UP) return BIG_UP;
        if (dir & J_DOWN) return BIG_DOWN;
    }
    return BIG_NONE;
}

void big_fill(void)
{
    char line[160];
    int i, n = ui_sections(), cur = ui_section(), rows = ui_rows(), row = ui_current_row(), first, sfirst;
    Port *p;
    const char *inst;
    if (!B) return;
    /* the sections: the chosen one in view */
    sfirst = big_first_row(cur, n, BIG_SECS);
    for (i = 0; i < BIG_SECS; i++) {
        char id[8];
        olui_el *e;
        snprintf(id, sizeof id, "bs%d", i);
        e = el(id);
        if (!e) continue;
        olui_set_hidden(B, e, sfirst + i >= n);
        if (sfirst + i < n) {
            olui_set_text(B, e, ui_section_name(sfirst + i));
            olui_set_style(B, e, sfirst + i == cur ? cur_sec : other_sec);
        }
    }
    snprintf(line, sizeof line, rows == 1 ? "1 game" : "%d games", rows);
    put("bcount", line);
    /* the games: the chosen one in the middle */
    if (row < 0 && rows > 0) { ui_select_row(0); row = 0; }
    first = big_first_row(row, rows, BIG_ROWS);
    for (i = 0; i < BIG_ROWS; i++) {
        char id[8];
        olui_el *e;
        Port *g = ui_row(first + i);
        snprintf(id, sizeof id, "bg%d", i);
        e = el(id);
        if (!e) continue;
        olui_set_text(B, e, g ? g->title : (i == 0 && !rows ? "Nothing here yet" : ""));
        olui_set_style(B, e, g && first + i == row ? cur_game : other_game);
    }
    p = ui_selected();
    inst = p && BH && BH->installed ? BH->installed(BH->data, p) : NULL;
    put("btitle", p ? p->title : "");
    put("babout", p ? p->summary : "");
    put("byear", p ? p->year : "");
    if (p && p->cpu) snprintf(line, sizeof line, "%s%s", p->cpu, p->fpu ? " with FPU" : "");
    else snprintf(line, sizeof line, "%s", p && p->fit ? p->fit : "");
    put("bruns", line);
    put("bby", p ? p->author : "");
    put("bstate", p ? port_state_text(p) : "");
    if (inst) snprintf(line, sizeof line, "Installed: %s", inst);
    else if (p && port_on_disk_only(p)) snprintf(line, sizeof line, "On the Games drive");
    else if (p && port_installable(p)) snprintf(line, sizeof line, "Not installed: fire installs it");
    else snprintf(line, sizeof line, "%s", p ? "Not out yet" : "");
    put("bwhere", line);
    put("bfire", inst || (p && port_on_disk_only(p)) ? "Fire: Play" : p && port_installable(p) ? "Fire: Install" : "");
}

int big_input(int what)
{
    int n = ui_sections(), cur = ui_section(), rows = ui_rows(), row = ui_current_row();
    Port *p = ui_selected();
    switch (what) {
    case BIG_LEFT:  if (cur > 0) { ui_set_section(cur - 1); ui_select_row(0); big_fill(); return BIG_MOVED; } break;
    case BIG_RIGHT: if (cur < n - 1) { ui_set_section(cur + 1); ui_select_row(0); big_fill(); return BIG_MOVED; } break;
    case BIG_UP:    if (row > 0) { ui_select_row(row - 1); big_fill(); return BIG_MOVED; } break;
    case BIG_DOWN:  if (row < rows - 1) { ui_select_row(row + 1); big_fill(); return BIG_MOVED; } break;
    case BIG_FIRE:
        if (!p) break;
        if ((BH && BH->installed && BH->installed(BH->data, p)) || port_on_disk_only(p)) return BIG_PLAY;
        if (port_installable(p)) return BIG_INSTALL;
        break;
    case BIG_BACK:  return BIG_LEAVE;
    }
    return BIG_DID_NOTHING;
}

int big_build(olui *ui, UiHost *host, char *err, size_t errlen)
{
    B = ui;
    BH = host;
    if (!olui_load(ui, markup, css, err, errlen)) return 0;
    big_fill();
    return 1;
}
