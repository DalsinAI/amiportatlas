/* AmiPortAtlas as a commodity: the commands and the tool types (src/cxcmd.h). MIT, (c) 2026 The AmiCradle Team. */
#include <stdio.h>
#include <string.h>
#include "../src/cxcmd.h"

static int fails, runs;
#define CHECK(c, what) do { runs++; if (!(c)) { fails++; printf("FAIL %s (line %d)\n", what, __LINE__); } } while (0)

int main(void)
{
    apcx_opts o;
    char longkey[100];

    apcx_defaults(&o);
    CHECK(o.pri == 0 && o.popup == 1 && !strcmp(o.popkey, "ctrl alt g"), "defaults: window at the start, ctrl alt g");
    CHECK(apcx_word(&o, "CX_POPUP=NO") && o.popup == 0, "CX_POPUP=NO");
    CHECK(apcx_word(&o, "cx_popup = yes") && o.popup == 1, "letter case and spaces");
    CHECK(apcx_word(&o, "CX_POPUP") && o.popup == 1, "CX_POPUP alone is yes");
    CHECK(apcx_word(&o, "CX_PRIORITY=-5") && o.pri == -5, "priority");
    CHECK(apcx_word(&o, "CX_PRIORITY=900") && o.pri == 127, "priority held to 127");
    CHECK(apcx_word(&o, "CX_PRIORITY=-900") && o.pri == -128, "priority held to -128");
    CHECK(apcx_word(&o, "CX_PRIORITY=high") && o.pri == -128, "a word that isn't a number leaves it");
    CHECK(apcx_word(&o, "CX_POPKEY=lamiga f1") && !strcmp(o.popkey, "lamiga f1"), "a hot key");
    CHECK(apcx_word(&o, "CX_POPKEY=NONE") && o.popkey[0] == 0, "NONE: no hot key");
    memset(longkey, 'a', sizeof longkey); memcpy(longkey, "CX_POPKEY=", 10); longkey[99] = 0;
    apcx_defaults(&o);
    CHECK(apcx_word(&o, longkey) && !strcmp(o.popkey, "ctrl alt g"), "a key too long is refused");
    CHECK(!apcx_word(&o, "CX_POPUPS=NO") && o.popup == 1, "another word is not ours");
    CHECK(!apcx_word(&o, "DONOTWAIT") && !apcx_word(&o, NULL), "Workbench's own tool types pass by");

    CHECK(apcx_do(APCX_APPEAR, 0, 0) == APCX_SHOW, "Show opens the window");
    CHECK(apcx_do(APCX_APPEAR, 1, 0) == APCX_SHOW, "Show with it open brings it to the front");
    CHECK(apcx_do(APCX_APPEAR, 0, 1) == 0, "not while disabled");
    CHECK(apcx_do(APCX_DISAPPEAR, 1, 0) == APCX_HIDE && apcx_do(APCX_DISAPPEAR, 0, 0) == 0, "Hide closes it, only when open");
    CHECK(apcx_do(APCX_DISABLE, 1, 0) == (APCX_OFF | APCX_HIDE), "Disable: broker off, window shut");
    CHECK(apcx_do(APCX_DISABLE, 0, 1) == 0, "Disable twice does nothing");
    CHECK(apcx_do(APCX_ENABLE, 0, 1) == APCX_ON && apcx_do(APCX_ENABLE, 0, 0) == 0, "Enable");
    CHECK(apcx_do(APCX_KILL, 1, 0) == APCX_QUIT && apcx_do(APCX_KILL, 0, 1) == APCX_QUIT, "Remove quits");
    CHECK(apcx_do(APCX_UNIQUE, 0, 0) == APCX_SHOW, "a second start shows the first one's window");
    CHECK(apcx_do(APCX_UNIQUE, 0, 1) == (APCX_ON | APCX_SHOW), "and turns it back on");
    CHECK(apcx_do(99, 0, 0) == 0, "an unknown command does nothing");
    printf("cx: %d of %d passed\n", runs - fails, runs);
    return fails != 0;
}
