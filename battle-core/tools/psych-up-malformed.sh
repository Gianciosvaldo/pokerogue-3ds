#!/bin/sh
# Strict-guard check for psych-up-test: every malformed variant of a complete trace must be
# refused with exit 2 (a parse refusal, not a comparison result); the intact trace must pass.
# usage: tools/psych-up-malformed.sh <upstream checkout> <complete psych-up trace>
set -eu
UP=${1:?upstream checkout}; GOOD=${2:?complete trace}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
BUILD=${BUILD:-"$HERE/build"}
make -s -C "$HERE" -j2 BUILD="$BUILD" "$BUILD/psych-up-test"
T=$(mktemp -d "${TMPDIR:-/tmp}/psych-up-malformed.XXXXXX")
trap 'rm -rf "$T"' EXIT
export TURNTEST_LOCALES="$UP/locales/en"
N=$(sed -n '1s/^# PSYCH_UP_CONTRACT 1 \([0-9]*\)$/\1/p' "$GOOD")
[ -n "$N" ] || { echo "not a psych-up trace" >&2; exit 2; }
if "$BUILD/psych-up-test" "$GOOD" > "$T/good.log" 2>&1; then echo "intact: exit 0"; else echo "intact trace failed" >&2; exit 1; fi
fail=0
check() { # name file
    if "$BUILD/psych-up-test" "$2" > "$T/out.log" 2>&1; then r=0; else r=$?; fi
    if [ "$r" = 2 ]; then echo "refused (2): $1"; else echo "NOT REFUSED ($r): $1"; fail=1; fi
}
last=$(awk '/^CASE /{n=NR} END{print n}' "$GOOD")
sed 1d "$GOOD" > "$T/a"; check "missing header" "$T/a"
sed '1s/.*/# PSYCH_UP_CONTRACT 1 '$((N + 1))'/' "$GOOD" > "$T/a"; check "header count too high" "$T/a"
sed '1s/.*/# PSYCH_UP_CONTRACT 1 '$((N - 1))'/' "$GOOD" > "$T/a"; check "header count too low" "$T/a"
sed '1s/.*/# PSYCH_UP_CONTRACT 1 0/' "$GOOD" > "$T/a"; check "zero cases" "$T/a"
sed '1s/.*/# PSYCH_UP_CONTRACT 1 '"$N"'.5/' "$GOOD" > "$T/a"; check "fractional count" "$T/a"
sed '1s/.*/# GROWTH_CONTRACT 1 '"$N"'/' "$GOOD" > "$T/a"; check "other contract header" "$T/a"
sed '$d' "$GOOD" > "$T/a"; check "missing footer" "$T/a"
sed '$s/.*/# PSYCH_UP_COMPLETE '$((N - 1))'/' "$GOOD" > "$T/a"; check "footer count mismatch" "$T/a"
{ cat "$GOOD"; echo "CASE $N"; } > "$T/a"; check "record after footer" "$T/a"
{ cat "$GOOD"; echo "# PSYCH_UP_COMPLETE $N"; } > "$T/a"; check "second footer" "$T/a"
head -n $((last - 1)) "$GOOD" > "$T/a"; echo "# PSYCH_UP_COMPLETE $N" >> "$T/a"; check "last case dropped" "$T/a"
awk -v L="$last" 'NR<L+3{print} END{print "# PSYCH_UP_COMPLETE '"$N"'"}' "$GOOD" > "$T/a"; check "truncated last case" "$T/a"
sed '0,/^CASE 1$/s//CASE 0/' "$GOOD" > "$T/a"; check "duplicate case id" "$T/a"
sed '0,/^CASE 1$/s//CASE 2/' "$GOOD" > "$T/a"; check "out-of-order case id" "$T/a"
sed '0,/^CASE 1$/s//CASE 1x/' "$GOOD" > "$T/a"; check "malformed case id" "$T/a"
sed '0,/^RUN$/{/^RUN$/d}' "$GOOD" > "$T/a"; check "missing RUN" "$T/a"
awk '!d && /^RUN$/{print; print; d=1; next} {print}' "$GOOD" > "$T/a"; check "second RUN" "$T/a"
awk '!d && /^CRITPROBE /{d=1; next} {print}' "$GOOD" > "$T/a"; check "missing CRITPROBE" "$T/a"
awk '!d && /^CRITPROBE /{print; print; d=1; next} {print}' "$GOOD" > "$T/a"; check "second CRITPROBE" "$T/a"
awk '!d && /^CRITPROBE /{sub(/move=[0-9]+/, "move=99999"); d=1} {print}' "$GOOD" > "$T/a"; check "CRITPROBE move out of range" "$T/a"
awk '!d && /^CRITPROBE /{sub(/def=[0-9]+/, "def=" substr($2, 5)); d=1} {print}' "$GOOD" > "$T/a"; check "CRITPROBE attacker as defender" "$T/a"
awk '!d && /^CMD / && / move=244 /{sub(/ move=244 /, " move=33 "); d=1} {print}' "$GOOD" > "$T/a"; check "no Psych Up command" "$T/a"
awk '!d && /^CASE /{print; print "# stray"; d=1; next} {print}' "$GOOD" > "$T/a"; check "stray comment record" "$T/a"
awk '!d && /^MON /{print; print "MOD side=0 cls=FocusBandModifier pid=1 stack=1"; d=1; next} {print}' "$GOOD" > "$T/a"; check "modifier record" "$T/a"
awk '!d && /^BTAG /{sub(/type=[A-Z_]+/, "type=NOT_A_TAG"); d=1} {print}' "$GOOD" > "$T/a"; check "unknown battler tag" "$T/a"
awk '!d && /^END /{next_end=1} !d && next_end && /^CASE /{print "END stop=TurnInitPhase"; d=1} {print}' "$GOOD" > "$T/a"; check "END outside a case" "$T/a"
exit $fail
