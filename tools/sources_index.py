#!/usr/bin/env python3
"""The readers for the sources that list games with only their pages (Dale, 17 Nov 2026: "list and link,
install later"): each turns a site's own listing into an index of the Port Atlas's form (docs/SOURCES.md), with
`release_url` the game's page and no download, so AmiPortAtlas shows them, says Get it... and opens the page.

    python3 tools/sources_index.py aminet  INDEX.gz|INDEX   data/aminet-game.json
    python3 tools/sources_index.py whdload all.html          data/whdload.json
    python3 tools/sources_index.py itchio  tag-*.xml ...     data/itchio.json

aminet:  Aminet's whole index (https://aminet.net/INDEX.gz): the game/ drawers, less data, hints, patches and
         editors, less builds for other systems (OS4, MorphOS, AROS x86, PPC) and sources.
whdload: the table of every install (https://www.whdload.de/games/all.html): the install's page; the game
         itself is the player's own (WHDLoad installs run the original disks).
itchio:  itch.io's own feeds of the games tagged for the Amiga (https://itch.io/games/tag-amiga.xml?page=N, and
         tag-amiga-500, tag-amiga-1200, tag-aga, tag-amiga-aga), every page, one game once: the game's page, its
         maker, price and year. A game whose builds are only for other systems (Windows, HTML...) is left out
         unless its name or line says it is for the Amiga.
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


ITEM = re.compile(r"<item>(.*?)</item>", re.S)
AMIGA_WORDS = re.compile(r"(^|[^a-z0-9])(amiga|aga|ocs|ecs|a500|a600|a1200|a4000|cd32|whdload|kickstart|amigaos)([^a-z0-9]|$)", re.I)


def tag(item, name):
    m = re.search(r"<%s>(.*?)</%s>" % (name, name), item, re.S)
    if not m:
        return ""
    text = m.group(1)
    c = re.match(r"^<!\[CDATA\[(.*)\]\]>$", text, re.S)
    return html.unescape(c.group(1) if c else text)


def itch_item(item):
    """One <item> of an itch.io feed as an entry; None for one that isn't for the Amiga."""
    page = tag(item, "link").strip() or tag(item, "guid").strip()
    m = re.match(r"^https://([a-z0-9-]+)\.itch\.io/([a-z0-9_-]+)$", page, re.I)
    if not m:
        return None
    title = tag(item, "plainTitle").strip() or tag(item, "title").strip()
    line = re.sub(r"<[^>]+>", "", tag(item, "description")).strip().split("\n")[0].strip()
    others = re.findall(r"<([a-z]+)>yes</\1>", tag(item, "platforms"))
    if others and not (AMIGA_WORDS.search(title) or AMIGA_WORDS.search(line)):
        return None
    kinds = re.findall(r"\[([^\]]+)\]", tag(item, "title"))
    price = tag(item, "price").strip()
    free = re.match(r"^\D*0+([.,]0+)?\D*$", price) is not None
    year = re.search(r"\b(19|20)\d\d\b", tag(item, "createDate"))
    e = {"id": plain_id("itch-" + m.group(1) + "-" + m.group(2)), "title": title, "kind": "game", "state": "released",
         "short": line or title, "author": m.group(1), "category": kinds[-1].lower() if len(kinds) > 1 else "itch.io",
         "release_url": page}
    e["short"] = (e["short"] + (" (free)" if free or not price else " (%s)" % price))[:200]
    if year:
        e["year"] = year.group(0)
    return e


def itchio_entries(texts):
    out, seen = [], set()
    for text in texts:
        for item in ITEM.findall(text):
            e = itch_item(item)
            if not e or e["id"] in seen:
                continue
            seen.add(e["id"])
            out.append(e)
    out.sort(key=lambda p: p["title"].lower())
    return out


def index(name, ports):
    return {"schema_version": 1, "generated": date.today().isoformat(), "name": name, "ports": ports}


def main(argv):
    if len(argv) >= 4 and argv[1] == "itchio":
        texts = [open(f, encoding="utf-8").read() for f in argv[2:-1]]
        return write(argv[-1], index("itch.io: Amiga games", itchio_entries(texts)))
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
    return write(dst, doc)


def write(dst, doc):
    with open(dst, "w", encoding="utf-8") as f:
        json.dump(doc, f, ensure_ascii=False, separators=(",", ":"))
        f.write("\n")
    print("%s: %d games" % (dst, len(doc["ports"])))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
