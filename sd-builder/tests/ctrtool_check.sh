#!/bin/sh
# Opens a PokeRogue.cia with devkitPro-independent tooling (3DSGuy's ctrtool) and checks it the way a second reader
# would: the NCCH, ExeFS and RomFS hashes, the three RomFS hash-tree levels, the ExHeader's memory mode, and the
# five packs extracted from the RomFS byte for byte.
#
#   sd-builder/tests/ctrtool_check.sh CTRTOOL PokeRogue.cia PACK_DIR
#
# The CIA is signed with makerom's test keys (-target t, which is what a CFW with signature patches installs), so the
# signature checks are expected to fail: the TMD, the ticket, the ExHeader's access descriptor and the NCCH's
# signature are the only "FAIL"/"invalid" lines allowed. Anything else wrong fails the test.
set -e
CT=$(realpath "$1"); CIA=$(realpath "$2"); PACKS=$(realpath "$3")
W=$(mktemp -d "${TMPDIR:-/tmp}/ctrtool-check.XXXX")
trap 'rm -rf "$W"' EXIT
fail() { echo "FAIL: $*"; exit 1; }

"$CT" -q -t cia --contents="$W/c" "$CIA" > "$W/cia.txt" 2>&1 || { cat "$W/cia.txt"; fail "ctrtool can't open the CIA"; }
set -- "$W"/c.*
[ $# = 1 ] || fail "expected one content in the CIA, found $#"
"$CT" -y -t ncch --romfsdir="$W/rd" --exefsdir="$W/ex" "$1" > "$W/ncch.txt" 2>&1 || { cat "$W/ncch.txt"; fail "ctrtool can't open the NCCH"; }
cat "$W/ncch.txt" "$W/cia.txt" | grep -E "ERROR|FAIL|invalid" || true
bad=$(cat "$W/ncch.txt" "$W/cia.txt" | grep -E "ERROR|FAIL" | grep -vE "Signature for (Ticket|TitleMetaData|AccessDescriptor) was invalid|^Signature: \(FAIL\)" || true)
[ -z "$bad" ] || fail "unexpected problems: $bad"
for want in "ExeFS hash: (GOOD)" "RomFS hash: (GOOD)" "Level 0: (GOOD)" "Level 1: (GOOD)" "Level 2: (GOOD)"; do
  grep -qF "$want" "$W/ncch.txt" || fail "ctrtool output has no '$want'"
done
grep -F "System mode:" "$W/ncch.txt" | grep -qF "(GOOD)" || fail "ctrtool doesn't accept the memory mode"
grep -qF "Service access: (GOOD)" "$W/ncch.txt" || fail "ctrtool doesn't accept the service list"
for p in gfx snd bgm font data; do
  [ -f "$W/rd/$p.pak" ] || fail "$p.pak is not in the RomFS"
  cmp -s "$W/rd/$p.pak" "$PACKS/$p.pak" || fail "$p.pak in the RomFS differs from the pack"
  echo "ok   $p.pak extracted from the RomFS is the pack"
done
[ "$(ls "$W/rd" | wc -l)" = 5 ] || fail "the RomFS holds more than the five packs"
echo "ok   ctrtool hashes good, memory mode and service list accepted, five packs identical"
