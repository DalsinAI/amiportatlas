/* AmiPortAtlas: what differs between AmigaOS 3.2.3 / GFOS, AROS and AmigaOS 4.1.
 * MIT, (c) 2026 The AmiCradle Team.
 *
 * GFOS is AmigaOS 3.2.3 with GFOpenUp and the GFOpenKick ROM: the same 68k
 * binary as 3.2.3. AROS builds for m68k (the same ports run there) and for
 * x86 and ARM (the list and the release pages; installs once the feed has
 * builds for them). AmigaOS 4.1 builds for PPC.
 */
#ifndef AMIPORTATLAS_PLATFORM_H
#define AMIPORTATLAS_PLATFORM_H

#ifdef __amigaos4__
#define __USE_INLINE__
#endif

#include <exec/types.h>
#include <exec/execbase.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/dos.h>

#if defined(__amigaos4__)
#define AP_SYSTEM "AmigaOS 4.1"
#elif defined(__AROS__)
#define AP_SYSTEM "AROS"
#else
#define AP_SYSTEM "AmigaOS 3.2"
#endif

/* The archives in the feed hold 68k programs for AmigaOS 3.x. They install
 * where 68k programs run natively: AmigaOS 3.2.3, GFOS and AROS m68k. */
#if defined(__mc68000__) || defined(__m68k__) || defined(mc68000)
#define AP_RUNS_68K_PORTS 1
#else
#define AP_RUNS_68K_PORTS 0
#endif

/* Opens and closes every library the program uses (and, on AmigaOS 4, its
 * interface). Answers 0 and the missing library's name in *missing. */
int  platform_open(const char **missing);
void platform_close(void);

/* bsdsocket.library and gfopentls.library are opened when first needed,
 * since a TCP/IP stack may not be running yet. Answer 0 with a reason. */
int  platform_open_net(const char **why);

/* A Hook calling fn(hook, object, message) on every system. */
typedef ULONG (*HookFn)(struct Hook *, APTR, APTR);
void platform_hook(struct Hook *h, HookFn fn, APTR data);

/* The CPU and FPU, as the install checks need them (68k only; elsewhere 0). */
int  platform_cpu(void);       /* 68000, 68020, 68030, 68040, 68060 */
int  platform_has_fpu(void);

/* Free space on the volume holding path, in KB (no 4 GB overflow). -1: unknown. */
long platform_free_kb(CONST_STRPTR path);

/* Runs a command line and waits; answers its return code (-1: could not start). */
LONG platform_run(CONST_STRPTR command);

/* Starts a program as Workbench would from its icon (path without ".info"). */
int  platform_launch(CONST_STRPTR path);

/* Opens a web page in the user's browser (OpenURL, else GFOpenBrowser). */
void platform_open_url(CONST_STRPTR url);

#endif
