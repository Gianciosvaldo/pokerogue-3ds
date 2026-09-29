"""Fonts -> pre-rendered bitmap fonts (RFNT + RRT glyph pages).

Upstream draws text with TTF fonts at canvas sizes (96px = 16 logical px at
the 320x180 resolution the 3DS draws at 1:1). The 3DS app should not run
FreeType or keep the 5 MB CJK-capable font open, so every glyph is rendered
here once per size, 1-bit (crisp pixel-font look, as the skeleton's FreeType
path did), and packed into 256x256 pages grouped by codepoint order.

Entries per (font, size); the app only loads the pages a string touches:
  fonts/<font>@<size>             RFNT header + page index
  fonts/<font>@<size>/<n>         RRT texture of page n (indexed: 0 transparent, 1 white)
  fonts/<font>@<size>/<n>.glyphs  glyph records of page n

RFNT layout (little endian):
  "RFN1" u16 size, s16 ascent, s16 descent, s16 line_h, u32 glyphs, u16 pages, u16 0
  pages x {u32 first_codepoint, u32 last_codepoint}
Glyph record (16 bytes, sorted by codepoint; off_x/off_y from the pen position and the line top):
  u32 codepoint, u16 page, u16 x, u16 y, u8 w, u8 h, s8 off_x, s8 off_y, s8 advance, u8 0
"""
import struct

import numpy as np
from fontTools.ttLib import TTFont
from PIL import ImageFont, features

import gfx

PAGE = 256
# logical sizes: round(canvas px / 6) over every size upstream's UI code sets (5..16, 21)
SIZES = [5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 21]
FONTS = {  # upstream fontFamily -> file in assets/fonts
    "emerald": "pokemon-emerald-pro.ttf",
    "pkmnems": "pkmnems.ttf",
    "devanagari": "8-bit-devanagari.ttf",
    "rogue-thai": "rogue-thai.ttf",
    "terrible-thaifix": "terrible-thaifix.ttf",
}


def require_raqm():
    """Glyphs are placed with Raqm (HarfBuzz) layout, as browsers do. Pillow's basic layout uses hinted
    advances: about 5% of glyphs come out 1 px wider or narrower than the font's own widths, and some
    bitmaps shift. Pillow's Linux and macOS wheels include Raqm; on Windows it also needs fribidi-0.dll
    next to python.exe (sd-builder installs it)."""
    if not features.check("raqm"):
        raise RuntimeError("font.pak needs Pillow with Raqm text layout (on Windows, fribidi-0.dll next to "
                           "python.exe); without it glyph widths differ from upstream")


def bake(path, size):
    """Returns (rfnt_bytes, [(rrt page bytes, glyph table bytes)], stats)."""
    require_raqm()
    cps = sorted(c for c in TTFont(path).getBestCmap() if c >= 0x20)
    font = ImageFont.truetype(path, size, layout_engine=ImageFont.Layout.RAQM)
    ascent, descent = font.getmetrics()
    glyphs = []  # (cp, bitmap uint8 HxW, ox, oy, adv)
    for cp in cps:
        ch = chr(cp)
        try:
            mask, (ox, oy) = font.getmask2(ch, mode="1")
        except Exception:
            continue
        w, h = mask.size
        bm = np.array(mask, dtype=np.uint8).reshape(h, w) if w and h else np.zeros((0, 0), np.uint8)
        if bm.size and not bm.any():
            bm = np.zeros((0, 0), np.uint8)
        else:
            # trim blank rows/cols (getmask2 pads to the ink box already, but be safe)
            if bm.size:
                ys, xs = np.nonzero(bm.any(1))[0], np.nonzero(bm.any(0))[0]
                bm = bm[ys[0]:ys[-1] + 1, xs[0]:xs[-1] + 1]
                ox, oy = ox + xs[0], oy + ys[0]
        adv = int(round(font.getlength(ch)))
        glyphs.append((cp, (bm > 0).astype(np.uint8), ox, oy, adv))

    # shelf-pack in codepoint order into 256x256 pages, 1 px gutter
    pages, place = [], []
    x = y = shelf = 0
    cur = np.zeros((PAGE, PAGE), np.uint8)
    for cp, bm, ox, oy, adv in glyphs:
        h, w = bm.shape if bm.size else (0, 0)
        if w == 0:
            place.append((len(pages), 0, 0))
            continue
        if x + w > PAGE:
            x, y, shelf = 0, y + shelf + 1, 0
        if y + h > PAGE:
            pages.append(cur)
            cur = np.zeros((PAGE, PAGE), np.uint8)
            x = y = shelf = 0
        cur[y:y + h, x:x + w] = bm
        place.append((len(pages), x, y))
        x += w + 1
        shelf = max(shelf, h)
    pages.append(cur)

    tables = [bytearray() for _ in pages]
    ranges = [[None, None] for _ in pages]
    for (cp, bm, ox, oy, adv), (p, x, y) in zip(glyphs, place):
        h, w = bm.shape if bm.size else (0, 0)
        tables[p] += struct.pack("<IHHHBBbbbB", cp, p, x, y, w, h, max(-128, min(127, ox)),
                                 max(-128, min(127, oy)), max(-128, min(127, adv)), 0)
        if ranges[p][0] is None:
            ranges[p][0] = cp
        ranges[p][1] = cp
    line_h = ascent + descent
    rfnt = struct.pack("<4sHhhhIHH", b"RFN1", size, ascent, descent, line_h, len(glyphs), len(pages), 0)
    rfnt += b"".join(struct.pack("<II", a or 0, b or 0) for a, b in ranges)

    page_blobs = []
    for pg in pages:
        # trim the last page's unused height to the next power of two
        used = np.nonzero(pg.any(1))[0]
        hh = gfx.pow2(int(used[-1]) + 1) if len(used) else 8
        page_blobs.append(gfx.encode_raw_page(pg[:hh], [0, 0xFFFFFFFF]))
    page_blobs = list(zip(page_blobs, [bytes(t) for t in tables]))
    return rfnt, page_blobs, {"glyphs": len(glyphs), "pages": len(pages)}
