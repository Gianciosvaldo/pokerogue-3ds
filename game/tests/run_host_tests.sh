#!/bin/sh
# The game/ host tests CI runs (the PC check of game/README.md): builds them in parallel, runs each one, prints
# every failure and exits 1 if any failed. From any folder: game/tests/run_host_tests.sh
#
# Not run here: evtest and switchtest (need upstream's locales/en), startertest (compares against an upstream
# checkout), animtest and anim_details_test (need libjansson and upstream's animation files), mkfixture (a tool, not a test).
# runtest plays 10000 runs (about a minute): below roughly 5000 its evolution coverage floor (5 evolved, 5 cancelled, 3 moves)
# is not met by chance and is reported as not checked; the floor is never lowered. tests/observer_mutations.py (46 mutations,
# about 6 minutes) is run by hand.
set -u
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
cd "$HERE"
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 2)}
TESTS="stat_banner_test form_anim_test map_ui_test splicer_ui_test shop_ui_test summary_ui_test saves_test
       starter_data_test starter_ui_test observer_test postmortem_test store_test profile_tx_test flowtest radar_test capsule_test bgmtest"
make -j"$JOBS" $TESTS runtest || { echo "game/tests: build failed" >&2; exit 1; }
failed=""
for t in $TESTS; do
    echo "== $t"
    "build/$t" || failed="$failed $t"
done
echo "== runtest 10000"
build/runtest 10000 || failed="$failed runtest"
echo "== runtest (system data, saves) 100"
RUNTEST_SYS=1 RUNTEST_SAVE=1 build/runtest 100 0 || failed="$failed runtest-sys-save"
if [ -n "$failed" ]; then echo "game/tests FAILED:$failed" >&2; exit 1; fi
echo "game/tests: all passed"
