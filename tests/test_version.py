"""The version is the same everywhere it is written: both programs' $VER, the systray's name, the
web agent and the HOWTO's archive name. MIT, (c) 2026 The AmiCradle Team."""
import os
import re
import sys

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")


def read(p):
    return open(os.path.join(ROOT, p), encoding="latin-1").read()


found = {
    "AmiPortAtlas $VER": re.findall(r"\$VER: AmiPortAtlas (\d+\.\d+\.\d+) \(", read("src/app.c")),
    "systray name": re.findall(r'"AmiPortAtlas (\d+\.\d+\.\d+)"', read("src/app.c")),
    "Atlas $VER": re.findall(r"\$VER: Atlas (\d+\.\d+\.\d+) \(", read("src/atlas.c")),
    "web agent": re.findall(r"AmiPortAtlas/(\d+\.\d+\.\d+) ", read("src/http.c")),
    "HOWTO archive": re.findall(r"AmiPortAtlas-(\d+\.\d+\.\d+)\.lha", read("docs/HOWTO.md")),
}
fails = [k for k, v in found.items() if len(v) != 1]
versions = {v[0] for v in found.values() if v}
if len(versions) != 1:
    fails.append("versions differ: %s" % found)
dates = set(re.findall(r"\$VER: \w+ [\d.]+ \((\d\d\.\d\d\.\d{4})\)", read("src/app.c") + read("src/atlas.c")))
if len(dates) != 1:
    fails.append("dates differ: %s" % dates)
print("version: %s" % ("all passed (%s)" % versions.pop() if not fails else "FAIL %s" % fails))
sys.exit(1 if fails else 0)
