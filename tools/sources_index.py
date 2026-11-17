#!/usr/bin/env python3
"""The readers for the sources that list games with only their pages (Dale, 17 Nov 2026: "list and link,
install later"): each turns a site's own listing into an index of the Port Atlas's form (docs/SOURCES.md), with
`release_url` the game's page and no download, so AmiPortAtlas shows them, says Get it... and opens the page.

    python3 tools/sources_index.py aminet  INDEX.gz|INDEX   data/aminet-game.json
    python3 tools/sources_index.py whdload all.html          data/whdload.json

aminet:  Aminet's whole index (https://aminet.net/INDEX.gz): the game/ drawers, less data, hints, patches and
         editors, less builds for other systems (OS4, MorphOS, AROS x86, PPC) and sources.
whdload: the table of every install (https://www.whdload.de/games/all.html): the install's page; the game
         itself is the player's own (WHDLoad installs run the original disks).
They run off the Amiga (where the Port Atlas is made); nothing is downloaded but the listing.
MIT, (c) 2026 The AmiCradle Team.
"""
import gzip
import html
import json
import re
import sys
from datetime import date

AMINET_SKIP_DIRS = {"game/data", "game/hint", "game/patch", "game/edit"}
OTHER_SYSTEMS = re.compile(r"(^|[^a-z])(os4|morphos|mos|aros|i386|x86|x86_64|ppc|warpos|wos|src|source)([^a-z]|$)", re.I)
PLAIN = re.compile(r"[^A-Za-z0-9._+-]")


def plain_id(name):
    """An id AmiPortAtlas takes: no ':' or '/', at most 100 characters."""
    return PLAIN.sub("_", name)[:100]


def size_bytes(text):
    m = re.match(r"^([0-9.]+)([KMG]?)$", text.strip())
    if not m:
        return 0
    return int(float(m.group(1)) * {"": 1, "K": 1024, "M": 1024 ** 2, "G": 1024 ** 3}[m.group(2)])


def aminet_line(line):
    """One line of Aminet's INDEX: name, drawer, size, age, short. None for a line that isn't one."""
    m = re.match(r"^(\S+)\s+([a-z]+/[a-z0-9]+)\s+([0-9.]+[KMG]?)\s+(\d+)\s(.*)$", line.rstrip("\r\n"))
    if not m:
        return None
    return {"file": m.group(1), "dir": m.group(2), "size": m.group(3), "short": m.group(5).strip()}


def aminet_entries(lines):
    out, seen = [], set()
    for line in lines:
        e = aminet_line(line)
        if not e or not e["dir"].startswith("game/") or e["dir"] in AMINET_SKIP_DIRS:
            continue
        base = re.sub(r"\.(lha|lzh|lzx|zip|tar\.gz|tgz|dms|adf)$", "", e["file"], flags=re.I)
        if OTHER_SYSTEMS.search(base) or OTHER_SYSTEMS.search(e["short"]):
            continue
        ident = plain_id(e["dir"].split("/")[1] + "-" + base)
        if ident in seen:
            continue
        seen.add(ident)
        out.append({"id": ident, "title": base, "kind": "game", "state": "released",
                    "short": e["short"], "category": e["dir"],
                    "release_url": "https://aminet.net/package/%s/%s" % (e["dir"], base),
                    "download_size": size_bytes(e["size"])})
    out.sort(key=lambda p: p["title"].lower())
    return out


ROW = re.compile(r'<TR><TD ALIGN=CENTER><A HREF="([^"/]+)\.lha">(.*?)</A></TD><TD><A HREF="[^"]*">Info</A></TD>'
                 r'<TD ALIGN=RIGHT>\d+</TD><TD ALIGN=CENTER>(\d{4}-\d\d-\d\d)</TD><TD ALIGN=RIGHT>\d+</TD>'
                 r'<TD ALIGN=CENTER>(.*?)</TD>', re.I)


def whdload_entries(text):
    out, seen = [], set()
    for m in ROW.finditer(text):
        base, title, when, by = m.group(1), m.group(2), m.group(3), m.group(4)
        title = html.unescape(re.sub(r"<[^>]+>", "", title)).strip()
        by = html.unescape(re.sub(r"<[^>]+>", "", by)).strip()
        ident = plain_id(base)
        if not title or ident in seen:
            continue
        seen.add(ident)
        out.append({"id": ident, "title": title, "kind": "game", "state": "released",
                    "short": "WHDLoad install (%s); the game is your own" % when,
                    "author": by, "category": "whdload",
                    "release_url": "https://www.whdload.de/games/%s.html" % base})
    out.sort(key=lambda p: p["title"].lower())
    return out


def index(name, ports):
    return {"schema_version": 1, "generated": date.today().isoformat(), "name": name, "ports": ports}


def main(argv):
    if len(argv) != 4 or argv[1] not in ("aminet", "whdload"):
        print(__doc__)
        return 2
    kind, src, dst = argv[1:]
    raw = open(src, "rb").read()
    if raw[:2] == b"\x1f\x8b":
        raw = gzip.decompress(raw)
    text = raw.decode("latin-1")
    if kind == "aminet":
        doc = index("Aminet: games", aminet_entries(text.splitlines()))
    else:
        doc = index("WHDLoad installs", whdload_entries(text))
    with open(dst, "w", encoding="utf-8") as f:
        json.dump(doc, f, ensure_ascii=False, separators=(",", ":"))
        f.write("\n")
    print("%s: %d games" % (dst, len(doc["ports"])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
