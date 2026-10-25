#!/usr/bin/env python3
"""build_sd.py --extra-images: PNGs from a folder laid out like assets/images go into gfx.pak, and replace an upstream file
of the same name. Uses tiny made-up pictures, no upstream files.

    python3 tests/extra_images_test.py
"""
import json
import os
import sys
import tempfile

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import build_sd  # noqa: E402


def png(path, size, colour):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    Image.new("RGBA", size, colour).save(path)


def entries(out):
    names = []
    data = open(os.path.join(out, "gfx.pak"), "rb").read()
    for name in ("images/trainer/custom/kira", "images/trainer/youngster", "images/trainer/custom/wamuu"):
        if name.encode() in data:
            names.append(name)
    return names


def main():
    with tempfile.TemporaryDirectory() as t:
        assets, extra, plain, out = (os.path.join(t, d) for d in ("assets", "extra", "plain", "out"))
        os.makedirs(os.path.join(assets, "battle-anims"))
        png(os.path.join(assets, "images", "trainer", "youngster.png"), (40, 40), (255, 0, 0, 255))
        png(os.path.join(extra, "trainer", "custom", "kira.png"), (82, 91), (0, 0, 255, 255))
        png(os.path.join(extra, "trainer", "custom", "wamuu.png"), (66, 53), (0, 255, 0, 255))
        os.makedirs(out)
        report = {}
        build_sd.build_gfx(assets, out, 1, report)
        assert report["gfx"]["entries"] == 1 and not report["gfx"]["errors"], report["gfx"]
        assert entries(out) == ["images/trainer/youngster"], entries(out)
        report = {}
        build_sd.build_gfx(assets, out, 1, report, extra)
        assert report["gfx"]["entries"] == 3 and not report["gfx"]["errors"], report["gfx"]
        assert sorted(entries(out)) == ["images/trainer/custom/kira", "images/trainer/custom/wamuu", "images/trainer/youngster"], entries(out)
        # a file of the same name replaces upstream's: still one entry for it
        png(os.path.join(extra, "trainer", "youngster.png"), (40, 40), (9, 9, 9, 255))
        report = {}
        build_sd.build_gfx(assets, out, 1, report, extra)
        assert report["gfx"]["entries"] == 3, report["gfx"]["entries"]
    print("extra_images_test: OK")


if __name__ == "__main__":
    main()
