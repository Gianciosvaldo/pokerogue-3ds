#!/bin/sh
# Assembles PokeRogue-SD-Builder.zip: the builder, the pipeline scripts and the Windows audio encoder.
# It holds no Pokemon or PokeRogue content; the builder downloads that on the user's PC.
#
#   [GAME_DIR=DIR] sd-builder/make-zip.sh [OUT_DIR] [APP.3dsx ...]
#
# Needs x86_64-w64-mingw32-gcc (apt install mingw-w64), zip and python3. Any .3dsx given is copied to the
# card's 3ds folder by the builder (for example the current bench or game build).
#
# GAME_DIR is a game/ build folder holding pokerogue.3dsx, pokerogue.elf and pokerogue.smdh. With it the zip can make
# the one-file PokeRogue.3dsx and PokeRogue.cia; without it the builder only makes the loose files. The HOME menu
# banner is drawn here (tools/make_banner_art.py) and packed with bannertool: BANNERTOOL=PATH uses a copy you have,
# otherwise the pinned Linux release below is downloaded.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(dirname "$HERE")
OUT=${1:-$HERE/dist}
[ $# -gt 0 ] && shift
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
D="$STAGE/PokeRogue-SD-Builder"
mkdir -p "$D/files/pipeline/tools" "$D/files/apps" "$OUT"
OUT=$(cd "$OUT" && pwd)  # zip runs from the staging folder

cp "$HERE/windows/BUILD-SD-CARD.bat" "$HERE/READ-ME-FIRST.txt" "$D/"
cp "$HERE/builder.py" "$HERE/bundle.py" "$HERE/pins.json" "$HERE/windows/setup.ps1" "$D/files/"
for f in build_sd.py gfx.py audio.py font.py rpak.py; do cp "$REPO/assets-pipeline/$f" "$D/files/pipeline/"; done
cp "$REPO/assets-pipeline/tools/dspadpcm.c" "$D/files/pipeline/tools/"
x86_64-w64-mingw32-gcc -O2 -static -s -Wl,--no-insert-timestamp -o "$D/files/pipeline/tools/dspadpcm.exe" \
  "$REPO/assets-pipeline/tools/dspadpcm.c" -lm
for app in "$@"; do cp "$app" "$D/files/apps/"; done
# The project's own pictures (the JoJo trainers' sprites): the builder packs them into gfx.pak (see README.md)
cp -R "$HERE/extra-images" "$D/files/extra-images"

BANNERTOOL_URL=https://github.com/carstene1ns/3ds-bannertool/releases/download/1.2.3/bannertool-1.2.3-linux.tar.gz
BANNERTOOL_SHA256=748519d200519db18e9fd00c332ac32f5411e41230258291e89953a76c1f7155
if [ -n "$GAME_DIR" ]; then
  for f in pokerogue.3dsx pokerogue.elf pokerogue.smdh; do
    [ -f "$GAME_DIR/$f" ] || { echo "GAME_DIR has no $f" >&2; exit 1; }
  done
  mkdir -p "$D/files/game"
  cp "$GAME_DIR/pokerogue.3dsx" "$GAME_DIR/pokerogue.elf" "$GAME_DIR/pokerogue.smdh" "$D/files/game/"
  cp "$HERE/cia/pokerogue.rsf" "$D/files/game/"
  BT=${BANNERTOOL:-}
  if [ -z "$BT" ]; then
    mkdir -p "$STAGE/bt"
    curl -sSL --retry 4 --retry-all-errors -o "$STAGE/bt/bannertool.tar.gz" "$BANNERTOOL_URL"
    echo "$BANNERTOOL_SHA256  $STAGE/bt/bannertool.tar.gz" | sha256sum -c - >&2
    tar -xzf "$STAGE/bt/bannertool.tar.gz" -C "$STAGE/bt"
    BT=$(find "$STAGE/bt" -type f -name bannertool | head -n 1)
    chmod +x "$BT"
  fi
  mkdir -p "$STAGE/art"
  python3 "$HERE/tools/make_banner_art.py" "$STAGE/art"
  "$BT" makebanner -i "$STAGE/art/banner.png" -a "$STAGE/art/silence.wav" -o "$D/files/game/banner.bnr" >&2
fi
REV=$(git -C "$REPO" rev-parse --short HEAD)
printf 'Built from pokerogue-3ds %s (%s).\n' "$REV" "$(git -C "$REPO" log -1 --format=%cd --date=short)" > "$D/files/VERSION.txt"

# Windows line endings for what Windows tools read (cmd needs them for labels in .bat files)
for f in "$D/BUILD-SD-CARD.bat" "$D/READ-ME-FIRST.txt" "$D/files/setup.ps1" "$D/files/VERSION.txt"; do
  python3 -c 'import sys; p=sys.argv[1]; b=open(p,"rb").read().replace(b"\r\n",b"\n").replace(b"\n",b"\r\n"); open(p,"wb").write(b)' "$f"
done

# Nothing from upstream may ever be in the zip
if find "$D" -type f -not -path "$D/files/extra-images/*" \( -name '*.png' -o -name '*.pak' -o -name '*.ogg' -o -name '*.mp3' -o -name '*.m4a' \
     -o -name '*.wav' -o -name '*.ttf' -o -name '*.json' ! -name 'pins.json' \) | grep -q .; then
  echo "refusing: upstream content in the zip" >&2; exit 1
fi

rm -f "$OUT/PokeRogue-SD-Builder.zip"
(cd "$STAGE" && TZ=UTC find PokeRogue-SD-Builder -exec touch -d '2026-01-01 00:00:00' {} + &&
  find PokeRogue-SD-Builder | LC_ALL=C sort | zip -q -X -D "$OUT/PokeRogue-SD-Builder.zip" -@)
ls -l "$OUT/PokeRogue-SD-Builder.zip"
unzip -l "$OUT/PokeRogue-SD-Builder.zip"
