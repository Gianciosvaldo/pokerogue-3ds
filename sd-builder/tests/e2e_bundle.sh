#!/bin/sh
# The one-file part of the end-to-end test: from an unzipped builder whose packs are already built (e2e.sh runs
# this after its own builds), make PokeRogue.3dsx and PokeRogue.cia on fake SD cards, answer the format menu, and
# check what lands on the cards against the packs.
#
#   sd-builder/tests/e2e_bundle.sh BUILDER_DIR MIRROR_DIR [extra builder.py arguments, e.g. --existing-packs]
#
# MIRROR_DIR needs makerom-v0.18.4-ubuntu_x86_64.zip and DSP1.3dsx (the names the builder saves them as).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
B=$(realpath "$1"); MIRROR=$(realpath "$2"); shift 2
T=$(mktemp -d /tmp/sdb-e2e-bundle.XXXX)
PACKS="$B/files/build/3ds/pokerogue"
fail() { echo "FAIL: $*"; exit 1; }
run() { python3 "$B/files/builder.py" --mirror "$MIRROR" "$@"; }

[ -e "$B/files/game/pokerogue.3dsx" ] || fail "this builder has no game app in it"

echo "== --format both on a fresh card"
SD="$T/sd1"; mkdir -p "$SD/3ds"
run --sd "$SD" --yes --format both "$@" > "$T/both.txt" 2>&1 || { cat "$T/both.txt"; fail "--format both"; }
python3 -I "$HERE/check_bundle.py" "$PACKS" --3dsx "$SD/3ds/PokeRogue.3dsx" --cia "$SD/cias/PokeRogue.cia" > "$T/check.txt" || { grep -v '^ok ' "$T/check.txt"; fail "the files don't match the packs"; }
tail -1 "$T/check.txt"
[ -z "$(find "$SD" -name '*.pak')" ] || fail "loose packs on the card of a one-file build"

echo "== again with nothing changed (should reuse both files)"
run --sd "$SD" --yes --format both "$@" > "$T/again.txt" 2>&1 || { cat "$T/again.txt"; fail "second --format both"; }
[ "$(grep -c 'already made' "$T/again.txt")" = 2 ] || { cat "$T/again.txt"; fail "rebuilt a file whose inputs hadn't changed"; }
grep -q 'already on the card' "$T/again.txt" || fail "copied files that were already on the card"

echo "== the menu: 2 = the installable file only"
SD="$T/sd2"; mkdir -p "$SD/3ds"
printf '2\n' | run --sd "$SD" "$@" > "$T/menu2.txt" 2>&1 || { cat "$T/menu2.txt"; fail "menu answer 2"; }
[ -s "$SD/cias/PokeRogue.cia" ] || fail "menu 2 made no .cia"
[ ! -e "$SD/3ds/PokeRogue.3dsx" ] || fail "menu 2 also put a .3dsx on the card"
echo "== the menu: Enter = the .3dsx"
SD="$T/sd3"; mkdir -p "$SD/3ds"
printf '\n' | run --sd "$SD" "$@" > "$T/menu1.txt" 2>&1 || { cat "$T/menu1.txt"; fail "menu answer Enter"; }
[ -s "$SD/3ds/PokeRogue.3dsx" ] || fail "menu Enter made no .3dsx"
[ ! -e "$SD/cias" ] || fail "menu Enter also made a .cia"
echo "== the menu: a wrong answer asks again, then 3 = loose files"
SD="$T/sd4"; mkdir -p "$SD/3ds"
printf 'x\n3\n' | run --sd "$SD" "$@" > "$T/menu3.txt" 2>&1 || { cat "$T/menu3.txt"; fail "menu answers x, 3"; }
grep -q 'Please type 1, 2 or 3' "$T/menu3.txt" || fail "no second ask after a wrong answer"
for s in data font gfx snd bgm; do cmp -s "$SD/3ds/pokerogue/$s.pak" "$PACKS/$s.pak" || fail "menu 3: $s.pak"; done
cmp -s "$SD/3ds/PokeRogue.3dsx" "$B/files/game/pokerogue.3dsx" || fail "menu 3: the plain app isn't on the card"

echo "== the CIA again with ctrtool, if one is given"
if [ -n "$CTRTOOL" ]; then sh "$HERE/ctrtool_check.sh" "$CTRTOOL" "$T/sd1/cias/PokeRogue.cia" "$PACKS"; fi
rm -rf "$T"
echo "E2E BUNDLE OK"
