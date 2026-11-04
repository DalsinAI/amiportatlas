#!/usr/bin/env python3
"""Draws tests/ui_host's window as a PNG, to look at it on the host:

    python3 tools/render_ui.py ops.txt window.png GFOPENLAYOUT/tests/ui_render.py [picture.png]

GFOpenLayout's own ui_render.py draws the boxes, text and marks; this adds
what the Amiga program draws itself: the list's state marks (img ops) and
the selected game's picture in its box. Not the Amiga's drawing.
MIT, (c) 2026 The AmiCradle Team.
"""
import os
import subprocess
import sys
import tempfile

from PIL import Image, ImageDraw

MARKS = {1: (46, 157, 91), 2: (58, 110, 168), 3: (224, 160, 48), 4: (181, 190, 201), 5: (217, 165, 32)}


def main():
    ops, out, renderer = sys.argv[1], sys.argv[2], sys.argv[3]
    picture = sys.argv[4] if len(sys.argv) > 4 else None
    images, shot, keep = [], None, []
    for line in open(ops, encoding="utf-8"):
        f = line.split()
        if f and f[0] == "image":
            images.append(tuple(map(int, f[1:6])))
        elif f and f[0] == "shot":
            shot = tuple(map(int, f[1:5]))
        else:
            keep.append(line)
    with tempfile.NamedTemporaryFile("w", suffix=".txt", delete=False, encoding="utf-8") as t:
        t.writelines(keep)
    subprocess.run([sys.executable, renderer, t.name, out], check=True)
    os.unlink(t.name)
    img = Image.open(out).convert("RGBA")
    d = ImageDraw.Draw(img)
    for x, y, w, h, n in images:
        s = min(w, h, 10)
        cx, cy = x + w // 2, y + h // 2
        d.ellipse((cx - s // 2, cy - s // 2, cx + s // 2, cy + s // 2), fill=MARKS.get(n, (0, 0, 0)))
    if shot and picture:
        x, y, w, h = shot
        p = Image.open(picture).convert("RGBA")
        k = min(w / p.width, h / p.height)
        p = p.resize((max(1, int(p.width * k)), max(1, int(p.height * k))), Image.NEAREST)
        img.paste(p, (x + (w - p.width) // 2, y + (h - p.height) // 2))
    img.save(out)


main()
