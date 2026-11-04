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
