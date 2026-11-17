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
    int w = argc > 5 ? atoi(argv[4]) : 1100, h = argc > 5 ? atoi(argv[5]) : 680, rows;
    const char *why;
    Feed *feed;
    olui *ui;
    olui_rect r;
    UiHost host;

    if (argc < 4) { printf("ui_host ports.json theme ops.txt [w h]\n"); return 2; }
    feed_utf8 = 1;
    json = slurp(argv[1], &len);
    CHECK(json != NULL);
    if (!json) return 20;
    feed = feed_parse(json, len, &why);
    CHECK(feed != NULL);
    if (!feed) { printf("feed: %s\n", why); return 20; }

    ui = olui_new();
    theme = slurp(argv[2], NULL);
    CHECK(theme != NULL);
    tcss = theme ? olui_theme_css(theme, 0, err, sizeof err) : NULL;
    if (!tcss) printf("theme: %s\n", err);
    CHECK(tcss != NULL);
    if (tcss) olui_add_css(ui, tcss);

    memset(&host, 0, sizeof host);
    host.installed = host_installed;
    host.picture = host_picture;
    host.install = host_install;
    host.launch = host_launch;
    host.is_new = host_is_new;
    if (!ui_build(ui, &host, err, sizeof err)) { printf("markup: %s\n", err); return 20; }
    CHECK(olui_layout(ui, w, h));

    ui_set_feed(feed);
    rows = ui_rows();
    CHECK(rows == feed->count);
    CHECK(ui_selected() != NULL && ui_selected()->number == 1);  /* sorted by number */

    /* Filters: Fit GPU, then back to Any. */
    {
        olui_event ev;
        memset(&ev, 0, sizeof ev);
        CHECK(olui_find(ui, "fit") && olui_find(ui, "list") && olui_find(ui, "shot"));
    }

    /* Pick Kobo Deluxe, as the mockup shows, by its row. */
    {
        int i, kobo = -1;
        for (i = 0; i < rows; i++) {
            ui_select_row(i);
            if (ui_selected() && strcmp(ui_selected()->title, "Kobo Deluxe") == 0) { kobo = i; break; }
        }
        CHECK(kobo >= 0);
        CHECK(pictured == ui_selected());   /* its picture was asked for */
        /* Nothing is released: Install is off, Release page on. */
        ui_select_row(kobo);
    }
    for (len = 0; len < 50; len++) ui_tick();   /* 5 s: one picture on, when it has two */
    /* Two sources: the Atlas and a small index of another's, one list. */
    {
        static const char extra[] = "{\"schema_version\":1,\"ports\":[{\"id\":\"tiny\",\"title\":\"Tiny Tetris\","
            "\"short\":\"Falling blocks\",\"author\":\"Someone\",\"version\":\"1.0\",\"state\":\"released\"}]}";
        static const char *const ids[2] = { ATLAS_SOURCE, "aminet-game" };
        static const char *const names[2] = { "AmiCradle Port Atlas", "Aminet: games" };
        Feed *two[2];
        Feed *merged;
        int count = feed->count, i, tiny = -1;
        two[0] = feed;
        two[1] = feed_parse(extra, (long)sizeof extra - 1, &why);
        CHECK(two[1] != NULL);
        merged = feed_merge(two, ids, 2);
        CHECK(merged != NULL && merged->count == count + 1);
        feed = merged;
        /* a game on the Games drive that no source lists, as the program adds them from the index */
        {
            Port *more = (Port *)realloc(feed->ports, (size_t)(feed->count + 1) * sizeof(Port)), *p;
            CHECK(more != NULL);
            if (!more) return 20;
            feed->ports = more;
            p = &feed->ports[feed->count++];
            memset(p, 0, sizeof *p);
            p->id = dup("disk:more games/zeewolf");
            p->title = dup("Zeewolf");
            p->drawer = dup("Zeewolf");
            p->source = dup(DISK_SOURCE);
            p->kind = dup("game");
            p->category = dup("More Games");
        }
        ui_set_sources(ids, names, 2);
        ui_set_feed(feed);
        CHECK(ui_rows() == count + 2);
        CHECK(ui_selected() && ui_selected()->number == 1);   /* the Atlas's first */
        for (i = 0; i < ui_rows(); i++) {
            ui_select_row(i);
            if (ui_selected() && strcmp(ui_selected()->title, "Tiny Tetris") == 0) { tiny = i; break; }
        }
        CHECK(tiny == ui_rows() - 2);   /* no number: after the Atlas's */
        CHECK(ui_selected() && strcmp(ui_selected()->id, "tiny@aminet-game") == 0);
        for (i = 0; i < ui_rows(); i++) {
            ui_select_row(i);
            if (ui_selected() && strcmp(ui_selected()->title, "Kobo Deluxe") == 0) break;
        }
    }
    /* The sections above the list are found: All, New, then a tab for each drawer with something to show. */
    {
        int all = ui_rows(), i, n = 0;
        static const char *const on_drive[] = { "whdload", "Ports", "More Games", "Itch.io", "more games" };
        olui_el *tabs = olui_find(ui, "sections");
        CHECK(tabs != NULL && olui_tabs_current(tabs) == 0 && ui_section() == 0);
        /* no Games drive: only the drawers the sources have games for */
        CHECK(ui_sections() == 5);
        CHECK(strcmp(ui_section_name(0), "All") == 0 && strcmp(ui_section_name(1), "New") == 0);
        CHECK(strcmp(ui_section_name(2), "Ports") == 0 && strcmp(ui_section_name(3), "Aminet") == 0
              && strcmp(ui_section_name(4), "Exclusive") == 0 && ui_section_name(5) == NULL);
        CHECK(ui_find_section("WHDLoad") == -1 && ui_find_section("aminet") == 3);
        ui_set_section(3);                                  /* Aminet: Tiny Tetris */
        CHECK(ui_rows() == 1 && ui_selected() && strcmp(ui_selected()->title, "Tiny Tetris") == 0);
        /* the Games drive's drawers are found: the standard ones in their order, a new one after them */
        ui_set_drawers(on_drive, 5);
        CHECK(ui_sections() == 8);
        CHECK(strcmp(ui_section_name(2), "Ports") == 0 && strcmp(ui_section_name(3), "WHDLoad") == 0
              && strcmp(ui_section_name(4), "Itch.io") == 0 && strcmp(ui_section_name(5), "Aminet") == 0
              && strcmp(ui_section_name(6), "Exclusive") == 0 && strcmp(ui_section_name(7), "More Games") == 0);
        CHECK(ui_section() == 5 && ui_rows() == 1);         /* still on Aminet, which moved along */
        tabs = olui_find(ui, "sections");
        CHECK(tabs != NULL && olui_tabs_current(tabs) == 5);
        ui_set_drawers(on_drive, 5);                        /* the same again: nothing changes */
        CHECK(olui_find(ui, "sections") == tabs && ui_section() == 5);
        ui_set_section(1);                                  /* New */
        CHECK(ui_rows() == 2 && olui_tabs_current(tabs) == 1);
        CHECK(ui_selected() && strcmp(ui_selected()->title, "Kobo Deluxe") == 0);
        ui_set_section(2);                                  /* Ports: the Atlas's ports, not our own games */
        n = ui_rows();
        CHECK(n > 0 && n < all);
        for (i = 0; i < n; i++) { ui_select_row(i); CHECK(ui_selected() && !port_is_original(ui_selected())
                                                          && strcmp(ui_selected()->source, ATLAS_SOURCE) == 0); }
        ui_set_section(3); CHECK(ui_rows() == 0 && ui_selected() == NULL);   /* WHDLoad: on the drive, no source yet */
        ui_set_section(4); CHECK(ui_rows() == 0);                            /* Itch.io */
        ui_set_section(7);                                  /* More Games: a drawer of the player's own, and what is in it */
        CHECK(ui_rows() == 1 && ui_selected() && port_on_disk_only(ui_selected())
              && strcmp(ui_selected()->title, "Zeewolf") == 0 && strcmp(games_drawer_of(ui_selected()), "More Games") == 0);
        ui_set_section(6);                                  /* Exclusive: our own */
        CHECK(ui_rows() >= 1);
        for (i = 0; i < ui_rows(); i++) { ui_select_row(i); CHECK(ui_selected() && port_is_original(ui_selected())); }
        CHECK(n + 1 + 1 + ui_rows() == all);                /* every game is in one drawer */
        ui_set_section(8); CHECK(ui_section() == 6);        /* no such section */
        /* a drawer that goes while it is shown: back to All */
        ui_set_section(7);
        ui_set_drawers(on_drive, 2);
        CHECK(ui_sections() == 6 && ui_section() == 0 && ui_rows() == all && ui_find_section("More Games") == -1);
        ui_set_drawers(on_drive, 5);
        /* a click on a tab does the same */
        {
            olui_rect at;
            tabs = olui_find(ui, "sections");
            ui_set_section(2);
            CHECK(olui_layout(ui, w, h));
            CHECK(olui_box(ui, tabs, &at));
            olui_mouse(ui, (int)at.x + 12, (int)at.y + 8, OLUI_MOUSE_DOWN);
            olui_mouse(ui, (int)at.x + 12, (int)at.y + 8, OLUI_MOUSE_UP);
            CHECK(ui_section() == 0 && ui_rows() == all);
        }
        if (argc > 6 && atoi(argv[6]) == 0) ui_set_drawers(NULL, 0);   /* the picture: 0 no Games drive, else that section */
        else if (argc > 6) ui_set_section(atoi(argv[6]));
        for (i = 0; i < ui_rows(); i++) {
            ui_select_row(i);
            if (ui_selected() && strcmp(ui_selected()->title, "Kobo Deluxe") == 0) break;
        }
    }
    ui_status("4 sources: 415 of 415 shown, 1 to install");
    CHECK(olui_layout(ui, w, h));
    CHECK(olui_box(ui, olui_find(ui, "shot"), &r));

    out = fopen(argv[3], "w");
    CHECK(out != NULL);
    if (out) {
        fprintf(out, "size %d %d\n", w, h);
        olui_draw(ui, NULL, draw, NULL);
        fprintf(out, "shot %ld %ld %ld %ld\n", (long)r.x, (long)r.y, (long)r.w, (long)r.h);
        fclose(out);
    }
    printf("%d rows; picture box %ldx%ld at %ld,%ld; %s\n", rows, (long)r.w, (long)r.h, (long)r.x, (long)r.y,
           failures ? "FAILED" : "all passed");
    olui_free(ui);
    feed_free(feed);
    free(json);
    free(theme);
    free(tcss);
    return failures != 0;
}
