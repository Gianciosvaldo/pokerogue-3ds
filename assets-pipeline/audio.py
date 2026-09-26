"""Audio -> RSND (DSP-ADPCM) entries.

Why DSP-ADPCM: the 3DS DSP decodes it in hardware, so music and sound effects
cost no ARM11 CPU time, which both routes (native battle core on an Old 3DS,
QuickJS on a New 3DS) are short of. It is the codec commercial 3DS games
stream music in. The cost is SD space: 4 bits per sample.

  BGM:  decoded by ffmpeg, resampled to 32728 Hz (the DSP's output rate), stereo,
        streamed in 14336-sample blocks from bgm.pak. Each track carries its
        default loop point from upstream (audio-manager.ts getBgmLoopPoint for
        battle/title tracks, biome-bgm-loop-points.json for biome tracks).
  SFX:  sound effects, cries, move-animation sounds: original rate capped at
        32728 Hz, mono when both channels are identical, one block, loaded whole.
"""
import json
import os
import re
import struct
import subprocess
import tempfile

import numpy as np

BGM_RATE = 32728
BLOCK = 14336


def _ffmpeg_pcm(src, rate, channels):
    cmd = ["ffmpeg", "-v", "error", "-nostdin", "-i", src, "-f", "s16le", "-acodec", "pcm_s16le",
           "-ac", str(channels), "-ar", str(rate), "-"]
    return subprocess.run(cmd, check=True, capture_output=True).stdout


def probe(src):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "stream=sample_rate,channels", "-of", "json", src],
                         check=True, capture_output=True, text=True).stdout
    s = json.loads(out)["streams"][0]
    return int(s["sample_rate"]), int(s["channels"])


def encode(encoder, pcm, channels, rate, loop=-1, block=BLOCK):
    with tempfile.TemporaryDirectory() as td:
        raw, out = os.path.join(td, "in.raw"), os.path.join(td, "out.rsnd")
        open(raw, "wb").write(pcm)
        r = subprocess.run([encoder, raw, out, str(channels), str(rate), str(loop), str(block)],
                           check=True, capture_output=True, text=True)
        return open(out, "rb").read(), float(r.stdout.strip() or 0)


def bgm_loop_points(game_dir, assets_dir):
    """track name -> loop start in seconds."""
    pts = {}
    biome = os.path.join(assets_dir, "biome-bgm-loop-points.json")
    if os.path.exists(biome):
        pts.update(json.load(open(biome)))
    if game_dir:
        src = open(os.path.join(game_dir, "src", "audio", "audio-manager.ts"), encoding="utf-8").read()
        i = src.find("private getBgmLoopPoint")
        if i >= 0:
            body = src[i:src.index("\n  }\n", i)]
            pending = []
            for m in re.finditer(r'case\s+"([^"]+)"\s*:|return\s+([\d.]+)\s*;', body):
                if m.group(1):
                    pending.append(m.group(1))
                else:
                    for k in pending:
                        pts[k] = float(m.group(2))
                    pending = []
    return pts


def convert_bgm(encoder, src, loop_seconds):
    pcm = _ffmpeg_pcm(src, BGM_RATE, 2)
    loop = int(round(loop_seconds * BGM_RATE)) if loop_seconds is not None else -1
    return encode(encoder, pcm, 2, BGM_RATE, loop, BLOCK)


def convert_sfx(encoder, src):
    rate, ch = probe(src)
    rate = min(rate, BGM_RATE)
    ch = min(ch, 2)
    pcm = _ffmpeg_pcm(src, rate, ch)
    if ch == 2:
        a = np.frombuffer(pcm, "<i2").reshape(-1, 2)
        if np.abs(a[:, 0].astype(np.int32) - a[:, 1]).max(initial=0) <= 2:
            pcm = a[:, 0].copy().tobytes()
            ch = 1
    return encode(encoder, pcm, ch, rate, -1, 0)


# ---------------------------------------------------------------- reference decoder (tests)


def decode_rsnd(b):
    """Decodes an RSND blob to (rate, int16 array [samples, channels]); mirrors the DSP."""
    magic, ch, enc, _, rate, n, loop, block, block_bytes, data_off, nblocks = struct.unpack_from("<4sBBHIIIIIII", b, 0)
    assert magic == b"RSN1" and enc == 2
    coefs = [struct.unpack_from("<16h", b, 36 + 36 * c) for c in range(ch)]
    out = np.zeros((n, ch), np.int16)
    fpb = block // 14
    nframes = (n + 13) // 14
    off = data_off
    hist = [[0, 0] for _ in range(ch)]
    for blk in range(nblocks):
        f0 = blk * fpb
        cnt = min(fpb, nframes - f0)
        for c in range(ch):
            h1, h2 = hist[c]
            for f in range(cnt):
                fr = b[off + f * 8: off + f * 8 + 8]
                ci, sc = fr[0] >> 4, fr[0] & 15
                c1, c2 = coefs[c][ci * 2], coefs[c][ci * 2 + 1]
                for i in range(14):
                    s = f0 * 14 + f * 14 + i
                    if s >= n:
                        break
                    nib = (fr[1 + i // 2] >> (0 if i & 1 else 4)) & 15
                    if nib >= 8:
                        nib -= 16
                    v = (((nib << sc) << 11) + 1024 + c1 * h1 + c2 * h2) >> 11
                    v = max(-32768, min(32767, v))
                    out[s, c] = v
                    h2, h1 = h1, v
            hist[c] = [h1, h2]
            off += cnt * 8
    return rate, out
