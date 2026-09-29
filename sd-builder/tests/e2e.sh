#!/bin/sh
# End-to-end test of the packaged builder: unzip it, run it against a fake SD card with every download
# served from a local mirror, then check the card, a second run (everything skipped) and a
# build-only run (SD-CARD-FILES).
#
#   sd-builder/tests/e2e.sh ZIP MIRROR_DIR [REFERENCE_PACK_DIR] [--wine]
#
# MIRROR_DIR holds each download under the name the builder saves it as: the files named in pins.json,
# plus assets-87426a7.zip and locales-270ed0a.zip (git archive --prefix=REPO-COMMIT/ of the pinned commits,
# which is what codeload serves). REFERENCE_PACK_DIR: packs from build_sd.py to compare byte for byte
# (all five must match; under Wine, data, font and gfx must, and the audio packs are only compared).
# --wine runs the Windows Python, ffmpeg and encoder under Wine (needs wine64; the Python comes from
# the mirror, as setup.ps1 would fetch it on Windows).
set -e
ZIP=$(realpath "$1"); MIRROR=$(realpath "$2"); REF=${3:+$(realpath "$3")}
WINE=; [ "$4" = "--wine" ] && WINE=1
T=$(mktemp -d /tmp/sdb-e2e.XXXX)
echo "test dir $T"
cd "$T" && unzip -q "$ZIP"
B="$T/PokeRogue-SD-Builder"
SD="$T/fake-sd"; mkdir -p "$SD/3ds" "$SD/luma"
fail() { echo "FAIL: $*"; exit 1; }

run() {
  if [ -n "$WINE" ]; then
    # Wine's console layer needs stdout to be a pipe, not a file
    { WINEDEBUG=-all wine "$B/files/runtime/python/python.exe" -X utf8 "$B/files/builder.py" "$@" < /dev/null
      echo $? > "$T/rc"; } 2>&1 | cat
    return "$(cat "$T/rc")"
  else
    python3 "$B/files/builder.py" "$@"
  fi
}

if [ -n "$WINE" ]; then  # what setup.ps1 does on Windows
  mkdir -p "$B/files/runtime" && tar -xzf "$MIRROR"/cpython-*-windows-msvc-install_only.tar.gz -C "$B/files/runtime"
  SDARG="Z:$(echo "$SD" | tr / '\\')"; MARG="Z:$(echo "$MIRROR" | tr / '\\')"
else
  SDARG="$SD"; MARG="$MIRROR"
fi

echo "== first run"
run --sd "$SDARG" --yes --mirror "$MARG" || fail "first run exited non-zero"
for s in data font gfx snd bgm; do
  [ -s "$SD/3ds/pokerogue/$s.pak" ] || fail "$s.pak missing on the card"
  if [ -n "$REF" ]; then
    if cmp -s "$SD/3ds/pokerogue/$s.pak" "$REF/$s.pak"; then echo "$s.pak identical to the reference"
    else echo "$s.pak DIFFERS from the reference ($(stat -c %s "$SD/3ds/pokerogue/$s.pak") vs $(stat -c %s "$REF/$s.pak") bytes)"
      # same machine, same ffmpeg: every pack must match; under Wine only the audio may (other ffmpeg build)
      [ -z "$WINE" ] || [ "$s" = snd ] || [ "$s" = bgm ] || fail "$s.pak differs"
      [ -n "$WINE" ] || fail "$s.pak differs"; fi
  fi
done
[ -s "$SD/3ds/DSP1.3dsx" ] || fail "DSP1.3dsx not put on a card without dspfirm.cdc"
for app in "$B"/files/apps/*.3dsx; do [ -e "$app" ] || continue
  cmp -s "$app" "$SD/3ds/$(basename "$app")" || fail "app $(basename "$app") not copied"; done
ls "$SD/3ds/pokerogue" | grep -v '\.pak$' | grep . && fail "stray files on the card"
grep -q "=== done" "$B/build-log.txt" || fail "log has no done line"

echo "== second run (should skip everything; the card now has dspfirm.cdc)"
echo fake > "$SD/3ds/dspfirm.cdc"
rm "$SD/3ds/DSP1.3dsx"
run --sd "$SDARG" --yes --mirror "$MARG" > "$T/second.txt" 2>&1 || { cat "$T/second.txt"; fail "second run"; }
cat "$T/second.txt"
[ "$(grep -c 'already done' "$T/second.txt")" = 5 ] || fail "second run rebuilt something"
[ "$(grep -c 'pak: already on the card' "$T/second.txt")" = 5 ] || fail "second run copied again"
grep -q "Sound: ready" "$T/second.txt" || fail "dspfirm.cdc not seen"
[ -e "$SD/3ds/DSP1.3dsx" ] && fail "DSP1 put on a card that has dspfirm.cdc"

echo "== build-only run"
run --no-sd --yes --mirror "$MARG" > "$T/third.txt" 2>&1 || { cat "$T/third.txt"; fail "build-only run"; }
for s in data font gfx snd bgm; do cmp -s "$B/SD-CARD-FILES/3ds/pokerogue/$s.pak" "$SD/3ds/pokerogue/$s.pak" \
  || fail "SD-CARD-FILES $s.pak"; done
[ -s "$B/SD-CARD-FILES/3ds/DSP1.3dsx" ] || fail "no DSP1 in SD-CARD-FILES"
ls -R "$B/SD-CARD-FILES"

echo "== interrupted stage is redone"
rm "$B/files/build/3ds/pokerogue/.data.done"
run --sd "$SDARG" --yes --mirror "$MARG" > "$T/fourth.txt" 2>&1 || { cat "$T/fourth.txt"; fail "fourth run"; }
grep -q "Converting the game's text" "$T/fourth.txt" || fail "data stage not redone"
[ "$(grep -c 'already done' "$T/fourth.txt")" = 4 ] || fail "other stages redone"

echo "ALL OK ($T)"
