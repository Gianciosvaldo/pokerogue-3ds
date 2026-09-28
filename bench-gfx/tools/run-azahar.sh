#!/bin/sh
# Runs the bench headless in Azahar (Old 3DS mode) and copies the result file here.
# Usage: tools/run-azahar.sh LR_RUN CORE.so SD_CARD_DIR [out-dir]
#   LR_RUN   lr-run built from assets-pipeline/app-src/tools/lr-run.c
#   CORE.so  the Azahar libretro core (see tools/azahar-libretro/README.md)
#   SD_CARD_DIR  the build_sd.py output (holding 3ds/pokerogue/*.pak), or an empty dir for no packs
# The emulated SD card goes to a temporary DATA dir; the packs are symlinked, not copied.
set -e
LR=${1:?lr-run}; CORE=${2:?core}; SD=${3:?sd card dir}; OUT=${4:-.}
HERE=$(cd "$(dirname "$0")/.." && pwd)
DATA=$(mktemp -d)
mkdir -p "$DATA/Azahar/sdmc/3ds/pokerogue"
for p in gfx snd bgm font; do
  [ -f "$SD/3ds/pokerogue/$p.pak" ] && ln -s "$(cd "$SD/3ds/pokerogue" && pwd)/$p.pak" "$DATA/Azahar/sdmc/3ds/pokerogue/$p.pak"
done
touch "$DATA/Azahar/sdmc/3ds/pokerogue-gfx-bench-headless"
# Azahar's HLE DSP does not run the firmware, but libctru's ndspInit wants the file to exist.
printf 'placeholder, not a DSP firmware' > "$DATA/Azahar/sdmc/3ds/dspfirm.cdc"
"$LR" "$CORE" "$HERE/pokerogue-gfx-bench.3dsx" "$DATA" frames=60000 \
  until="$DATA/Azahar/sdmc/3ds/pokerogue-gfx-bench-done" shotlast="$OUT/gfx-bench-last.png"
cp "$DATA/Azahar/sdmc/3ds/pokerogue-gfx-bench-result.txt" "$DATA/Azahar/sdmc/3ds/pokerogue-gfx-bench-log.txt" "$OUT/"
rm -rf "$DATA"
echo "results in $OUT/pokerogue-gfx-bench-result.txt"
