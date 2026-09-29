#!/bin/sh
# Assembles PokeRogue-SD-Builder.zip: the builder, the pipeline scripts and the Windows audio encoder.
# It holds no Pokemon or PokeRogue content; the builder downloads that on the user's PC.
#
#   sd-builder/make-zip.sh [OUT_DIR] [APP.3dsx ...]
#
# Needs x86_64-w64-mingw32-gcc (apt install mingw-w64), zip and python3. Any .3dsx given is copied to the
# card's 3ds folder by the builder (for example the current bench or game build).
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(dirname "$HERE")
OUT=${1:-$HERE/dist}
[ $# -gt 0 ] && shift
STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
D="$STAGE/PokeRogue-SD-Builder"
mkdir -p "$D/files/pipeline/tools" "$D/files/apps" "$OUT"

cp "$HERE/windows/BUILD-SD-CARD.bat" "$HERE/READ-ME-FIRST.txt" "$D/"
cp "$HERE/builder.py" "$HERE/pins.json" "$HERE/windows/setup.ps1" "$D/files/"
for f in build_sd.py gfx.py audio.py font.py rpak.py; do cp "$REPO/assets-pipeline/$f" "$D/files/pipeline/"; done
cp "$REPO/assets-pipeline/tools/dspadpcm.c" "$D/files/pipeline/tools/"
x86_64-w64-mingw32-gcc -O2 -static -s -o "$D/files/pipeline/tools/dspadpcm.exe" \
  "$REPO/assets-pipeline/tools/dspadpcm.c" -lm
for app in "$@"; do cp "$app" "$D/files/apps/"; done
REV=$(git -C "$REPO" rev-parse --short HEAD)
printf 'Built from pokerogue-3ds %s on %s.\n' "$REV" "$(date -u +%Y-%m-%d)" > "$D/files/VERSION.txt"

# Windows line endings for what Windows tools read (cmd needs them for labels in .bat files)
for f in "$D/BUILD-SD-CARD.bat" "$D/READ-ME-FIRST.txt" "$D/files/setup.ps1" "$D/files/VERSION.txt"; do
  python3 -c 'import sys; p=sys.argv[1]; b=open(p,"rb").read().replace(b"\r\n",b"\n").replace(b"\n",b"\r\n"); open(p,"wb").write(b)' "$f"
done

# Nothing from upstream may ever be in the zip
if find "$D" -type f \( -name '*.png' -o -name '*.pak' -o -name '*.ogg' -o -name '*.mp3' -o -name '*.m4a' \
     -o -name '*.wav' -o -name '*.ttf' -o -name '*.json' ! -name 'pins.json' \) | grep -q .; then
  echo "refusing: upstream content in the zip" >&2; exit 1
fi

rm -f "$OUT/PokeRogue-SD-Builder.zip"
(cd "$STAGE" && TZ=UTC find PokeRogue-SD-Builder -exec touch -d '2026-01-01 00:00:00' {} + &&
  find PokeRogue-SD-Builder | LC_ALL=C sort | zip -q -X -D "$OUT/PokeRogue-SD-Builder.zip" -@)
ls -l "$OUT/PokeRogue-SD-Builder.zip"
unzip -l "$OUT/PokeRogue-SD-Builder.zip"
