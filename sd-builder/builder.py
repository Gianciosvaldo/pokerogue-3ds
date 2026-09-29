#!/usr/bin/env python3
"""PokeRogue 3DS SD card builder: the program behind BUILD-SD-CARD.bat.

Downloads the tools and PokeRogue's public files at pinned versions (pins.json), runs
assets-pipeline/build_sd.py to make the five packs, copies them to the SD card's
3ds/pokerogue folder, and checks for the sound firmware (3ds/dspfirm.cdc), putting the
DSP1 dumper on the card when it is missing. Every step is kept, so running it again
carries on where it stopped. Only the standard library is used here; the pipeline's
libraries are unpacked for build_sd.py alone.

    python builder.py [--sd PATH | --no-sd] [--yes] [--jobs N] [--mirror DIR]

--sd PATH   use PATH as the SD card (any folder; tests use this) instead of asking
--no-sd     build the files on this PC only (they go to SD-CARD-FILES, to copy by hand)
--yes       answer yes to every question (tests)
--mirror    fetch every download from DIR by file name instead of the internet (tests)

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
import threading
import time
import urllib.error
import urllib.request
import zipfile

FILES = os.path.dirname(os.path.abspath(__file__))  # the zip's files/ folder (sd-builder/ in the repo)
ROOT = os.path.dirname(FILES)  # the folder lorenzo extracted
PINS = json.load(open(os.path.join(FILES, "pins.json"), encoding="utf-8"))
PIPELINE = os.path.join(FILES, "pipeline")
RUNTIME = os.path.join(FILES, "runtime")
DOWNLOADS = os.path.join(FILES, "downloads")
BUILD = os.path.join(FILES, "build")  # build_sd.py's --out; its reports and our markers stay here
PACK_DIR = os.path.join(BUILD, "3ds", "pokerogue")
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


# ---------------------------------------------------------------- copying


def copy_to_sd(sd):
    try:
        return _copy_to_sd(sd)
    except OSError as e:
        log(f"copy failed: {e!r}")
        if getattr(e, "winerror", None) == 19 or isinstance(e, PermissionError):
            raise Stop(WRITE_PROTECTED)
        if getattr(e, "winerror", None) == 112 or getattr(e, "errno", None) == 28:
            raise Stop("The SD card is full. Free some space on it, then run the builder again (nothing will be "
                       "rebuilt).")
        raise Stop(f"Copying to the SD card failed ({e}). Check the card is still in the PC and run the builder "
                   "again; nothing will be rebuilt.")


def _copy_to_sd(sd):
    dest = os.path.join(sd, "3ds", "pokerogue")
    paks = [f"{s}.pak" for s in PINS["packs"]]
    need = sum(os.path.getsize(os.path.join(PACK_DIR, p)) for p in paks)
    while not os.path.isdir(sd):
        say(f"The SD card ({sd}) isn't in the PC any more.")
        if _args.yes or not ask("Put it back in, wait a few seconds, then press Enter. (Type n to stop here "
                                "and copy the files yourself.)"):
            return False
    have = sum(os.path.getsize(os.path.join(dest, p)) for p in paks if os.path.exists(os.path.join(dest, p)))
    free = shutil.disk_usage(sd).free
    if free + have < need + 20 * MB:
        raise Stop(f"The SD card needs {gb(need - have + 20 * MB)} free and has {gb(free)}. Free some space on it "
                   "(for example old screenshots or videos), then run the builder again. Nothing will be rebuilt.")
    os.makedirs(dest, exist_ok=True)
    say(f"Copying the files to the SD card ({need / MB:,.0f} MB)...")
    for p in paks:
        src, dst = os.path.join(PACK_DIR, p), os.path.join(dest, p)
        size = os.path.getsize(src)
        if os.path.exists(dst) and os.path.getsize(dst) == size and _same_file(src, dst):
            say(f"   {p}: already on the card")
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
                    print(f"\r   {p}: {100 * done / size:3.0f}%", end="", flush=True)
            b.flush()
            os.fsync(b.fileno())
        if os.path.getsize(part) != size:
            raise Stop(f"Copying {p} to the SD card went wrong. Check the card has space and run the builder again.")
        os.replace(part, dst)
        print(f"\r   {p}: copied" + " " * 10, flush=True)
        log(f"copied {p} ({size} bytes) to {dst}")
    for f in _apps():
        shutil.copyfile(os.path.join(FILES, "apps", f), os.path.join(sd, "3ds", f))
        say(f"   {f}: put in the 3ds folder")
    return True


def _apps():
    """.3dsx files shipped in the zip's files/apps folder, to go in the card's 3ds folder."""
    d = os.path.join(FILES, "apps")
    return sorted(f for f in os.listdir(d) if f.endswith(".3dsx")) if os.path.isdir(d) else []


def make_hand_copy():
    """SD-CARD-FILES: exactly what goes on the card (the packs, the zip's apps and DSP1), for copying by hand.
    Hard links where the drive allows them, so it takes no extra space."""
    shutil.rmtree(HAND_COPY, ignore_errors=True)
    dest = os.path.join(HAND_COPY, "3ds", "pokerogue")
    os.makedirs(dest)
    files = [(os.path.join(PACK_DIR, f"{s}.pak"), os.path.join(dest, f"{s}.pak")) for s in PINS["packs"]]
    files += [(os.path.join(FILES, "apps", f), os.path.join(HAND_COPY, "3ds", f)) for f in _apps()]
    try:
        pin = PINS["dsp1"]
        files.append((fetch(pin["url"], os.path.join(DOWNLOADS, "DSP1.3dsx"), "DSP1, the sound firmware dumper",
                            pin["sha256"], pin["size"]), os.path.join(HAND_COPY, "3ds", "DSP1.3dsx")))
    except Stop as e:
        log(f"DSP1 download failed: {e}")
    for src, dst in files:
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
    ap.add_argument("--yes", action="store_true")
    ap.add_argument("--jobs", type=int)
    ap.add_argument("--mirror")
    _args = ap.parse_args()
    if WINDOWS:
        import ctypes
        ctypes.windll.kernel32.SetErrorMode(1)  # no "insert a disk" pop-ups from empty card slots
    log(f"=== start {sys.version.split()[0]} {sys.platform} cpus={os.cpu_count()} root={ROOT}")

    say("")
    say("PokeRogue SD card builder")
    say("=========================")
    say("This makes the game's pictures, sounds, music and text for your 3DS from PokeRogue's")
    say("public files, and puts them on your SD card. It downloads about 700 MB and takes roughly")
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
        free = shutil.disk_usage(ROOT).free
        if free < 3 * GB and not os.path.exists(os.path.join(PACK_DIR, ".bgm.done")):
            raise Stop(f"This PC's drive needs about 3 GB free for the build and has {gb(free)}. Free some space "
                       "(or move this folder to a drive with more), then run the builder again.")
        sd = choose_sd()
        if sd:
            check_sd(sd)
            say(f"Using the SD card at {sd}. You can leave it in the PC while this runs.")
        else:
            say("The files will be made in this folder, in SD-CARD-FILES, for you to copy later.")
        say("")
        lib = unpack_wheels()
        ffmpeg = unpack_ffmpeg()
        assets = fetch_upstream("assets", "PokeRogue's pictures and sounds (about 530 MB)")
        locales = fetch_upstream("locales", "PokeRogue's text in every language (about 4 MB)")
        game = fetch_game_files()
        say("")
        say("Converting everything for the 3DS. This is the long part.")
        build(assets, locales, game, lib, ffmpeg)
        say("")
        copied = copy_to_sd(sd) if sd else False
        sound = check_sound(sd) if copied else None
        if not copied:
            make_hand_copy()
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
        say(f"The game's files are on the SD card, in {os.path.join(sd, '3ds', 'pokerogue')}.")
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
        say("To put them on the SD card yourself: copy the 3ds folder from SD-CARD-FILES onto the SD card,")
        say("and say yes if Windows asks to merge it with the card's own 3ds folder.")
        say("If the card has no 3ds\\dspfirm.cdc file, start DSP1 once from the Homebrew Launcher for sound.")
    say("")
    say("Keep these files to yourself: they're only for your own console.")
    log("=== done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
