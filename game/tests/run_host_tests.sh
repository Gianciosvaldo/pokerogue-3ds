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
TESTS="charge_visibility_test battle_feedback_test persistent_item_bar_test item_description_test battle_stat_panels_test egg_hatch_ui_test trainer_render_test party_item_test stat_banner_test form_anim_test transform_visual_test map_ui_test splicer_ui_test shop_ui_test summary_ui_test saves_test
       starter_data_test starter_ui_test observer_test postmortem_test result_gains_test win_gains_test char_sprite_test msgbox_test store_test custom_run_test profile_tx_test gacha_host_test gacha_screen_test flowtest radar_test achv_screen_test achv_hooks_test capsule_test run_menu_test name_entry_test slot_screen_test run_history_test history_screen_test bgmtest"
make -j"$JOBS" $TESTS runtest || { echo "game/tests: build failed" >&2; exit 1; }
failed=""
for t in $TESTS; do
    echo "== $t"
    if [ "$t" = battle_stat_panels_test ]; then
        "build/$t" fixtures/battle-stat-panels.native.txt || failed="$failed $t"
    else
        "build/$t" || failed="$failed $t"
    fi
done
# The rival now fights at wave 8 and later, so the random player dies sooner and these three need more runs to reach the same
# full-party catch and release-list coverage (floors unchanged).
echo "== runtest (IV Scanner encounter prompt and assessment) 150"
RUNTEST_SCANNER=1 build/runtest 150 0 || failed="$failed runtest-scanner"
echo "== runtest (Check Team form toggle and reward return) 350"
RUNTEST_FORM=1 build/runtest 350 0 || failed="$failed runtest-form"
echo "== runtest (Check Team form change, animation and reward return) 350"
RUNTEST_FORM=active build/runtest 350 0 || failed="$failed runtest-form-active"
echo "== runtest 10000"
build/runtest 10000 || failed="$failed runtest"
# Keep the original first 100 cases and extend their deterministic sequence:
# the smaller corpus can make every full-party choice Release, missing Cancel.
echo "== runtest (system data, saves) 300"
RUNTEST_SYS=1 RUNTEST_SAVE=1 build/runtest 300 0 || failed="$failed runtest-sys-save"
echo "== runtest (Save and Quit from the in-run menu, continued from the slot) 300"
RUNTEST_SYS=1 RUNTEST_SAVE=quit build/runtest 300 0 || failed="$failed runtest-quit"
echo "== runtest (custom runs: the rematch offers, wins and vouchers through run_screen) 200"
RUNTEST_CUSTOM=strong build/runtest 200 0 || failed="$failed runtest-custom"
echo "== runtest (custom runs dropped and continued from the custom slot) 100"
RUNTEST_CUSTOM=strong RUNTEST_SAVE=resume build/runtest 100 0 || failed="$failed runtest-custom-resume"
echo "== runtest (custom runs left through Save and Quit and continued from the custom slot) 100"
RUNTEST_CUSTOM=strong RUNTEST_SAVE=quit build/runtest 100 0 || failed="$failed runtest-custom-quit"
if [ -n "$failed" ]; then echo "game/tests FAILED:$failed" >&2; exit 1; fi
echo "game/tests: all passed"
