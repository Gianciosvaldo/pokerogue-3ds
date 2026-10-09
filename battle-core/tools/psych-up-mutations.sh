#!/bin/sh
# Mutation check: each mutant is the real CopyStatsAttr leaf in src/move_effects.c, changed in a
# scratch copy of battle-core, rebuilt, and replayed by psych-up-test on an unchanged upstream
# trace; every mutant must be caught (exit 1, mismatches reported), the intact build must pass.
# usage: tools/psych-up-mutations.sh <upstream checkout> <complete psych-up trace>
set -eu
UP=${1:?upstream checkout}; TRACE=${2:?complete trace}
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
T=$(mktemp -d "${TMPDIR:-/tmp}/psych-up-mutations.XXXXXX")
trap 'rm -rf "$T"' EXIT
export TURNTEST_LOCALES="$UP/locales/en"
fail=0
run() { # name python-replacement-old python-replacement-new
    rm -rf "$T/core"; mkdir -p "$T/core"
    (cd "$HERE" && tar cf - --exclude=./build . ) | (cd "$T/core" && tar xf -)
    if [ -n "$2" ]; then
        OLD="$2" NEW="$3" python3 - "$T/core/src/move_effects.c" <<'PY'
import os, sys
p = sys.argv[1]; s = open(p).read(); old = os.environ["OLD"]; new = os.environ["NEW"]
if s.count(old) != 1: sys.exit("mutation site not found exactly once")
open(p, "w").write(s.replace(old, new))
PY
    fi
    make -s -C "$T/core" -j2 BUILD="$T/core/build" "$T/core/build/psych-up-test" > "$T/make.log" 2>&1 || { echo "$1: build failed" >&2; exit 2; }
    if "$T/core/build/psych-up-test" "$TRACE" > "$T/$1.log" 2>&1; then r=0; else r=$?; fi
    summary=$(grep -E '^(covered|texts of covered|crit probes)' "$T/$1.log" | tr '\n' ';')
    echo "$1: exit $r; $summary"
}
run intact "" ""
[ "$r" = 0 ] || { echo "intact build does not pass" >&2; exit 1; }
run omit-accuracy-evasion "for (int s = 0; s < 7; s++) {" "for (int s = 0; s < 5; s++) {"
[ "$r" = 1 ] || fail=1
run stale-critical-tag "        else
            pr_remove_tag(bt, e, user, BATTLER_TAG_TYPE_CRIT_BOOST);
" ""
[ "$r" = 1 ] || fail=1
exit $fail
