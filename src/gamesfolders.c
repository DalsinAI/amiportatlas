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

static int same_word(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        int x = (unsigned char)*a, y = (unsigned char)*b;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (y >= 'A' && y <= 'Z') y += 32;
        if (x != y) return 0;
    }
    return *a == *b;
}

static int begins(const char *s, const char *with)
{
    for (; *with; s++, with++) {
        int x = (unsigned char)*s;
        if (x >= 'A' && x <= 'Z') x += 32;
        if (x != *with) return 0;
    }
    return 1;
}

/* not a game's own start: a read-me, a guide, an installer */
static int not_a_start(const char *name)
{
    static const char *const no[] = { "readme", "read me", "read_me", "liesmich", "install", "uninstall", "setup",
                                      "manual", "guide", "docs", "doc", "license", "licence", "copying", "history",
                                      "changes", "faq", "help", "config", "prefs", "edit", NULL };
    int i;
    size_t n = strlen(name);
    if (n > 6 && same_word(name + n - 6, ".guide")) return 1;
    if (n > 4 && (same_word(name + n - 4, ".txt") || same_word(name + n - 4, ".doc") || same_word(name + n - 4, ".pdf")
                  || same_word(name + n - 4, ".htm"))) return 1;
    if (n > 5 && same_word(name + n - 5, ".html")) return 1;
    for (i = 0; no[i]; i++) if (begins(name, no[i])) return 1;
    return 0;
}

int games_pick_start(const char *drawer, const char *const *names, const int *tool, int n)
{
    int i, tools = 0, last = -1;
    if (!names || !tool || n <= 0) return -1;
    if (drawer)
        for (i = 0; i < n; i++) if (tool[i] && names[i] && same_word(names[i], drawer)) return i;
    if (drawer)
        for (i = 0; i < n; i++) if (!tool[i] && names[i] && same_word(names[i], drawer)) return i;
    for (i = 0; i < n; i++) if (tool[i] && names[i]) { tools++; last = i; }
    if (tools == 1) return last;
    for (i = 0; i < n; i++) if (tool[i] && names[i] && !not_a_start(names[i])) return i;
    for (i = 0; i < n; i++) if (!tool[i] && names[i] && !not_a_start(names[i])) return i;
    return -1;
}
