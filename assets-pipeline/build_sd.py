#!/usr/bin/env python3
"""Build the PokeRogue 3DS SD-card asset packs from upstream sources.

    python3 build_sd.py --assets PATH/pokerogue-assets --game PATH/pokerogue [--out sd-card] [--only gfx,snd,bgm,font,data]

--assets  checkout of github.com/pagefaultgames/pokerogue-assets
--game    checkout of github.com/pagefaultgames/pokerogue (for BGM loop points,
          locales submodule, and the font list); optional for gfx-only builds.
Output:   OUT/3ds/pokerogue/{gfx,snd,bgm,data}.pak + build-report.json
Copy the contents of OUT to the root of the SD card.

The packs contain Pokemon sprites, music and other art owned by others. They are
generated for personal use on your own console and must not be shared.
"""
import argparse
import json
import os
import sys
import time
from concurrent.futures import ProcessPoolExecutor

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import audio  # noqa: E402
import font  # noqa: E402
import gfx  # noqa: E402
import rpak  # noqa: E402

SKIP_IMAGE_EXT = {".mp4"}


def _gfx_job(args):
    png, rel, bg_mode = args
    try:
        data, st = gfx.convert_png(png, rel, bg_mode)
        return rel, data, st, None
    except Exception as e:  # report and keep going
        return rel, None, None, f"{type(e).__name__}: {e}"


def build_gfx(assets, out_dir, jobs, report):
    root = os.path.join(assets, "images")
    anim_bgs = gfx.anim_backgrounds(assets)
    todo, variant_maps, skipped = [], [], []
    for d, _, files in os.walk(root):
        for f in sorted(files):
            p = os.path.join(d, f)
            rel = os.path.relpath(p, root).replace(os.sep, "/")
            if f.endswith(".png"):
                todo.append((p, rel, gfx.anim_bg_mode(rel, anim_bgs)))
            elif f.endswith(".json"):
                if rel.startswith("pokemon/variant/") and not os.path.exists(p[:-5] + ".png"):
                    variant_maps.append((p, rel))
                elif not os.path.exists(p[:-5] + ".png"):
                    skipped.append(rel)
            elif os.path.splitext(f)[1] in SKIP_IMAGE_EXT:
                skipped.append(rel)
    w = rpak.Writer(os.path.join(out_dir, "gfx.pak"))
    stats = {"entries": 0, "stored": 0, "raw": 0, "tex_bytes": 0, "errors": [], "multipage": [], "direct": 0,
             "rgba8": 0}
    per_entry = {}
    t0 = time.time()
    with ProcessPoolExecutor(jobs) as ex:
        for i, (rel, data, st, err) in enumerate(ex.map(_gfx_job, todo, chunksize=16)):
            if err:
                stats["errors"].append([rel, err])
                continue
            stats["stored"] += w.add("images/" + rel[:-4], data, rpak.TYPE_TEX)
            stats["raw"] += len(data)
            stats["entries"] += 1
            stats["tex_bytes"] += st["tex_bytes"]
            per_entry[rel] = [st["tex_bytes"], len(data), st["frames"], st["unique"]]
            stats["direct"] += st["kind"] == gfx.KIND_DIRECT
            stats["rgba8"] += st["gpu"] == gfx.GPU_RGBA8
            if len(st["pages"]) > 1:
                stats["multipage"].append([rel, st["pages"]])
            if (i + 1) % 2000 == 0:
                print(f"  gfx {i + 1}/{len(todo)} ({time.time() - t0:.0f} s)", flush=True)
    for p, rel in variant_maps:
        js = json.load(open(p, encoding="utf-8"))
        if rel.endswith("_masterlist.json"):
            data = json.dumps(js, separators=(",", ":")).encode()
            stats["stored"] += w.add("images/" + rel, data, rpak.TYPE_JSON)
        else:
            data = gfx.convert_variant_map(js)
            stats["stored"] += w.add("images/" + rel, data, rpak.TYPE_VARIANT)
        stats["entries"] += 1
    w.close()
    stats["skipped"] = skipped
    json.dump(per_entry, open(os.path.join(out_dir, "gfx-entries.json"), "w"), separators=(",", ":"))
    stats["seconds"] = round(time.time() - t0, 1)
    report["gfx"] = stats
    print(f"gfx.pak: {stats['entries']} entries, {stats['stored'] / 1e6:.1f} MB on SD, "
          f"{len(stats['errors'])} errors, {stats['seconds']} s")


def find_encoder(explicit):
    """The dspadpcm host tool: --encoder, next to this script, or built with cc."""
    here = os.path.dirname(os.path.abspath(__file__))
    for p in [explicit, os.path.join(here, "tools", "dspadpcm"), os.path.join(here, "tools", "dspadpcm.exe")]:
        if p and os.path.exists(p):
            return p
    out = os.path.join(here, "tools", "dspadpcm")
    import subprocess
    for cc in ("cc", "gcc", "clang"):
        try:
            subprocess.run([cc, "-O2", "-o", out, os.path.join(here, "tools", "dspadpcm.c"), "-lm"], check=True)
            return out
        except (OSError, subprocess.CalledProcessError):
            continue
    raise SystemExit("need a C compiler to build tools/dspadpcm.c (or pass --encoder)")


def _bgm_job(args):
    enc, src, loop = args
    try:
        data, snr = audio.convert_bgm(enc, src, loop)
        return src, data, snr, None
    except Exception as e:
        return src, None, None, f"{type(e).__name__}: {e}"


def _sfx_job(args):
    enc, src = args
    try:
        data, snr = audio.convert_sfx(enc, src)
        return src, data, snr, None
    except Exception as e:
        return src, None, None, f"{type(e).__name__}: {e}"


AUDIO_EXT = (".mp3", ".m4a", ".wav", ".ogg")


def build_bgm(assets, game, out_dir, jobs, encoder, report):
    root = os.path.join(assets, "audio", "bgm")
    loops = audio.bgm_loop_points(game, assets)
    files = sorted(os.path.relpath(os.path.join(d, f), root).replace(os.sep, "/")
                   for d, _, fs in os.walk(root) for f in fs if f.lower().endswith(AUDIO_EXT))
    # keys keep subfolders: upstream plays e.g. "bw/evolution_fanfare"
    todo = [(encoder, os.path.join(root, f), loops.get(os.path.splitext(f)[0])) for f in files]
    w = rpak.Writer(os.path.join(out_dir, "bgm.pak"))
    st = {"entries": 0, "stored": 0, "errors": [], "snr_min": None, "with_loop": 0}
    t0 = time.time()
    with ProcessPoolExecutor(jobs) as ex:
        for i, (src, data, snr, err) in enumerate(ex.map(_bgm_job, todo)):
            if err:
                st["errors"].append([src, err])
                continue
            key = "audio/bgm/" + os.path.splitext(os.path.relpath(src, root).replace(os.sep, "/"))[0]
            st["stored"] += w.add(key, data, rpak.TYPE_SND, compress=False)  # streamed: must stay raw
            st["entries"] += 1
            st["with_loop"] += todo[i][2] is not None
            st["snr_min"] = snr if st["snr_min"] is None else min(st["snr_min"], snr)
            if (i + 1) % 20 == 0:
                print(f"  bgm {i + 1}/{len(todo)} ({time.time() - t0:.0f} s)", flush=True)
    w.close()
    st["seconds"] = round(time.time() - t0, 1)
    report["bgm"] = st
    print(f"bgm.pak: {st['entries']} tracks, {st['stored'] / 1e6:.1f} MB, {len(st['errors'])} errors, {st['seconds']} s")


def build_snd(assets, out_dir, jobs, encoder, report):
    todo = []
    for sub in ("se", "ui", "cry", "battle_anims"):
        root = os.path.join(assets, "audio", sub)
        for d, _, files in os.walk(root):
            for f in sorted(files):
                if f.lower().endswith(AUDIO_EXT):
                    todo.append((encoder, os.path.join(d, f)))
    w = rpak.Writer(os.path.join(out_dir, "snd.pak"))
    st = {"entries": 0, "stored": 0, "errors": [], "snr_min": None, "dupes": []}
    base = os.path.join(assets, "")
    t0 = time.time()
    with ProcessPoolExecutor(jobs) as ex:
        for src, data, snr, err in ex.map(_sfx_job, todo, chunksize=8):
            if err:
                st["errors"].append([src, err])
                continue
            key = os.path.splitext(os.path.relpath(src, base).replace(os.sep, "/"))[0]
            try:
                st["stored"] += w.add(key, data, rpak.TYPE_SND)
            except ValueError as e:  # same name, different extension (e.g. .wav and .WAV)
                st["dupes"].append(str(e))
                continue
            st["entries"] += 1
            st["snr_min"] = snr if st["snr_min"] is None else min(st["snr_min"], snr)
    w.close()
    st["seconds"] = round(time.time() - t0, 1)
    report["snd"] = st
    print(f"snd.pak: {st['entries']} sounds, {st['stored'] / 1e6:.1f} MB, {len(st['errors'])} errors, {st['seconds']} s")


def _font_job(args):
    path, name, size = args
    return name, size, font.bake(path, size)


def build_font(assets, out_dir, jobs, report):
    todo = [(os.path.join(assets, "fonts", f), name, size) for name, f in font.FONTS.items()
            if os.path.exists(os.path.join(assets, "fonts", f)) for size in font.SIZES]
    w = rpak.Writer(os.path.join(out_dir, "font.pak"))
    st = {"fonts": 0, "stored": 0, "pages": 0}
    t0 = time.time()
    with ProcessPoolExecutor(jobs) as ex:
        for name, size, (rfnt, pages, s) in ex.map(_font_job, todo):
            key = f"fonts/{name}@{size}"
            st["stored"] += w.add(key, rfnt, rpak.TYPE_FONT)
            for i, (tex, table) in enumerate(pages):
                st["stored"] += w.add(f"{key}/{i}", tex, rpak.TYPE_TEX)
                st["stored"] += w.add(f"{key}/{i}.glyphs", table, rpak.TYPE_BIN)
            st["fonts"] += 1
            st["pages"] += len(pages)
    w.close()
    st["seconds"] = round(time.time() - t0, 1)
    report["font"] = st
    print(f"font.pak: {st['fonts']} font sizes, {st['pages']} pages, {st['stored'] / 1e6:.1f} MB, {st['seconds']} s")


def _minify(path):
    txt = open(path, encoding="utf-8-sig").read()
    if path.endswith(".jsonc"):
        import re
        txt = re.sub(r"^\s*//.*$", "", txt, flags=re.M)
    return json.dumps(json.loads(txt), separators=(",", ":"), ensure_ascii=False).encode("utf-8")


def build_data(assets, game, locales, out_dir, report):
    """JSON data the game logic reads: battle-anims, root asset JSONs, locales, merged BGM loop points.
    Stored minified (JSON) and LZ4-compressed; keys keep upstream paths."""
    w = rpak.Writer(os.path.join(out_dir, "data.pak"))
    st = {"entries": 0, "stored": 0, "raw": 0, "errors": []}

    def add(key, data, type_=rpak.TYPE_JSON):
        st["stored"] += w.add(key, data, type_)
        st["raw"] += len(data)
        st["entries"] += 1

    for d, _, files in os.walk(os.path.join(assets, "battle-anims")):
        for f in sorted(files):
            if f.endswith(".json"):
                p = os.path.join(d, f)
                try:
                    add(os.path.relpath(p, assets).replace(os.sep, "/"), _minify(p))
                except ValueError as e:
                    st["errors"].append([p, str(e)])
    for f in sorted(os.listdir(assets)):
        if f.endswith(".json"):
            add(f, _minify(os.path.join(assets, f)))
    fonts = os.path.join(assets, "fonts")
    for f in sorted(os.listdir(fonts)) if os.path.isdir(fonts) else []:
        p = os.path.join(fonts, f)
        if f.endswith(".png"):
            data, _ = gfx.convert_png(p, "fonts/" + f)
            add("fonts/" + f[:-4], data, rpak.TYPE_TEX)
        elif f.endswith(".xml"):
            add("fonts/" + f, open(p, "rb").read(), rpak.TYPE_BIN)
    loops = audio.bgm_loop_points(game, assets)
    add("audio/bgm-loop-points.json", json.dumps(loops, separators=(",", ":")).encode())
    if locales and os.path.isdir(locales):
        for d, _, files in os.walk(locales):
            if ".git" in d or os.path.samefile(d, locales):
                continue  # root holds tooling configs (tsconfig, biome), not game text
            for f in sorted(files):
                if f.endswith((".json", ".jsonc")):
                    p = os.path.join(d, f)
                    try:
                        add("locales/" + os.path.relpath(p, locales).replace(os.sep, "/"), _minify(p))
                    except ValueError as e:
                        st["errors"].append([p, str(e)])
    w.close()
    report["data"] = st
    print(f"data.pak: {st['entries']} entries, {st['stored'] / 1e6:.1f} MB ({st['raw'] / 1e6:.1f} MB unpacked), "
          f"{len(st['errors'])} errors")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--assets", required=True)
    ap.add_argument("--game")
    ap.add_argument("--out", default="sd-card")
    ap.add_argument("--only", default="gfx,snd,bgm,font,data")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 2)
    ap.add_argument("--encoder", help="path to a built tools/dspadpcm")
    ap.add_argument("--locales", help="checkout of pokerogue-locales (default: GAME/locales)")
    a = ap.parse_args()
    only = set(a.only.split(","))
    out_dir = os.path.join(a.out, "3ds", "pokerogue")
    os.makedirs(out_dir, exist_ok=True)
    report_path = os.path.join(out_dir, "build-report.json")
    report = json.load(open(report_path)) if os.path.exists(report_path) else {}
    if "gfx" in only:
        build_gfx(a.assets, out_dir, a.jobs, report)
    if "data" in only:
        loc = a.locales or (os.path.join(a.game, "locales") if a.game else None)
        build_data(a.assets, a.game, loc, out_dir, report)
    if "font" in only:
        build_font(a.assets, out_dir, a.jobs, report)
    if "snd" in only or "bgm" in only:
        enc = find_encoder(a.encoder)
        if "snd" in only:
            build_snd(a.assets, out_dir, a.jobs, enc, report)
        if "bgm" in only:
            build_bgm(a.assets, a.game, out_dir, a.jobs, enc, report)
    json.dump(report, open(report_path, "w"), indent=1)


if __name__ == "__main__":
    main()
