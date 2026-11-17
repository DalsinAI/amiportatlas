/* AmiPortAtlas's full screen mode: the same games, sections and choice as the window, big, on a screen of its own,
 * for a joystick (or the cursor keys). Plain C on olui's own calls, as ui.c, so the host tests lay it out and draw it
 * (tests/big_host.c); app.c opens the screen and reads the joystick.
 *
 *   left / right   the section before / after (All, New, then the drawers found on Games:)
 *   up / down      the game before / after; held, it runs
 *   fire           Play an installed game, Install one that isn't
 *   fire 2 / Esc   back to the window
 * MIT, (c) 2026 The AmiCradle Team. */
#ifndef AMIPORTATLAS_BIG_H
#define AMIPORTATLAS_BIG_H

#include "gfopenlayout_ui.h"
#include "ui.h"

enum { BIG_NONE, BIG_LEFT, BIG_RIGHT, BIG_UP, BIG_DOWN, BIG_FIRE, BIG_BACK };
/* what big_input asks of the program */
enum { BIG_DID_NOTHING, BIG_MOVED, BIG_PLAY, BIG_INSTALL, BIG_LEAVE, BIG_PAGE };   /* BIG_PAGE: open its page (port_link_only) */

#define BIG_ROWS 11     /* games in the column at once */

/* Makes the full screen's contents in its own ui (after the theme's CSS), sharing the window's games (ui.c). */
int  big_build(olui *ui, UiHost *host, char *err, size_t errlen);
/* Shows the section, the column of games around the one chosen and its facts again. */
void big_fill(void);
/* One move of the stick or press of a button. */
int  big_input(int what);
/* The first row of the column for the chosen row, so it stays in the middle: pure, for the tests. */
int  big_first_row(int current, int rows, int shown);
/* A joystick reading (lowlevel.library's JPF_ bits as numbers: right 1<<24, left 1<<25, down 1<<26, up 1<<27,
 * fire 1<<22 (red), fire 2 1<<23 (blue)) against the last one: a new press, or a held direction repeated every
 * `repeat` ticks after `delay`. *held counts the ticks. Pure, for the tests. */
int  big_stick(unsigned long now, unsigned long last, int *held, int delay, int repeat);

#endif
