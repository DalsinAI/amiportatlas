/* AmiPortAtlas: the window, described in HTML and CSS for GFOpenLayout UI.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * Plain C on olui's own calls, with no Amiga calls, so the host tests lay
 * the real window out and draw it (tests/ui_host.c). What needs the system
 * (pictures, installing, starting a game, a web page) the program gives as
 * UiHost callbacks.
 */
#ifndef AMIPORTATLAS_UI_H
#define AMIPORTATLAS_UI_H

#include "gfopenlayout_ui.h"
#include "feed.h"

/* Picture numbers for the list's state marks (olui img numbers). */
enum { MARK_AVAILABLE = 1, MARK_INSTALLED, MARK_UPDATE, MARK_COMING, MARK_ORIGINAL };

typedef struct UiHost {
    void *data;
    const char *(*installed)(void *data, const Port *p);      /* its version, or NULL */
    void (*picture)(void *data, const Port *p, int shot);     /* show picture shot of p (p NULL: none) */
    void (*install)(void *data, Port *p);                     /* Install..., Update... */
    void (*launch)(void *data, Port *p);                      /* Play, double-clicks */
    void (*remove)(void *data, Port *p);
    void (*open_url)(void *data, const char *url);
    void (*refresh)(void *data);                              /* Check for updates */
} UiHost;

/* Makes the window's contents in ui (after the theme's CSS is added). */
int  ui_build(olui *ui, UiHost *host, char *err, size_t errlen);

/* A new feed (the old one is the caller's to free afterwards). */
void ui_set_feed(Feed *feed);
/* The list again, as after an install or a removal. */
void ui_refill(void);

void ui_status(const char *utf8);
void ui_progress(int percent);         /* -1 hides the gauge */
void ui_busy(int busy);                /* buttons off while installing */

/* Every 100 ms: the pictures cycle every 5 s. */
void ui_tick(void);

Port *ui_selected(void);
void  ui_select_row(int row);
int   ui_rows(void);

#endif
