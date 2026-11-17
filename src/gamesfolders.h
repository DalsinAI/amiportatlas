/* AmiPortAtlas: the Games drive's five drawers and which a game belongs in.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * Plain C with no Amiga calls and no SQLite, so the window, the index and
 * the host tests all use it. docs/GAMES.md has the rule.
 */
#ifndef AMIPORTATLAS_GAMESFOLDERS_H
#define AMIPORTATLAS_GAMESFOLDERS_H

#include "feed.h"

/* The Games drive's drawers, in the order they are shown. */
enum { GF_NONE = -1, GF_PORTS, GF_WHDLOAD, GF_ITCH, GF_AMINET, GF_EXCLUSIVE, GF_COUNT };

/* "Ports", "WHDLoad", "Itch.io", "Aminet", "Exclusive"; NULL for GF_NONE. */
const char *games_folder_name(int folder);

/* The drawer with that name, without case; GF_NONE when it is none of them. */
int games_folder_named(const char *name);

/* Where a source's games go: a source whose id is or starts with "whdload",
 * "itch" or "aminet" has its own drawer, the Port Atlas ("amicradle") is
 * Ports, and any other source has none (GF_NONE): its entries are indexed
 * but belong in no drawer of the Games drive. */
int games_folder_of_source(const char *source_id);

/* Where an entry goes: one of our own games (origin "original") is
 * Exclusive wherever it is listed; anything that is not a game has no
 * drawer; else its source's. */
int games_folder_of(const Port *p);

/* A game found in a drawer of the Games drive that no source lists: its
 * source is DISK_SOURCE and its category the name of the drawer at the top
 * of the drive it is in ("WHDLoad", or one the player made). */
#define DISK_SOURCE "disk"
int port_on_disk_only(const Port *p);

/* The name of the drawer at the top of the Games drive an entry is in or
 * goes to; NULL when it has none. */
const char *games_drawer_of(const Port *p);

/* A drawer that only sorts the games under it, as big sets have ("A", "B",
 * "0-9", "#"): a name of one character, or "0-9". Its drawers are the
 * games; it is not one itself. */
int games_shelf_drawer(const char *name);

#endif
