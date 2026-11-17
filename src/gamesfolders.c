/* AmiPortAtlas: the Games drive's five drawers. MIT, (c) 2026 The AmiCradle Team.
 * Plain C with no Amiga calls and no SQLite. */
#include <string.h>
#include <ctype.h>
#include "gamesfolders.h"

static const char *const folder_names[GF_COUNT] = { "Ports", "WHDLoad", "Itch.io", "Aminet", "Exclusive" };

const char *games_folder_name(int folder)
{
    return folder >= 0 && folder < GF_COUNT ? folder_names[folder] : NULL;
}

static int same_nocase(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    return *a == *b;
}

int games_folder_named(const char *name)
{
    int i;
    if (!name) return GF_NONE;
    for (i = 0; i < GF_COUNT; i++)
        if (same_nocase(name, folder_names[i])) return i;
    return GF_NONE;
}

static int starts(const char *s, const char *with)
{
    return strncmp(s, with, strlen(with)) == 0;
}

int games_folder_of_source(const char *id)
{
    if (!id) return GF_NONE;
    if (starts(id, "whdload")) return GF_WHDLOAD;
    if (starts(id, "itch")) return GF_ITCH;
    if (starts(id, "aminet")) return GF_AMINET;
    if (strcmp(id, ATLAS_SOURCE) == 0) return GF_PORTS;
    return GF_NONE;
}

static int is_game(const Port *p)
{
    return !p->kind || !*p->kind || strcmp(p->kind, "game") == 0;
}

int port_on_disk_only(const Port *p)
{
    return p->source && strcmp(p->source, DISK_SOURCE) == 0;
}

const char *games_drawer_of(const Port *p)
{
    if (port_on_disk_only(p)) return p->category && *p->category ? p->category : NULL;
    return games_folder_name(games_folder_of(p));
}

int games_folder_of(const Port *p)
{
    if (port_on_disk_only(p)) return games_folder_named(p->category);
    if (!is_game(p)) return GF_NONE;
    if (port_is_original(p)) return GF_EXCLUSIVE;
    return games_folder_of_source(p->source ? p->source : ATLAS_SOURCE);
}

int games_install_drawer(const Port *p, const char *drive, char *out, int max)
{
    const char *name = port_on_disk_only(p) ? NULL : games_folder_name(games_folder_of(p));
    size_t d = drive ? strlen(drive) : 0, n = name ? strlen(name) : 0;
    int slash = d && drive[d - 1] != ':' && drive[d - 1] != '/';
    if (max > 0) out[0] = 0;
    if (!d || !n || d + (size_t)slash + n + 1 > (size_t)max) return 0;
    memcpy(out, drive, d);
    if (slash) out[d++] = '/';
    memcpy(out + d, name, n + 1);
    return 1;
}

int games_shelf_drawer(const char *name)
{
    return name && ((name[0] && !name[1]) || strcmp(name, "0-9") == 0);
}
