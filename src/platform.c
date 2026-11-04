/* AmiPortAtlas: libraries, hooks, CPU checks and launching, per system.
 * MIT, (c) 2026 The AmiCradle Team.
 */
#include "platform.h"

#include <string.h>
#include <stdio.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <workbench/workbench.h>
#include <intuition/intuitionbase.h>
#include <graphics/gfxbase.h>
#include <proto/intuition.h>
#include <proto/graphics.h>
#include <proto/gadtools.h>
#include <proto/datatypes.h>
#include <proto/wb.h>
#include <proto/asl.h>
#include <proto/icon.h>
#if defined(__has_include)
#if __has_include(<proto/bsdsocket.h>)
#include <proto/bsdsocket.h>   /* OpenNDK, NDK 3.2 */
#else
#include <proto/socket.h>      /* Roadshow SDK, AROS, AmigaOS 4 */
#endif
#else
#include <proto/socket.h>
#endif
#include <clib/alib_protos.h>

struct IntuitionBase *IntuitionBase;
struct GfxBase *GfxBase;
struct Library *GadToolsBase, *DataTypesBase, *WorkbenchBase, *AslBase, *IconBase;
struct Library *SocketBase;
#ifdef AP_HAVE_GFOPENTLS
struct Library *OpenTLSBase;
#endif

#ifdef __amigaos4__
struct IntuitionIFace *IIntuition;
struct GraphicsIFace *IGraphics;
struct GadToolsIFace *IGadTools;
struct DataTypesIFace *IDataTypes;
struct WorkbenchIFace *IWorkbench;
struct AslIFace *IAsl;
struct IconIFace *IIcon;
struct SocketIFace *ISocket;

static int iface(struct Library *base, struct Interface **out)
{
    if (!base) return 0;
    *out = GetInterface(base, "main", 1, NULL);
    return *out != NULL;
}
#define AP_OPEN(base, name, ver, ifc) \
    ((base = (void *)OpenLibrary(name, ver)) != NULL && iface((struct Library *)base, (struct Interface **)&ifc))
#define AP_CLOSE(base, ifc) do { if (ifc) DropInterface((struct Interface *)ifc); ifc = NULL; \
    if (base) CloseLibrary((struct Library *)base); base = NULL; } while (0)
#else
#define AP_OPEN(base, name, ver, ifc) ((base = (void *)OpenLibrary(name, ver)) != NULL)
#define AP_CLOSE(base, ifc) do { if (base) CloseLibrary((struct Library *)base); base = NULL; } while (0)
#endif

int platform_open(const char **missing)
{
    /* Version 39 (3.0) for GadTools listview hooks; workbench.library 44 for
     * OpenWorkbenchObjectA(). 3.2, AROS and 4.1 all have them. */
    if (!AP_OPEN(IntuitionBase, "intuition.library", 39, IIntuition)) { *missing = "intuition.library 39"; return 0; }
    if (!AP_OPEN(GfxBase, "graphics.library", 39, IGraphics)) { *missing = "graphics.library 39"; return 0; }
    if (!AP_OPEN(GadToolsBase, "gadtools.library", 39, IGadTools)) { *missing = "gadtools.library 39"; return 0; }
    if (!AP_OPEN(DataTypesBase, "datatypes.library", 39, IDataTypes)) { *missing = "datatypes.library 39"; return 0; }
    if (!AP_OPEN(WorkbenchBase, "workbench.library", 44, IWorkbench)) { *missing = "workbench.library 44"; return 0; }
    if (!AP_OPEN(AslBase, "asl.library", 39, IAsl)) { *missing = "asl.library 39"; return 0; }
    /* 44: GetDiskObject() and PutDiskObject() for found games' icons */
    if (!AP_OPEN(IconBase, "icon.library", 44, IIcon)) { *missing = "icon.library 44"; return 0; }
    return 1;
}

int platform_open_net(const char **why)
{
    if (!SocketBase && !AP_OPEN(SocketBase, "bsdsocket.library", 4, ISocket)) {
        *why = "No TCP/IP stack is running (bsdsocket.library could not be opened).";
        return 0;
    }
#ifdef AP_HAVE_GFOPENTLS
    if (!OpenTLSBase) {
        OpenTLSBase = OpenLibrary("gfopentls.library", 1);
        if (!OpenTLSBase) {
            *why = "AmiPortAtlas needs gfopentls.library 1 or newer (part of GFOpenUp).";
            return 0;
        }
    }
#endif
    return 1;
}

void platform_close(void)
{
#ifdef AP_HAVE_GFOPENTLS
    if (OpenTLSBase) CloseLibrary(OpenTLSBase);
    OpenTLSBase = NULL;
#endif
    AP_CLOSE(SocketBase, ISocket);
    AP_CLOSE(IconBase, IIcon);
    AP_CLOSE(AslBase, IAsl);
    AP_CLOSE(WorkbenchBase, IWorkbench);
    AP_CLOSE(DataTypesBase, IDataTypes);
    AP_CLOSE(GadToolsBase, IGadTools);
    AP_CLOSE(GfxBase, IGraphics);
    AP_CLOSE(IntuitionBase, IIntuition);
}

void platform_hook(struct Hook *h, HookFn fn, APTR data)
{
    memset(h, 0, sizeof *h);
#ifdef __amigaos4__
    h->h_Entry = (HOOKFUNC)fn;
#else
    /* amiga.lib's HookEntry moves A0/A2/A1 into C arguments (libamiga on AROS). */
    h->h_Entry = (HOOKFUNC)HookEntry;
    h->h_SubEntry = (HOOKFUNC)fn;
#endif
    h->h_Data = data;
}

int platform_cpu(void)
{
#if AP_RUNS_68K_PORTS
    UWORD f = SysBase->AttnFlags;
#ifdef AFF_68060
    if (f & AFF_68060) return 68060;
#else
    if (f & (1 << 7)) return 68060;
#endif
    if (f & AFF_68040) return 68040;
    if (f & AFF_68030) return 68030;
    if (f & AFF_68020) return 68020;
    if (f & AFF_68010) return 68010;
    return 68000;
#else
    return 0;
#endif
}

int platform_has_fpu(void)
{
#if AP_RUNS_68K_PORTS
    UWORD f = SysBase->AttnFlags;
    return (f & (AFF_68881 | AFF_68882 | AFF_FPU40)) != 0;
#else
    return 0;
#endif
}

long platform_free_kb(CONST_STRPTR path)
{
    struct InfoData *id;
    BPTR lock;
    long kb = -1;
    id = (struct InfoData *)AllocVec(sizeof(struct InfoData), MEMF_PUBLIC | MEMF_CLEAR);
    if (!id) return -1;
    lock = Lock(path, ACCESS_READ);
    if (lock) {
        if (Info(lock, id)) {
            ULONG freeb = (ULONG)(id->id_NumBlocks - id->id_NumBlocksUsed);
            ULONG bpb = (ULONG)id->id_BytesPerBlock;
            kb = (long)(freeb * (bpb / 256) / 4);
        }
        UnLock(lock);
    }
    FreeVec(id);
    return kb;
}

LONG platform_run(CONST_STRPTR command)
{
    BPTR out = Open("NIL:", MODE_NEWFILE);
    LONG rc;
    rc = SystemTags(command, SYS_Input, 0, SYS_Output, out, NP_StackSize, 32768, TAG_DONE);
    if (rc == -1 && out) Close(out);  /* SystemTags closes the handles only when it starts */
    return rc;
}

int platform_launch(CONST_STRPTR path)
{
    return OpenWorkbenchObjectA((STRPTR)path, NULL) != 0;
}

/* command run by a Shell in drawer dir (NULL: ours), waited for or not. A
 * command line is one line: the Shell reads no more of it. */
static LONG run_in(CONST_STRPTR dir, CONST_STRPTR command, long stack, int wait)
{
    BPTR in = Open((STRPTR)"NIL:", MODE_OLDFILE), out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    BPTR cd = dir ? Lock((STRPTR)dir, ACCESS_READ) : 0;
    LONG rc;
    if (!in || !out || (dir && !cd)) {
        if (in) Close(in);
        if (out) Close(out);
        if (cd) UnLock(cd);
        return -1;
    }
    rc = SystemTags(command, SYS_Input, in, SYS_Output, out, SYS_Asynch, wait ? FALSE : TRUE,
                    NP_StackSize, stack < 32768 ? 32768 : stack, cd ? NP_CurrentDir : TAG_IGNORE, cd, TAG_DONE);
    if (rc == -1) {                       /* not started: the handles and the lock are still ours */
        Close(in);
        Close(out);
        if (cd) UnLock(cd);
    }                                     /* started: the Shell's process frees all three as it ends */
    return rc;
}

LONG platform_run_in(CONST_STRPTR dir, CONST_STRPTR command)
{
    return run_in(dir, command, 65536, 1);
}

int platform_start(CONST_STRPTR drawer, CONST_STRPTR program, long stack)
{
    char cmd[300];
    snprintf(cmd, sizeof cmd, "\"%s\"", (const char *)program);
    return run_in(drawer, (CONST_STRPTR)cmd, stack < 65536 ? 65536 : stack, 0) != -1;
}

void platform_open_url(CONST_STRPTR url)
{
    char cmd[512];
    struct Library *OpenURLBase = OpenLibrary("openurl.library", 1);
    if (OpenURLBase) {
        CloseLibrary(OpenURLBase);
        snprintf(cmd, sizeof cmd, "C:OpenURL \"%s\"", url);
    } else {
        snprintf(cmd, sizeof cmd, "Run >NIL: SYS:Utilities/GFOpenBrowser \"%s\"", url);
    }
    platform_run(cmd);
}
