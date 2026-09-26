#!/usr/bin/env python3
"""Download the upstream PokeRogue files the skeleton draws, into an SD card layout.

    python3 fetch-assets.py [OUTPUT_DIR]

Writes OUTPUT_DIR/3ds/pokerogue/assets/... (default OUTPUT_DIR: ./sd-card).
Copy the contents of OUTPUT_DIR to the root of the 3DS SD card.

These are Pokemon sprites / game art owned by others: they are fetched from
upstream for personal use and must not be redistributed.
"""
import os
import sys
import urllib.request

REPO = "https://raw.githubusercontent.com/pagefaultgames/pokerogue-assets"
COMMIT = "056a1f408f26a3be4fef243f7462cb43608c7928"  # assets commit the skeleton was tested against

FILES = [
    "images/arenas/grass_bg.png",
    "images/arenas/grass_a.png",
    "images/arenas/grass_b.png",
    "images/pokemon/445.png",
    "images/pokemon/445.json",
    "images/pokemon/back/257.png",
    "images/pokemon/back/257.json",
    "images/ui/pbinfo_enemy_mini.png",
    "images/ui/pbinfo_player.png",
    "images/ui/overlay_hp.png",
    "images/ui/overlay_hp.json",
    "images/ui/overlay_exp.png",
    "images/ui/bg.png",
    "images/ui/bg.json",
    "images/ui/windows/window_1.png",
    "images/ui/cursor.png",
    "fonts/pokemon-emerald-pro.ttf",
]


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "sd-card"
    base = os.path.join(out, "3ds", "pokerogue", "assets")
    for rel in FILES:
        dst = os.path.join(base, *rel.split("/"))
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        url = f"{REPO}/{COMMIT}/{rel}"
        with urllib.request.urlopen(url) as r, open(dst, "wb") as f:
            f.write(r.read())
        print("ok", rel)
    print(f"done: copy everything inside '{out}' to the root of the SD card")


if __name__ == "__main__":
    main()
