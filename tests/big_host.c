/* AmiPortAtlas: the real window, laid out and drawn on the host by
 * GFOpenLayout UI, from a real ports.json. MIT, (c) 2026 The AmiCradle Team.
 *
 *   ui_host ports.json theme-file ops.txt [width height]
 *
 * Checks the markup and CSS load, the list fills, sorting and the filters
 * work and the buttons follow the selection, then writes the window's draw
 * ops for tools/render_ui.py. Exit 0 when every check passes.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/ui.h"
#include "../src/big.h"
#include "../src/gamesfolders.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static char *slurp(const char *path, long *len)
{
    FILE *f = fopen(path, "rb");
    char *b;
    long n;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    b = (char *)malloc((size_t)n + 1);
    if (b && fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); b = NULL; }
    if (b) b[n] = 0;
    fclose(f);
    if (len) *len = n;
    return b;
}

/* ---- the host side ---- */

static const Port *pictured;
static int picture_shot, launches, installs;

static const char *host_installed(void *d, const Port *p)
{
    (void)d;
    /* As if two games were installed by hand earlier. */
    if (p->id && strcmp(p->id, "g-opentyrian") == 0) return "2.1";
    if (p->id && strcmp(p->id, "g-neverball-neverputt") == 0) return "1.6.0";
    return NULL;
}
static void host_picture(void *d, const Port *p, int s) { (void)d; pictured = p; picture_shot = s; }
static void host_install(void *d, Port *p) { (void)d; (void)p; installs++; }
static void host_launch(void *d, Port *p) { (void)d; (void)p; launches++; }

/* ---- draw ops, as gfopenlayout's tests/ui_render.py reads them ---- */

static FILE *out;

static void draw(void *data, const olui_draw_op *op)
{
    int i;
    (void)data;
    switch (op->kind) {
    case OLUI_DRAW_BOX:
        fprintf(out, "box %ld %ld %ld %ld %d %08lx %d %d", (long)op->box.x, (long)op->box.y, (long)op->box.w,
                (long)op->box.h, op->border, (unsigned long)op->border_argb, op->radius, op->stops);
        for (i = 0; i < op->stops; ++i) fprintf(out, " %08lx %d", (unsigned long)op->stop_argb[i], op->stop_pos[i]);
        fputc('\n', out);
        break;
    case OLUI_DRAW_TEXT:
        fprintf(out, "text %ld %ld %d %08lx %lu %.*s\n", (long)op->box.x, (long)op->baseline, op->font_pixels,
                (unsigned long)op->argb, (unsigned long)op->text_flags, (int)op->length, op->text);
        break;
    case OLUI_DRAW_MARK:
        fprintf(out, "mark %ld %ld %ld %ld %d %08lx\n", (long)op->box.x, (long)op->box.y, (long)op->box.w,
                (long)op->box.h, op->mark, (unsigned long)op->argb);
        break;
    case OLUI_DRAW_CLIP:
        fprintf(out, "clip %ld %ld %ld %ld\n", (long)op->clip.x, (long)op->clip.y, (long)op->clip.w, (long)op->clip.h);
        break;
    case OLUI_DRAW_IMAGE:
        fprintf(out, "image %ld %ld %ld %ld %d\n", (long)op->box.x, (long)op->box.y, (long)op->box.w,
                (long)op->box.h, op->image);
        break;
    default:
        break;
    }
}

/* The New section's stand-in for the index: Kobo Deluxe and Tiny Tetris are news. */
static int host_is_new(void *data, const Port *p)
{
    (void)data;
    return strcmp(p->title, "Kobo Deluxe") == 0 || strcmp(p->title, "Tiny Tetris") == 0;
}

static char *dup(const char *t)
{
    char *c = (char *)malloc(strlen(t) + 1);
    if (c) strcpy(c, t);
    return c;
}

int main(int argc, char **argv)
{
    char err[200];
    char *json, *theme, *tcss;
    long len;
    int w = argc > 5 ? atoi(argv[4]) : 1280, h = argc > 5 ? atoi(argv[5]) : 720, i, held = 0, k, all;
    const char *why;
    Feed *feed;
    olui *ui, *big;
    olui_rect r;
    UiHost host;
    static const char *const on_drive[] = { "Ports", "WHDLoad", "Itch.io", "Aminet", "Exclusive" };

    if (argc < 4) { printf("big_host ports.json theme ops.txt [w h]\n"); return 2; }
    feed_utf8 = 1;
    json = slurp(argv[1], &len);
    if (!json || !(feed = feed_parse(json, len, &why))) { printf("feed\n"); return 20; }
    theme = slurp(argv[2], NULL);
    tcss = theme ? olui_theme_css(theme, 0, err, sizeof err) : NULL;
    CHECK(tcss != NULL);

    memset(&host, 0, sizeof host);
    host.installed = host_installed;
    host.picture = host_picture;
    host.install = host_install;
    host.launch = host_launch;
    host.is_new = host_is_new;
    ui = olui_new();
    if (tcss) olui_add_css(ui, tcss);
    if (!ui_build(ui, &host, err, sizeof err)) { printf("window: %s\n", err); return 20; }
    ui_set_feed(feed);
    ui_set_drawers(on_drive, 5);
    all = ui_rows();
    big = olui_new();
    if (tcss) olui_add_css(big, tcss);
    if (!big_build(big, &host, err, sizeof err)) { printf("full screen: %s\n", err); return 20; }
    CHECK(olui_layout(big, w, h));

    /* the column keeps the chosen game in the middle */
    CHECK(big_first_row(0, 100, 11) == 0 && big_first_row(5, 100, 11) == 0 && big_first_row(6, 100, 11) == 1);
    CHECK(big_first_row(99, 100, 11) == 89 && big_first_row(3, 4, 11) == 0 && big_first_row(-1, 100, 11) == 0);

    /* the stick: a press moves once; held up or down runs after a wait; fire and fire 2 */
    CHECK(big_stick(1UL << 27, 0, &held, 5, 2) == BIG_UP && held == 0);
    for (k = 0, i = 0; i < 10; i++) if (big_stick(1UL << 27, 1UL << 27, &held, 5, 2) == BIG_UP) k++;
    CHECK(k == 3);                                            /* ticks 5, 7, 9 */
    held = 0;
    for (k = 0, i = 0; i < 10; i++) if (big_stick(1UL << 24, 1UL << 24, &held, 5, 2) != BIG_NONE) k++;
    CHECK(k == 0);                                            /* held right: one section, no run */
    CHECK(big_stick(1UL << 25, 0, &held, 5, 2) == BIG_LEFT && big_stick(1UL << 24, 0, &held, 5, 2) == BIG_RIGHT);
    CHECK(big_stick(1UL << 26, 0, &held, 5, 2) == BIG_DOWN);
    CHECK(big_stick(1UL << 22, 0, &held, 5, 2) == BIG_FIRE && big_stick(1UL << 23, 0, &held, 5, 2) == BIG_BACK);
    CHECK(big_stick(1UL << 22, 1UL << 22, &held, 5, 2) == BIG_NONE);   /* fire held: once */
    CHECK(big_stick(0, 1UL << 27, &held, 5, 2) == BIG_NONE);

    /* moving: the window's games and sections, shared */
    CHECK(ui_section() == 0 && ui_rows() == all && ui_current_row() == 0);
    CHECK(big_input(BIG_UP) == BIG_DID_NOTHING);              /* at the top */
    CHECK(big_input(BIG_DOWN) == BIG_MOVED && ui_current_row() == 1);
    CHECK(big_input(BIG_LEFT) == BIG_DID_NOTHING && ui_section() == 0);
    CHECK(big_input(BIG_RIGHT) == BIG_MOVED && ui_section() == 1 && ui_current_row() == (ui_rows() ? 0 : -1));   /* New */
    CHECK(big_input(BIG_BACK) == BIG_LEAVE);
    for (i = 0; i < 20; i++) big_input(BIG_RIGHT);
    CHECK(ui_section() == ui_sections() - 1);                 /* the last section, no further */
    while (ui_section() > 0) big_input(BIG_LEFT);
    /* fire: Play an installed game, Install one that can be */
    for (i = 0; i < ui_rows(); i++) { ui_select_row(i); if (host_installed(NULL, ui_selected())) break; }
    CHECK(i < ui_rows() && big_input(BIG_FIRE) == BIG_PLAY);
    for (i = 0; i < ui_rows(); i++) { ui_select_row(i); if (!host_installed(NULL, ui_selected()) && port_installable(ui_selected())) break; }
    if (i < ui_rows()) CHECK(big_input(BIG_FIRE) == BIG_INSTALL);   /* the test feed's one installable game may be installed */
    for (i = 0; i < ui_rows(); i++) { ui_select_row(i); if (!host_installed(NULL, ui_selected()) && !port_installable(ui_selected()) && !port_on_disk_only(ui_selected())) break; }
    CHECK(i < ui_rows() && big_input(BIG_FIRE) == BIG_DID_NOTHING);   /* not out yet */

    /* the picture: Kobo Deluxe in All */
    for (i = 0; i < ui_rows(); i++) { ui_select_row(i); if (strcmp(ui_selected()->title, "Kobo Deluxe") == 0) break; }
    big_fill();
    CHECK(olui_layout(big, w, h));
    CHECK(olui_box(big, olui_find(big, "shot"), &r));
    out = fopen(argv[3], "w");
    CHECK(out != NULL);
    if (out) {
        fprintf(out, "size %d %d\n", w, h);
        olui_draw(big, NULL, draw, NULL);
        fprintf(out, "shot %ld %ld %ld %ld\n", (long)r.x, (long)r.y, (long)r.w, (long)r.h);
        fclose(out);
    }
    printf("full screen %dx%d; picture box %ldx%ld; %s\n", w, h, (long)r.w, (long)r.h, failures ? "FAILED" : "all passed");
    olui_free(big);
    olui_free(ui);
    feed_free(feed);
    free(json); free(theme); free(tcss);
    return failures != 0;
}
