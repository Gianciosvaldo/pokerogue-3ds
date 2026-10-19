#!/usr/bin/env python3
"""PokeRogue 3DS SD card builder: the program behind BUILD-SD-CARD.bat.

Downloads the tools and PokeRogue's public files at pinned versions (pins.json), runs
assets-pipeline/build_sd.py to make the five packs, copies them to the SD card's
3ds/pokerogue folder, and checks for the sound firmware (3ds/dspfirm.cdc), putting the
DSP1 dumper on the card when it is missing. Every step is kept, so running it again
carries on where it stopped. Only the standard library is used here; the pipeline's
libraries are unpacked for build_sd.py alone.

    python builder.py [--sd PATH | --no-sd] [--format 3dsx|cia|both|folder] [--yes] [--jobs N] [--mirror DIR]

--sd PATH   use PATH as the SD card (any folder; tests use this) instead of asking
--no-sd     build the files on this PC only (they go to SD-CARD-FILES, to copy by hand)
--format    how the game goes on the card (it asks when this isn't given): 3dsx = one PokeRogue.3dsx with the packs
            inside, cia = one installable PokeRogue.cia, both = those two, folder = the app plus the pack files in
            3ds/pokerogue (the earlier way). With --yes and no --format, folder.
--yes       answer yes to every question (tests)
--mirror    fetch every download from DIR by file name instead of the internet (tests)
--existing-packs  skip the downloads and the conversion and use the packs already in files/build (tests)

The packs contain Pokemon and PokeRogue art and music: they are for your own console
and must never be shared.
"""
import argparse
import hashlib
import http.client
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import threading
import time
import urllib.error
import urllib.request
import zipfile

import bundle

FILES = os.path.dirname(os.path.abspath(__file__))  # the zip's files/ folder (sd-builder/ in the repo)
ROOT = os.path.dirname(FILES)  # the folder lorenzo extracted
PINS = json.load(open(os.path.join(FILES, "pins.json"), encoding="utf-8"))
PIPELINE = os.path.join(FILES, "pipeline")
RUNTIME = os.path.join(FILES, "runtime")
DOWNLOADS = os.path.join(FILES, "downloads")
BUILD = os.path.join(FILES, "build")  # build_sd.py's --out; its reports and our markers stay here
PACK_DIR = os.path.join(BUILD, "3ds", "pokerogue")
GAME = os.path.join(FILES, "game")  # the game app (pokerogue.3dsx, .elf, .smdh), its banner and makerom's spec
BUNDLE = os.path.join(BUILD, "bundle")  # the finished PokeRogue.3dsx and PokeRogue.cia
HAND_COPY = os.path.join(ROOT, "SD-CARD-FILES")  # only the files that go on the card, when copying by hand
LOG_PATH = os.path.join(ROOT, "build-log.txt")
WINDOWS = sys.platform == "win32"
MB = 1000 * 1000
GB = 1000 * MB

_log = open(LOG_PATH, "a", encoding="utf-8")
_args = None


class Stop(Exception):
    """A problem the user has to fix; the message says how."""


def log(msg):
    _log.write(f"{time.strftime('%Y-%m-%dT%H:%M:%S')}  {msg}\n")
    _log.flush()


def say(msg=""):
    print(" " + msg if msg else "", flush=True)
    log(msg)


def ask(question, default_yes=True):
    if _args.yes:
        return True
    while True:
        a = input(f" {question} {'[Y/n]' if default_yes else '[y/N]'} ").strip().lower()
        log(f"asked: {question} -> {a!r}")
        if not a:
            return default_yes
        if a in ("y", "yes", "s", "si"):
            return True
        if a in ("n", "no"):
            return False


def gb(n):
    return f"{n / GB:.1f} GB"


# ---------------------------------------------------------------- downloads


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def fetch(url, dest, what, sha256=None, size=None):
    """Downloads url to dest (kept between runs), resuming a partial file when the server allows it.
    Checked by SHA-256 when one is pinned."""
    if os.path.exists(dest) and (sha256 is None or sha256_of(dest) == sha256):
        log(f"have {dest}")
        return dest
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    part = dest + ".part"
    if _args.mirror:
        src = os.path.join(_args.mirror, os.path.basename(dest))
        shutil.copyfile(src, part)
    else:
        _download(url, part, what, size)
    if sha256 is not None and sha256_of(part) != sha256:
        os.remove(part)
        raise Stop(f"{what} came through damaged (its checksum is wrong). Run the builder again to retry.")
    os.replace(part, dest)
    return dest


def _download(url, part, what, size):
    say(f"Downloading {what}" + (f" ({size / MB:.0f} MB)" if size else "") + "...")
    log(f"GET {url}")
    for attempt in range(1, 6):
        have = os.path.getsize(part) if os.path.exists(part) else 0
        req = urllib.request.Request(url, headers={"User-Agent": "pokerogue-3ds-sd-builder"})
        if have:
            req.add_header("Range", f"bytes={have}-")
        try:
            with urllib.request.urlopen(req, timeout=60) as r:
                if have and r.status != 206:
                    have = 0  # no resume support: start over
                total = r.headers.get("Content-Length")
                total = int(total) + have if total else size
                with open(part, "ab" if have else "wb") as f:
                    done, last = have, 0.0
                    while True:
                        chunk = r.read(1 << 20)
                        if not chunk:
                            break
                        f.write(chunk)
                        done += len(chunk)
                        if time.time() - last > 1:
                            last = time.time()
                            pct = f"{100 * done / total:3.0f}%  " if total else ""
                            print(f"\r   {pct}{done / MB:,.0f} MB", end="", flush=True)
                print(f"\r   done, {done / MB:,.0f} MB" + " " * 12, flush=True)
                if total and done < total:
                    raise OSError(f"connection closed at {done} of {total} bytes")
                log(f"got {done} bytes")
                return
        except (OSError, http.client.HTTPException) as e:  # URLError and timeouts are OSErrors
            print()
            log(f"download attempt {attempt} failed: {e!r}")
            if isinstance(e, urllib.error.HTTPError) and e.code == 416:
                os.remove(part)  # our partial file is bad; start over
            if attempt == 5:
                raise Stop(f"Couldn't download {what}: {e}. Check the internet connection, then run the builder "
                           "again; it carries on from where it stopped.")
            say(f"The download was interrupted ({e}). Trying again in {5 * attempt} seconds...")
            time.sleep(5 * attempt)


def unpack_wheels():
    """The pipeline's libraries (numpy, Pillow, lz4, fontTools) go in runtime/lib, used only by build_sd.py.
    Elsewhere than Windows (tests), the running Python's own libraries are used."""
    if not WINDOWS:
        return ""
    lib = os.path.join(RUNTIME, "lib")
    marker = os.path.join(RUNTIME, "lib.ok")
    want = "\n".join(w["sha256"] for w in PINS["wheels"])
    if os.path.exists(marker) and open(marker).read() == want:
        return lib
    shutil.rmtree(lib, ignore_errors=True)
    say("Downloading the picture, font and compression libraries (24 MB)...")
    for w in PINS["wheels"]:
        name = os.path.basename(w["url"])
        path = fetch(w["url"], os.path.join(DOWNLOADS, name), name, w["sha256"])
        with zipfile.ZipFile(path) as z:
            z.extractall(lib)
    open(marker, "w").write(want)
    return lib


def install_fribidi():
    """fribidi-0.dll next to python.exe, so Pillow's Raqm text layout works and the fonts come out as on
    Linux (font.require_raqm). Elsewhere than Windows (tests), Pillow's wheel already includes it."""
    if not WINDOWS:
        return
    pin = PINS["fribidi"]
    dll = os.path.join(os.path.dirname(sys.executable), os.path.basename(pin["file"]))
    marker = os.path.join(RUNTIME, "fribidi.ok")
    if os.path.exists(dll) and os.path.exists(marker) and open(marker).read() == pin["sha256"]:
        return
    path = fetch(pin["url"], os.path.join(DOWNLOADS, os.path.basename(pin["url"])), "the text layout library",
                 pin["sha256"], pin["size"])
    with tarfile.open(path, "r:bz2") as t, t.extractfile(pin["file"]) as src, open(dll, "wb") as dst:
        shutil.copyfileobj(src, dst)
    open(marker, "w").write(pin["sha256"])


def unpack_ffmpeg():
    """ffmpeg.exe and ffprobe.exe, which the pipeline runs to decode PokeRogue's music and sounds.
    Elsewhere than Windows (tests), ffmpeg comes from PATH."""
    if not WINDOWS:
        return ""
    pin = PINS["ffmpeg"]
    bin_dir = os.path.join(RUNTIME, "ffmpeg")
    marker = os.path.join(RUNTIME, "ffmpeg.ok")
    if os.path.exists(marker) and open(marker).read() == pin["sha256"]:
        return bin_dir
    path = fetch(pin["url"], os.path.join(DOWNLOADS, os.path.basename(pin["url"])), "ffmpeg, the audio converter",
                 pin["sha256"], pin["size"])
    os.makedirs(bin_dir, exist_ok=True)
    with zipfile.ZipFile(path) as z:
        for name in z.namelist():
            rel = name.split("/", 1)[1] if "/" in name else name
            if rel in pin["files"]:
                with z.open(name) as src, open(os.path.join(bin_dir, os.path.basename(rel)), "wb") as dst:
                    shutil.copyfileobj(src, dst, 1 << 20)
    open(marker, "w").write(pin["sha256"])
    os.remove(path)  # 110 MB we no longer need
    return bin_dir


def fetch_makerom():
    """makerom, which builds the installable .cia. Pinned per platform (the Windows build here; Linux in tests)."""
    pin = PINS["makerom"]["windows" if WINDOWS else "linux"]
    dest = os.path.join(RUNTIME, "makerom")
    exe = os.path.join(dest, pin["file"])
    marker = exe + ".ok"
    if os.path.exists(exe) and os.path.exists(marker) and open(marker).read() == pin["sha256"]:
        return exe
    path = fetch(pin["url"], os.path.join(DOWNLOADS, os.path.basename(pin["url"])), "makerom, the maker of the "
                 "installable file", pin["sha256"], pin["size"])
    os.makedirs(dest, exist_ok=True)
    with zipfile.ZipFile(path) as z, z.open(pin["file"]) as src, open(exe, "wb") as dst:
        shutil.copyfileobj(src, dst)
    os.chmod(exe, 0o755)
    open(marker, "w").write(pin["sha256"])
    return exe


def work_dir():
    """Upstream's deepest path is 90 characters; keep the whole path under Windows' 260 limit."""
    wd = os.path.join(FILES, "work")
    if WINDOWS and len(wd) + 110 > 255:
        wd = os.path.join(os.environ.get("LOCALAPPDATA") or os.path.expanduser("~"), "PokeRogueSD")
        log(f"extracted folder path is long; unpacking upstream to {wd}")
    return wd


def fetch_upstream(name, what):
    """A codeload archive of an upstream repo at its pinned commit, unpacked (without the top folder) and
    checked against the pinned file count and size."""
    pin = PINS[name]
    dest = os.path.join(work_dir(), name)
    marker = dest + ".ok"
    if os.path.exists(marker) and open(marker).read() == pin["commit"]:
        return dest
    zpath = os.path.join(DOWNLOADS, f"{name}-{pin['commit'][:7]}.zip")
    fetch(pin["url"], zpath, what, size=pin.get("download_size"))
    say(f"Unpacking {what}...")
    tmp = dest + ".tmp"
    shutil.rmtree(tmp, ignore_errors=True)
    shutil.rmtree(dest, ignore_errors=True)
    count = size = 0
    try:
        with zipfile.ZipFile(zpath) as z:
            for info in z.infolist():
                rel = info.filename.split("/", 1)[1] if "/" in info.filename else ""
                if not rel or info.is_dir():
                    continue
                target = os.path.join(tmp, *rel.split("/"))
                os.makedirs(os.path.dirname(target), exist_ok=True)
                with z.open(info) as src, open(target, "wb") as dst:
                    shutil.copyfileobj(src, dst, 1 << 20)
                count += 1
                size += info.file_size
    except (zipfile.BadZipFile, EOFError) as e:
        os.remove(zpath)
        raise Stop(f"The {what} download was damaged ({e}). Run the builder again to download it again.")
    if (count, size) != (pin["files"], pin["bytes"]):
        log(f"{name}: {count} files, {size} bytes; expected {pin['files']}, {pin['bytes']}")
        os.remove(zpath)
        raise Stop(f"The {what} download isn't the expected version ({count} files instead of {pin['files']}). "
                   "Run the builder again; if this repeats, send build-log.txt in the project thread.")
    os.replace(tmp, dest)
    open(marker, "w").write(pin["commit"])
    os.remove(zpath)  # unpacked and checked: the archive is no longer needed
    log(f"{name}: {count} files, {size} bytes")
    return dest


def fetch_game_files():
    """The one game source file the pipeline reads (the music loop points in audio-manager.ts)."""
    game = os.path.join(work_dir(), "game")
    for rel, pin in PINS["game"]["files"].items():
        fetch(pin["url"], os.path.join(game, *rel.split("/")), os.path.basename(rel), pin["sha256"])
    return game


# ---------------------------------------------------------------- the SD card


def _win_drives():
    """[(letter, label, filesystem, free bytes, drive type, has a 3ds folder)] for drives with media."""
    import ctypes
    k32 = ctypes.windll.kernel32
    out = []
    mask = k32.GetLogicalDrives()
    for i in range(26):
        if not mask >> i & 1:
            continue
        root = f"{chr(65 + i)}:\\"
        dtype = k32.GetDriveTypeW(root)
        if dtype not in (2, 3):  # removable or fixed; skip network, CD and RAM drives
            continue
        label, fs = ctypes.create_unicode_buffer(261), ctypes.create_unicode_buffer(261)
        if not k32.GetVolumeInformationW(root, label, 261, None, None, None, fs, 261):
            continue  # empty card slot
        try:
            free = shutil.disk_usage(root).free
        except OSError:
            continue
        out.append((chr(65 + i), label.value, fs.value, free, dtype, os.path.isdir(root + "3ds")))
    return out


def _fs_of(root):
    if not WINDOWS:
        return "FAT32"
    import ctypes
    fs = ctypes.create_unicode_buffer(261)
    ok = ctypes.windll.kernel32.GetVolumeInformationW(os.path.splitdrive(root)[0] + "\\", None, 0, None, None, None,
                                                       fs, 261)
    return fs.value if ok else "?"


def choose_sd():
    """Returns the SD card's root folder, or None to build on this PC only."""
    if _args.no_sd:
        return None
    if _args.sd:
        return os.path.abspath(_args.sd)
    system = os.path.splitdrive(os.environ.get("SystemRoot", "C:\\Windows"))[0].upper()
    while True:
        say("Looking for your 3DS SD card...")
        drives = [d for d in _win_drives() if f"{d[0]}:" != system]
        likely = [d for d in drives if d[5]]
        for letter, label, fs, free, dtype, has3ds in drives:
            if has3ds or dtype == 2:
                note = "has a 3ds folder: this looks like your 3DS card" if has3ds else "no 3ds folder"
                say(f"   {letter}:  {label or '(no name)'}, {fs}, {gb(free)} free, {note}")
        if len(likely) == 1:
            prompt = (f"Press Enter to use {likely[0][0]}:, or type another drive letter "
                      "(or SKIP to put the files on the SD card yourself later):")
        else:
            if not likely:
                say("I can't see a drive with a 3ds folder. If the SD card isn't in the PC yet, put it in now, "
                    "wait a few seconds, and press Enter to look again.")
            prompt = "Type the SD card's drive letter (for example E), or SKIP to put the files on it yourself later:"
        if _args.yes:
            if len(likely) == 1:
                return likely[0][0] + ":\\"
            raise Stop("no single SD card found in --yes mode")
        answer = input(" " + prompt + " ").strip().rstrip(":\\/").upper()
        log(f"sd answer {answer!r}")
        if answer == "SKIP":
            return None
        if not answer:
            if len(likely) == 1:
                return likely[0][0] + ":\\"
            continue
        if len(answer) != 1 or not answer.isalpha():
            say("That isn't a drive letter. Type just the letter, for example E.")
            continue
        if f"{answer}:" == system:
            say(f"{answer}: is this PC's main drive, not the SD card.")
            continue
        match = [d for d in drives if d[0] == answer]
        if not match:
            say(f"There's no drive {answer}: with a card in it right now.")
            continue
        if not match[0][5] and not ask(f"{answer}: has no 3ds folder, which a modded 3DS's SD card always has. "
                                       "Use it anyway?", default_yes=False):
            continue
        return answer + ":\\"


def check_sd(sd):
    """Before the long build: the card is FAT32 (what the 3DS reads) and Windows can write to it."""
    fs = _fs_of(sd)
    log(f"sd {sd} filesystem {fs}")
    if fs.upper() != "FAT32" and not ask(f"This card is formatted as {fs}, and the 3DS only reads FAT32 cards. "
                                         "If your 3DS already uses this card, it's fine. Use it anyway?",
                                         default_yes=False):
        raise Stop("Stopped, so nothing was written to that card. Run the builder again and pick the 3DS's card.")
    probe = os.path.join(sd, "3ds", "pokerogue", "sd-builder-write-test.tmp")
    try:
        os.makedirs(os.path.dirname(probe), exist_ok=True)
        with open(probe, "wb") as f:
            f.write(b"ok")
        os.remove(probe)
    except OSError as e:
        log(f"write test failed: {e!r}")
        raise Stop(WRITE_PROTECTED)
    free = shutil.disk_usage(sd).free
    if free < PACKS_SIZE:
        say(f"Note: the SD card has {gb(free)} free and the files need about {gb(PACKS_SIZE)}. The build will go "
            "ahead; free some space on the card before it finishes copying.")


WRITE_PROTECTED = ("Windows can't write to the SD card. If the card (or its adapter) has a small lock switch on "
                   "its side, slide it up, away from LOCK, put the card back in and run the builder again.")
PACKS_SIZE = int(0.87 * GB)


# ---------------------------------------------------------------- building


def recipe():
    """What the packs depend on: upstream pins and the pipeline's own code. A change rebuilds them."""
    h = hashlib.sha256()
    for k in ("assets", "locales", "game"):
        h.update(json.dumps(PINS[k], sort_keys=True).encode())
    for d in (PIPELINE, os.path.join(PIPELINE, "tools")):
        for f in sorted(os.listdir(d)):
            if f.endswith((".py", ".exe", ".c")):
                h.update(f.encode() + open(os.path.join(d, f), "rb").read())
    return h.hexdigest()


STAGE_TEXT = {
    "data": "the game's text and data",
    "font": "the fonts",
    "gfx": "the pictures (the longest step with the music)",
    "snd": "the sound effects and cries",
    "bgm": "the music",
}


def build(assets, locales, game, lib, ffmpeg):
    os.makedirs(PACK_DIR, exist_ok=True)
    jobs = _args.jobs or max(1, (os.cpu_count() or 2) - 1)  # leave a core free so the PC stays usable
    encoder = os.path.join(PIPELINE, "tools", "dspadpcm.exe") if WINDOWS else _unix_encoder()
    env = dict(os.environ)
    env["PYTHONPATH"] = os.pathsep.join(p for p in (lib, PIPELINE) if p)
    if ffmpeg:
        env["PATH"] = ffmpeg + os.pathsep + env.get("PATH", "")
    want = recipe()
    stages = PINS["packs"]
    for n, stage in enumerate(stages, 1):
        pak = os.path.join(PACK_DIR, f"{stage}.pak")
        marker = os.path.join(PACK_DIR, f".{stage}.done")
        if os.path.exists(pak) and os.path.exists(marker) and open(marker).read() == want:
            say(f"[{n}/{len(stages)}] {STAGE_TEXT[stage]}: already done")
            continue
        say(f"[{n}/{len(stages)}] Converting {STAGE_TEXT[stage]}...")
        if os.path.exists(marker):
            os.remove(marker)
        cmd = [sys.executable, "-X", "utf8", os.path.join(PIPELINE, "build_sd.py"), "--assets", assets,
               "--game", game, "--locales", locales, "--out", BUILD, "--only", stage, "--jobs", str(jobs),
               "--encoder", encoder]
        log("run " + " ".join(cmd))
        t0 = time.time()
        _run_quietly(cmd, env, stage, t0)
        report = json.load(open(os.path.join(PACK_DIR, "build-report.json"), encoding="utf-8")).get(stage, {})
        errors = report.get("errors", [])
        expect = PINS.get("expect", {}).get(stage)
        got = report.get("entries", report.get("fonts")) or 0
        if errors:
            log(f"{stage} errors: {json.dumps(errors)[:4000]}")
        if expect is not None and got != expect:
            log(f"{stage}: {got} entries, expected {expect}")
        if expect and got < 0.99 * expect:  # the pinned files convert with no errors: this is a real problem
            if errors:
                say(f"   First problem: {errors[0][1]}")
            raise Stop(f"Converting {STAGE_TEXT[stage]} failed for {expect - got} of {expect} files. Run the "
                       "builder again to retry; if it fails the same way, send build-log.txt in the project thread.")
        if errors:
            say(f"   {len(errors)} files couldn't be converted (details in build-log.txt). The game will run, "
                "but those pictures or sounds will be missing.")
        open(marker, "w").write(want)
        log(f"{stage}.pak sha256 {sha256_of(pak)}")  # to compare with the tested build (sd-builder/README.md)
        say(f"   done in {(time.time() - t0) / 60:.0f} min, {os.path.getsize(pak) / MB:,.0f} MB")


def _unix_encoder():
    """Outside Windows (tests), the encoder is compiled from tools/dspadpcm.c with the system's C compiler."""
    exe = os.path.join(RUNTIME, "dspadpcm")
    if not os.path.exists(exe):
        os.makedirs(RUNTIME, exist_ok=True)
        subprocess.run(["cc", "-O2", "-o", exe, os.path.join(PIPELINE, "tools", "dspadpcm.c"), "-lm"], check=True)
    return exe


PROGRESS = re.compile(r"^\s+(?:gfx|bgm) (\d+)/(\d+) \(")


def _run_quietly(cmd, env, stage, t0):
    """Runs build_sd.py at below-normal priority; its output goes to the log, with a short
    'still working' line every minute so the window never looks frozen."""
    flags = 0x00004000 if WINDOWS else 0  # BELOW_NORMAL_PRIORITY_CLASS, inherited by its workers
    p = subprocess.Popen(cmd, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                         encoding="utf-8", errors="replace", creationflags=flags)
    tail = []
    last = [time.time()]

    def pump():
        for line in p.stdout:
            log(f"  {stage}| {line.rstrip()}")
            tail.append(line.rstrip())
            del tail[:-20]
            m = PROGRESS.match(line)
            if m:  # build_sd.py prints "  gfx 2000/16859 (40 s)" and "  bgm 20/178 (60 s)"
                print(f"   {100 * int(m.group(1)) // int(m.group(2))}% done", flush=True)
                last[0] = time.time()

    t = threading.Thread(target=pump, daemon=True)
    t.start()
    while p.poll() is None:
        time.sleep(1)
        if time.time() - last[0] >= 60:
            last[0] = time.time()
            say(f"   still working ({(time.time() - t0) / 60:.0f} min so far)...")
    t.join()
    if p.returncode != 0:
        say("   The converter stopped with an error. Its last lines:")
        for line in tail[-8:]:
            say("     " + line)
        raise Stop(f"Converting {STAGE_TEXT[stage]} failed. Run the builder again to retry; if it fails the "
                   "same way, send build-log.txt in the project thread.")


# ---------------------------------------------------------------- the single-file builds


def have_game():
    """True when this builder came with the game app (the zip does; builder.py run from the repo doesn't)."""
    return os.path.exists(os.path.join(GAME, "pokerogue.3dsx"))


FORMAT_MENU = (
    ("3dsx", "One .3dsx file (PokeRogue.3dsx) with everything inside. You start it from the Homebrew Launcher, "
             "like your other apps. Try this one first."),
    ("cia", "One installable file (PokeRogue.cia). You install it once with FBI, then start it from the HOME menu."),
    ("folder", "Loose files: the app plus the pack files in 3ds\\pokerogue, how the earlier builds worked."),
)


def choose_format():
    """"3dsx", "cia", "both" or "folder": how the game goes on the card. Asked before the long build."""
    if _args.format:
        if _args.format != "folder" and not have_game():
            raise Stop("This builder has no game app in it, so it can only make the loose files. Use the zip the "
                       "project thread gave you.")
        return _args.format
    if not have_game() or _args.yes:
        return "folder"
    say("How do you want the game on your 3DS?")
    for i, (_, text) in enumerate(FORMAT_MENU, 1):
        say(f"   {i}  {text}")
    while True:
        a = input(" Type 1, 2 or 3 and press Enter [1]: ").strip()
        log(f"format answer {a!r}")
        if not a or a == "1":
            return "3dsx"
        if a in ("2", "3"):
            return FORMAT_MENU[int(a) - 1][0]
        say("Please type 1, 2 or 3.")


def kinds_of(fmt):
    return {"3dsx": ["3dsx"], "cia": ["cia"], "both": ["3dsx", "cia"], "folder": []}[fmt]


def _packs():
    return [(f"{s}.pak", os.path.join(PACK_DIR, f"{s}.pak")) for s in PINS["packs"]]


def bundle_stamp(kind):
    """What a finished PokeRogue.<kind> depends on: the packs as they are now, the app and this tool's code."""
    h = hashlib.sha256(kind.encode())
    for f in (["pokerogue.3dsx"] if kind == "3dsx" else ["pokerogue.elf", "pokerogue.smdh", "banner.bnr", "pokerogue.rsf"]):
        h.update(f.encode() + sha256_of(os.path.join(GAME, f)).encode())
    h.update(sha256_of(os.path.join(FILES, "bundle.py")).encode())
    if kind == "cia":
        h.update(json.dumps(PINS["makerom"], sort_keys=True).encode())
    for name, path in _packs():
        st = os.stat(path)
        h.update(f"{name} {st.st_size} {st.st_mtime_ns}".encode())
    return h.hexdigest()


def bundle_game(kinds):
    """Makes PokeRogue.3dsx and/or PokeRogue.cia in build/bundle from the packs and the game app. Returns
    [(kind, path)]. A file whose inputs haven't changed is kept."""
    os.makedirs(BUNDLE, exist_ok=True)
    out = []
    size = sum(os.path.getsize(p) for _, p in _packs())
    for kind in kinds:
        dest = os.path.join(BUNDLE, f"PokeRogue.{kind}")
        marker = dest + ".stamp"
        stamp = bundle_stamp(kind)
        if os.path.exists(dest) and os.path.exists(marker) and open(marker).read() == stamp:
            say(f"PokeRogue.{kind}: already made")
            out.append((kind, dest))
            continue
        for f in (marker, dest):
            if os.path.exists(f):
                os.remove(f)
        free = shutil.disk_usage(BUNDLE).free
        if free < size + 100 * MB:
            raise Stop(f"This PC's drive needs {gb(size + 100 * MB)} free to make PokeRogue.{kind} and has "
                       f"{gb(free)}. Free some space, then run the builder again; nothing will be rebuilt.")
        try:
            bundle.check_fits(kind, size)
        except ValueError as e:
            raise Stop(f"PokeRogue.{kind} can't be made: {e}.")
        t0 = time.time()
        try:
            if kind == "3dsx":
                say("Making PokeRogue.3dsx: the app with the pack files inside it...")
                last = [0.0]

                def progress(done, size=size, last=last):
                    if time.time() - last[0] > 1:
                        last[0] = time.time()
                        print(f"\r   {100 * done // size}%", end="", flush=True)

                bundle.make_3dsx(os.path.join(GAME, "pokerogue.3dsx"), _packs(), dest + ".part", progress)
                print("\r   done" + " " * 10, flush=True)
                os.replace(dest + ".part", dest)
            else:
                _make_cia(dest)
        except OSError as e:
            log(f"making PokeRogue.{kind} failed: {e!r}")
            if getattr(e, "winerror", None) == 112 or getattr(e, "errno", None) == 28:
                raise Stop("This PC's drive is full. Free some space, then run the builder again; nothing will be "
                           "rebuilt.")
            raise Stop(f"Making PokeRogue.{kind} failed ({e}). Run the builder again; if it fails the same way, "
                       "send build-log.txt in the project thread.")
        except (RuntimeError, ValueError) as e:
            log(f"making PokeRogue.{kind} failed: {e}")
            raise Stop(f"Making PokeRogue.{kind} failed: {e}. Run the builder again; if it fails the same way, send "
                       "build-log.txt in the project thread.")
        open(marker, "w").write(stamp)
        log(f"PokeRogue.{kind}: {os.path.getsize(dest)} bytes in {time.time() - t0:.0f} s")
        out.append((kind, dest))
    return out


def _make_cia(dest):
    makerom = fetch_makerom()
    say("Making PokeRogue.cia: the installable file...")
    work = os.path.join(BUILD, "cia-work")
    shutil.rmtree(work, ignore_errors=True)
    os.makedirs(work)
    for f in ("pokerogue.rsf", "pokerogue.elf", "pokerogue.smdh", "banner.bnr"):
        shutil.copyfile(os.path.join(GAME, f), os.path.join(work, f))
    bundle.stage_romfs_dir(os.path.join(work, "romfs"), _packs())
    try:
        made = bundle.make_cia(makerom, work, "PokeRogue.cia", log)
        os.replace(made, dest)
    finally:
        shutil.rmtree(work, ignore_errors=True)  # the pack links, and the 850 MB copies on a drive without links


# ---------------------------------------------------------------- copying


def sd_items(fmt, outputs):
    """What goes on the card for this format: [(file on this PC, path inside the card, name to show)]."""
    items = []
    if fmt == "folder":
        items += [(path, os.path.join("3ds", "pokerogue", name), name) for name, path in _packs()]
        if have_game():
            items.append((os.path.join(GAME, "pokerogue.3dsx"), os.path.join("3ds", "PokeRogue.3dsx"), "PokeRogue.3dsx"))
    for kind, path in outputs:
        items.append((path, os.path.join("3ds", "PokeRogue.3dsx") if kind == "3dsx" else os.path.join("cias", "PokeRogue.cia"),
                      f"PokeRogue.{kind}"))
    items += [(os.path.join(FILES, "apps", f), os.path.join("3ds", f), f) for f in _apps()]
    return items


def copy_to_sd(sd, items):
    try:
        return _copy_to_sd(sd, items)
    except OSError as e:
        log(f"copy failed: {e!r}")
        if getattr(e, "winerror", None) == 19 or isinstance(e, PermissionError):
            raise Stop(WRITE_PROTECTED)
        if getattr(e, "winerror", None) == 112 or getattr(e, "errno", None) == 28:
            raise Stop("The SD card is full. Free some space on it, then run the builder again (nothing will be "
                       "rebuilt).")
        raise Stop(f"Copying to the SD card failed ({e}). Check the card is still in the PC and run the builder "
                   "again; nothing will be rebuilt.")


def _copy_to_sd(sd, items):
    need = sum(os.path.getsize(src) for src, _, _ in items)
    while not os.path.isdir(sd):
        say(f"The SD card ({sd}) isn't in the PC any more.")
        if _args.yes or not ask("Put it back in, wait a few seconds, then press Enter. (Type n to stop here "
                                "and copy the files yourself.)"):
            return False
    have = sum(os.path.getsize(os.path.join(sd, rel)) for _, rel, _ in items if os.path.exists(os.path.join(sd, rel)))
    free = shutil.disk_usage(sd).free
    if free + have < need + 20 * MB:
        raise Stop(f"The SD card needs {gb(need - have + 20 * MB)} free and has {gb(free)}. Free some space on it "
                   "(for example old screenshots or videos), then run the builder again. Nothing will be rebuilt.")
    say(f"Copying the files to the SD card ({need / MB:,.0f} MB)...")
    for src, rel, name in items:
        dst = os.path.join(sd, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        size = os.path.getsize(src)
        if os.path.exists(dst) and os.path.getsize(dst) == size and _same_file(src, dst):
            say(f"   {name}: already on the card")
            continue
        part = dst + ".part"
        with open(src, "rb") as a, open(part, "wb") as b:
            done, last = 0, 0.0
            while True:
                chunk = a.read(4 << 20)
                if not chunk:
                    break
                b.write(chunk)
                done += len(chunk)
                if time.time() - last > 1:
                    last = time.time()
                    print(f"\r   {name}: {100 * done / size:3.0f}%", end="", flush=True)
            b.flush()
            os.fsync(b.fileno())
        if os.path.getsize(part) != size:
            raise Stop(f"Copying {name} to the SD card went wrong. Check the card has space and run the builder again.")
        os.replace(part, dst)
        print(f"\r   {name}: copied" + " " * 10, flush=True)
        log(f"copied {name} ({size} bytes) to {dst}")
    return True


def _apps():
    """.3dsx files shipped in the zip's files/apps folder, to go in the card's 3ds folder."""
    d = os.path.join(FILES, "apps")
    return sorted(f for f in os.listdir(d) if f.endswith(".3dsx")) if os.path.isdir(d) else []


def make_hand_copy(items):
    """SD-CARD-FILES: exactly what goes on the card, for copying by hand. Hard links where the drive allows them,
    so it takes no extra space."""
    shutil.rmtree(HAND_COPY, ignore_errors=True)
    files = [(src, os.path.join(HAND_COPY, rel)) for src, rel, _ in items]
    try:
        pin = PINS["dsp1"]
        files.append((fetch(pin["url"], os.path.join(DOWNLOADS, "DSP1.3dsx"), "DSP1, the sound firmware dumper",
                            pin["sha256"], pin["size"]), os.path.join(HAND_COPY, "3ds", "DSP1.3dsx")))
    except Stop as e:
        log(f"DSP1 download failed: {e}")
    for src, dst in files:
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        try:
            os.link(src, dst)
        except OSError:
            shutil.copyfile(src, dst)


def _same_file(a, b):
    """Same size is checked first; then the first and last 4 MB, which a half-finished copy wouldn't match."""
    with open(a, "rb") as fa, open(b, "rb") as fb:
        if fa.read(4 << 20) != fb.read(4 << 20):
            return False
        fa.seek(-min(4 << 20, os.path.getsize(a)), 2)
        fb.seek(-min(4 << 20, os.path.getsize(b)), 2)
        return fa.read() == fb.read()


def check_sound(sd):
    """Returns "ready" when 3ds/dspfirm.cdc is on the card; otherwise puts DSP1 in the 3ds folder and returns
    "dsp1", or "missing" if DSP1 couldn't be downloaded."""
    cdc = os.path.join(sd, "3ds", "dspfirm.cdc")
    if os.path.exists(cdc) and os.path.getsize(cdc) > 0:
        return "ready"
    pin = PINS["dsp1"]
    try:
        path = fetch(pin["url"], os.path.join(DOWNLOADS, "DSP1.3dsx"), "DSP1, the sound firmware dumper",
                     pin["sha256"], pin["size"])
        shutil.copyfile(path, os.path.join(sd, "3ds", "DSP1.3dsx"))
        return "dsp1"
    except (Stop, OSError) as e:
        log(f"DSP1 failed: {e!r}")
        return "missing"


# ---------------------------------------------------------------- main


def main():
    global _args
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sd")
    ap.add_argument("--no-sd", action="store_true")
    ap.add_argument("--format", choices=["3dsx", "cia", "both", "folder"])
    ap.add_argument("--yes", action="store_true")
    ap.add_argument("--jobs", type=int)
    ap.add_argument("--mirror")
    ap.add_argument("--existing-packs", action="store_true")
    _args = ap.parse_args()
    if WINDOWS:
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(1)  # no "insert a disk" pop-ups from empty card slots
    log(f"=== start {sys.version.split()[0]} {sys.platform} cpus={os.cpu_count()} root={ROOT}")

    say("")
    say("PokeRogue SD card builder")
    say("=========================")
    say("This makes the game's pictures, sounds, music and text for your 3DS from PokeRogue's")
    say("public files, and puts the game on your SD card. It downloads about 700 MB and takes roughly")
    say("20 to 60 minutes. You can use the PC meanwhile. If it stops, just run it again.")
    say("The files it makes are for your own console only. Please don't share them.")
    say("")

    t0 = time.time()
    try:
        if "onedrive" in ROOT.lower() and not ask(
                "This folder is inside OneDrive, which would upload about 2 GB of files to the cloud. It's better "
                "to move the PokeRogue-SD-Builder folder to Downloads first and run it from there. Carry on here "
                "anyway?", default_yes=False):
            raise Stop("Move the PokeRogue-SD-Builder folder out of OneDrive (for example to Downloads), then "
                       "double-click BUILD-SD-CARD there.")
        sd = choose_sd()
        fmt = choose_format()
        log(f"format {fmt}")
        need = 3 * GB if fmt == "folder" else 4 * GB  # a single file is a second copy of the packs
        free = shutil.disk_usage(ROOT).free
        if free < need and not os.path.exists(os.path.join(PACK_DIR, ".bgm.done")):
            raise Stop(f"This PC's drive needs about {gb(need)} free for the build and has {gb(free)}. Free some "
                       "space (or move this folder to a drive with more), then run the builder again.")
        if sd:
            check_sd(sd)
            say(f"Using the SD card at {sd}. You can leave it in the PC while this runs.")
        else:
            say("The files will be made in this folder, in SD-CARD-FILES, for you to copy later.")
        say("")
        if _args.existing_packs:
            missing = [n for n, p in _packs() if not os.path.exists(p)]
            if missing:
                raise Stop(f"--existing-packs: {', '.join(missing)} not in {PACK_DIR}")
        else:
            lib = unpack_wheels()
            install_fribidi()
            ffmpeg = unpack_ffmpeg()
            assets = fetch_upstream("assets", "PokeRogue's pictures and sounds (about 530 MB)")
            locales = fetch_upstream("locales", "PokeRogue's text in every language (about 4 MB)")
            game = fetch_game_files()
            say("")
            say("Converting everything for the 3DS. This is the long part.")
            build(assets, locales, game, lib, ffmpeg)
            say("")
        outputs = bundle_game(kinds_of(fmt))
        items = sd_items(fmt, outputs)
        copied = copy_to_sd(sd, items) if sd else False
        sound = check_sound(sd) if copied else None
        if not copied:
            make_hand_copy(items)
    except Stop as e:
        say("")
        say(f"STOPPED: {e}")
        log("=== stopped")
        return 1
    except KeyboardInterrupt:
        say("")
        say("Stopped. Run the builder again to carry on from here.")
        return 1
    except Exception as e:  # anything unexpected: keep the details for the thread
        import traceback
        log(traceback.format_exc())
        say("")
        say(f"STOPPED by an unexpected error: {type(e).__name__}: {e}")
        say("Please send build-log.txt (it's in this folder) in the project thread.")
        return 1

    say("")
    say(f"Done in {(time.time() - t0) / 60:.0f} minutes.")
    if copied:
        _how_to_start(sd, fmt)
        if sound == "ready":
            say("Sound: ready (dspfirm.cdc is already on the card).")
        else:
            say("")
            say("One more step for sound, done once on the 3DS itself:")
            if sound == "missing":
                say("  0. DSP1 couldn't be downloaded. Get DSP1.3dsx from github.com/zoogie/DSP1/releases")
                say("     and copy it into the SD card's 3ds folder.")
            say("  1. Eject the SD card (see below), put it in the 3DS and turn it on.")
            say("  2. Open the Homebrew Launcher and start DSP1.")
            say("  3. When it says it's done, press START to go back. That's it: sound now works.")
            say("  (DSP1 copies the console's own sound firmware to the SD card as 3ds\\dspfirm.cdc.)")
        say("")
        say("Before you take the SD card out, eject it: in File Explorer, right-click the card's drive")
        say("and choose Eject. Then put it back in your 3DS.")
    else:
        say(f"The files are in {HAND_COPY}.")
        say("To put them on the SD card yourself: copy the folders in SD-CARD-FILES (3ds, and cias if it has one)")
        say("onto the SD card, and say yes if Windows asks to merge them with the card's own folders.")
        _how_to_start(None, fmt)
        say("If the card has no 3ds\\dspfirm.cdc file, start DSP1 once from the Homebrew Launcher for sound.")
    say("")
    say("Keep these files to yourself: they're only for your own console.")
    log("=== done")
    return 0


def _how_to_start(sd, fmt):
    """What to do on the 3DS, for the format that was made."""
    if fmt == "folder":
        if sd:
            say(f"The game's files are on the SD card, in {os.path.join(sd, '3ds', 'pokerogue')}.")
        say("To play: open the Homebrew Launcher on the 3DS and start PokeRogue 3DS.")
        return
    if "3dsx" in kinds_of(fmt):
        say("PokeRogue.3dsx is in the SD card's 3ds folder: one file, with everything inside.")
        say("To play: put the card in the 3DS, open the Homebrew Launcher and start PokeRogue 3DS.")
    if "cia" in kinds_of(fmt):
        say("PokeRogue.cia is in the SD card's cias folder: one file, with everything inside.")
        say("To install it: put the card in the 3DS, open FBI, choose SD, then the cias folder, then PokeRogue.cia,")
        say("then \"Install and delete CIA\". The card needs about 1 GB free while it installs. After that,")
        say("start PokeRogue 3DS from the HOME menu.")
    if sd:
        old = [p for p in PINS["packs"] if os.path.exists(os.path.join(sd, "3ds", "pokerogue", f"{p}.pak"))]
        if len(old) == len(PINS["packs"]):
            say("The five pack files already in the card's 3ds\\pokerogue folder (about 850 MB) aren't needed any more;")
            say("you can delete them to get the space back. Your saves are in 3ds\\pokerogue\\save: keep that folder.")


if __name__ == "__main__":
    sys.exit(main())
