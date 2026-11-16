#!/usr/bin/env python3
"""build_sd.py --extra-audio: tracks from a folder laid out like assets/audio/bgm go into bgm.pak (custom/jotaro.mp3 is the key
audio/bgm/custom/jotaro), replace an upstream track of the same name, and take a loop point from a "<name>.loop" file. Uses short
made-up tones, no upstream files. Needs ffmpeg and a C compiler (for tools/dspadpcm.c).

    python3 tests/extra_audio_test.py
"""
import os
import struct
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, ".."))
import audio  # noqa: E402
import build_sd  # noqa: E402
import rpak  # noqa: E402


def tone(path, hz, seconds):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", f"sine=frequency={hz}:duration={seconds}", "-ac", "2", path],
                   check=True)


def loop_of(blob):
    return struct.unpack_from("<4sBBHIIIIIII", blob, 0)[6]  # loop start in samples, 0xFFFFFFFF = none


def main():
    with tempfile.TemporaryDirectory() as t:
        assets, extra, out = (os.path.join(t, d) for d in ("assets", "extra", "out"))
        tone(os.path.join(assets, "audio", "bgm", "battle_wild.wav"), 440, 1)
        tone(os.path.join(assets, "audio", "bgm", "title.wav"), 330, 1)
        os.makedirs(out)
        enc = build_sd.find_encoder(None)
        report = {}
        build_sd.build_bgm(assets, None, out, 1, enc, report)
        assert report["bgm"]["entries"] == 2 and not report["bgm"]["errors"] and "extra" not in report["bgm"], report["bgm"]
        r = rpak.Reader(os.path.join(out, "bgm.pak"))
        assert sorted(r.keys()) == ["audio/bgm/battle_wild", "audio/bgm/title"], sorted(r.keys())
        upstream_title = r.read("audio/bgm/title")

        tone(os.path.join(extra, "custom", "jotaro.wav"), 550, 2)
        tone(os.path.join(extra, "custom", "pillar_men.wav"), 660, 2)
        open(os.path.join(extra, "custom", "pillar_men.loop"), "w").write("0.5\n")
        tone(os.path.join(extra, "title.wav"), 220, 1)  # replaces upstream's
        open(os.path.join(extra, "notes.txt"), "w").write("not a track")
        report = {}
        build_sd.build_bgm(assets, None, out, 1, enc, report, extra)
        st = report["bgm"]
        assert st["entries"] == 4 and not st["errors"] and st["extra"] == 2, st
        assert st["with_loop"] == 1, st

        own = {k: loop for _, k, loop in build_sd.extra_audio(extra)}
        assert own == {"custom/jotaro": None, "custom/pillar_men": 0.5, "title": None}, own

        # the pack: each key once, the extra title replaced upstream's, the loop point is in the track's header, and the tracks decode
        r = rpak.Reader(os.path.join(out, "bgm.pak"))
        assert sorted(r.keys()) == ["audio/bgm/battle_wild", "audio/bgm/custom/jotaro", "audio/bgm/custom/pillar_men", "audio/bgm/title"], \
            sorted(r.keys())
        assert r.read("audio/bgm/title") != upstream_title
        assert loop_of(r.read("audio/bgm/custom/pillar_men")) == (round(0.5 * audio.BGM_RATE) + 7) // 14 * 14  # the encoder starts loops on a frame
        assert loop_of(r.read("audio/bgm/custom/jotaro")) == 0xFFFFFFFF
        rate, pcm = audio.decode_rsnd(r.read("audio/bgm/custom/jotaro"))
        assert rate == audio.BGM_RATE and abs(len(pcm) - 2 * rate) <= 14 and pcm.shape[1] == 2, (rate, pcm.shape)

        # the se/ folder: sounds for snd.pak (audio/se/<name>), never music
        tone(os.path.join(assets, "audio", "se", "hit.wav"), 880, 0.2)
        tone(os.path.join(extra, "se", "the_world.wav"), 990, 0.5)
        report = {}
        build_sd.build_bgm(assets, None, out, 1, enc, report, extra)
        assert report["bgm"]["entries"] == 4 and report["bgm"]["extra"] == 2, report["bgm"]
        assert "audio/bgm/se/the_world" not in rpak.Reader(os.path.join(out, "bgm.pak")).keys()
        report = {}
        build_sd.build_snd(assets, out, 1, enc, report)
        assert report["snd"]["entries"] == 1 and "extra" not in report["snd"], report["snd"]
        report = {}
        build_sd.build_snd(assets, out, 1, enc, report, extra)
        assert report["snd"]["entries"] == 2 and report["snd"]["extra"] == 1 and not report["snd"]["errors"], report["snd"]
        assert sorted(rpak.Reader(os.path.join(out, "snd.pak")).keys()) == ["audio/se/hit", "audio/se/the_world"]
    print("extra_audio_test: OK")


if __name__ == "__main__":
    main()
