"""tools/sources_index.py on small copies of Aminet's index and WHDLoad's table (tests/sources/).
MIT, (c) 2026 The AmiCradle Team."""
import json
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
import sources_index as si  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL", what)


lines = open(os.path.join(HERE, "sources", "aminet-INDEX.txt"), encoding="latin-1").read().splitlines()
a = si.aminet_entries(lines)
titles = [p["title"] for p in a]
check(titles == ["1497", "Creepy", "MineSweeper", "pig-m68k-amigaos", "Sploggy", "SuddenDeath"], "aminet games: %s" % titles)
creepy = [p for p in a if p["title"] == "Creepy"][0]
check(creepy["release_url"] == "https://aminet.net/package/game/shoot/Creepy", "aminet page")
check(creepy["category"] == "game/shoot" and creepy["short"].startswith("Arcade game"), "aminet facts")
check(creepy["download_size"] == 314 * 1024 and "download_url" not in creepy, "listed, not installable")
check(all(":" not in p["id"] and "/" not in p["id"] for p in a), "plain ids")
check(si.size_bytes("1.2M") == int(1.2 * 1024 * 1024) and si.size_bytes("?") == 0, "sizes")

w = si.whdload_entries(open(os.path.join(HERE, "sources", "whdload-all.html"), encoding="latin-1").read())
check([p["title"] for p in w] == ["1000 Miglia", "1000cc Turbo"], "whdload games: %s" % [p["title"] for p in w])
check(w[0]["author"] == "CFou! & StingRay" and w[0]["release_url"] == "https://www.whdload.de/games/1000Miglia.html", "whdload facts")

feeds = [open(os.path.join(HERE, "sources", f), encoding="utf-8").read() for f in ("itchio-tag-amiga.xml", "itchio-tag-aga.xml")]
it = si.itchio_entries(feeds)
check([p["title"] for p in it] == ["Dungeon & Dragons AGA", "Sky Rider"], "itch.io games, once each, Amiga only: %s" % [p["title"] for p in it])
sky = [p for p in it if p["title"] == "Sky Rider"][0]
check(sky["release_url"] == "https://maker-one.itch.io/sky-rider" and "download_url" not in sky, "itch.io page, listed only")
check(sky["author"] == "maker-one" and sky["category"] == "racing" and sky["year"] == "2025", "itch.io facts")
check(sky["short"] == "A fast racer for the A500 (free)", "itch.io line: %s" % sky["short"])
check(si.tag("<description><![CDATA[Capcom&#039;s game]]></description>", "description") == "Capcom's game", "itch.io text unescaped")
dd = [p for p in it if p["title"].startswith("Dungeon")][0]
check(dd["short"].endswith("(2.00€)") and dd["id"] == "itch-maker-three-dungeon-aga", "itch.io price and id")
check(all(p["id"].startswith("itch") for p in it), "itch ids go to the Itch.io tab")

with tempfile.TemporaryDirectory() as d:
    out = os.path.join(d, "a.json")
    r = subprocess.run([sys.executable, os.path.join(HERE, "..", "tools", "sources_index.py"), "aminet",
                        os.path.join(HERE, "sources", "aminet-INDEX.txt"), out], capture_output=True, text=True)
    doc = json.load(open(out, encoding="utf-8"))
    check(r.returncode == 0 and doc["schema_version"] == 1 and len(doc["ports"]) == 6, "the index file")
    out = os.path.join(d, "i.json")
    r = subprocess.run([sys.executable, os.path.join(HERE, "..", "tools", "sources_index.py"), "itchio"] +
                       [os.path.join(HERE, "sources", f) for f in ("itchio-tag-amiga.xml", "itchio-tag-aga.xml")] + [out],
                       capture_output=True, text=True)
    doc = json.load(open(out, encoding="utf-8"))
    check(r.returncode == 0 and doc["name"] == "itch.io: Amiga games" and len(doc["ports"]) == 2, "the itch.io index file")

print("sources_index: %s" % ("all passed" if not fails else "%d failed" % fails))
sys.exit(1 if fails else 0)
