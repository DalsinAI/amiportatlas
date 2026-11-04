/* AmiPortAtlas: checking, installing, starting and removing a game.
 * MIT, (c) 2026 The AmiCradle Team. */
#ifndef AMIPORTATLAS_INSTALL_H
#define AMIPORTATLAS_INSTALL_H

#include "feed.h"
#include "http.h"

/* What this machine has installed through AmiPortAtlas, kept in
 * ENVARC:AmiPortAtlas/Installed (and ENV:), one "id<TAB>version<TAB>drawer"
 * line per game. */
void        installed_load(void);
const char *installed_version(const Port *p);  /* NULL: not installed */
const char *installed_path(const Port *p);     /* the game's drawer */
int         installed_found(const Port *p);    /* found on a disk, not installed by AmiPortAtlas */

/* Looks on DH0: to DH15:, three drawers deep, for games already installed:
 * a drawer named as a game (its feed drawer, title or id) with a program in
 * it, whose $VER gives the version. Each is remembered as found, so Play
 * starts it; Remove leaves it alone. tell() hears each disk and every few
 * drawers; answering 0 stops. Answers how many were found, -1 on no memory. */
typedef int (*ScanFn)(void *user, const char *where, int found);
int  install_scan(Port *const *ports, int n, ScanFn tell, void *user);

/* Installed games whose drawer or program has no icon: how many, or with
 * give, how many were given icon.library's default drawer and tool icons
 * (65536 stack). An icon already there is never touched. */
int  install_missing_icons(int give);

/* Can p be installed here into parent? Answers 1, or 0 with the reason the
 * player reads. Nothing is downloaded. */
int  install_check(const Port *p, const char *parent, char *why, int whylen);

/* The install steps, for the progress window. */
enum { STEP_DOWNLOAD, STEP_CHECK, STEP_UNPACK, STEP_INSTALL, STEP_TIDY, STEP_COUNT };
typedef void (*StepFn)(void *user, int step, long done, long total);

/* Downloads, checks, unpacks and runs the port's own Installer script into
 * parent, then records it. step() is told as each step starts and how a
 * download is going; answering 0 from progress stops it. */
int  install_run(const Port *p, const char *parent, StepFn step, HttpSink progress,
                 void *user, char *why, int whylen);

/* Starts an installed game as Workbench would. */
int  install_launch(const Port *p, char *why, int whylen);

/* Deletes the game's drawer and its icon and forgets it. */
int  install_remove(const Port *p, char *why, int whylen);

#endif
