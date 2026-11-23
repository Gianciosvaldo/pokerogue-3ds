#!/bin/sh
# tests/challenge_host_test.c (and, for the plants that name one, tests/catch_test.c or tests/mystery_teleport_test.c): show they can fail. Each plant
# changes one line of a copy of game-core; every copy's test must exit with the plant's status (1, the tests' CHECK; 134, an assert's abort).
# Usage: tools/challengehostplant.sh
set -e
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-challenge-host-plant}
fails=0; total=0
plant() {
  NAME=$1; FILE=$2; EXPR=$3; TEST=${4:-challenge_host_test}; WANT=${5:-1}
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  mkdir -p "$W/build"
  for d in ai-core reward-core run-core; do [ ! -d "$HERE/build/$d" ] || cp -a "$HERE/build/$d" "$W/build/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/$TEST" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$W/build/$TEST" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME ($TEST): exit $st"
  total=$((total + 1)); [ "$st" = "$WANT" ] || fails=$((fails + 1))
}
plant played-mask-moveset-randomizer gc_challenge.c 's/| (1u << CHALLENGES_HARDCORE);/| (1u << CHALLENGES_HARDCORE) | (1u << CHALLENGES_MOVESET_RANDOMIZER);/'
plant battle-rules-not-on-the-run-battle gc_challenge.c 's/run->bt.challenge_passives = r->mode == GAME_MODES_CHALLENGE ? r->challenge_passives : 0;/run->bt.challenge_passives = 0;/'
plant nuzlocke-without-hardcore gc_challenge.c 's/v\[CHALLENGES_LIMITED_CATCH\] \&\& v\[CHALLENGES_HARDCORE\]) f |= GC_RIBBON_NUZLOCKE/v[CHALLENGES_LIMITED_CATCH]) f |= GC_RIBBON_NUZLOCKE/'
plant nuzlocke-without-fresh gc_challenge.c 's/v\[CHALLENGES_FRESH_START\] \&\& v\[CHALLENGES_LIMITED_CATCH\] \&\& v\[CHALLENGES_HARDCORE\]) f |= GC_RIBBON_NUZLOCKE/v[CHALLENGES_LIMITED_CATCH] \&\& v[CHALLENGES_HARDCORE]) f |= GC_RIBBON_NUZLOCKE/'
plant flip-inverse-keeps-others gc_challenge.c 's/if (flip_or_inverse) return flip_or_inverse;/if (flip_or_inverse) return f;/'
plant fresh-max-3 gc_challenge.c 's/    2,  \/\* FRESH_START \*\//    3,  \/* FRESH_START *\//'
plant moveset-randomizer-ribbons gc_challenge.c 's/if (v\[CHALLENGES_MOVESET_RANDOMIZER\]) return 0;//'
plant save-no-value-check gc_save.c 's/||!gc_challenge_set_valid(&chal,NULL)//'
plant save-takes-unplayed gc_save.c 's/if (chal_flags \& ~gc_challenge_played_mask())/if (0)/'
plant finish-takes-unplayed gc_classic.c 's/gc_challenges_unplayed(run)/0/'
plant party-never-valid gc_challenge.c 's/        any_valid |= valid;/        any_valid |= 0;/'
# Limited Support and Hardcore in the run's phases (PR 3)
plant support-value-not-synced gc_challenge.c 's/r->challenge_limited_support = set->value\[CHALLENGES_LIMITED_SUPPORT\];/r->challenge_limited_support = 0;/'
plant no-heal-still-heals gc_phases.c 's/if (pr_run_get_party_heal_status(\&run->run)) gc_rq_unshift/if (1) gc_rq_unshift/'
plant no-heal-no-screen gc_phases.c 's/else gc_rq_unshift(run, gc_rp(RP_SELECT_MODIFIER, 0, 0, 0));/else (void)0;/'
plant hardcore-heal-revives gc_phases.c 's/if (prevent_revive \&\& b->hp <= 0) continue;/if (0) continue;/'
plant hardcore-heal-skips-the-living gc_phases.c 's/if (prevent_revive \&\& b->hp <= 0) continue;/if (prevent_revive) continue;/'
# Limited Catch: the catch that does not join, the obtain that does, AttemptCapturePhase's text, Teleporting Hijinks' exception
plant catch-ignores-limited gc_catch.c 's/const bool added = p->c == 1 || gc_challenge_add_to_party_ok(run);/const bool added = true;/' catch_test
plant catch-obtain-refused gc_catch.c 's/const bool added = p->c == 1 || gc_challenge_add_to_party_ok(run);/const bool added = gc_challenge_add_to_party_ok(run);/' catch_test
plant catch-text-not-told gc_battle.c 's/bt->catch_not_added = !gc_challenge_add_to_party_ok(run);/bt->catch_not_added = false;/' catch_test
plant catch-safari-text gc_catch.c 's/if (p->c == 3) gc_ev_msg(run, added ? "battle:pokemonCaught" : "battle:pokemonCaughtButChallenge"/if (p->c == 3) gc_ev_msg(run, "battle:pokemonCaught"/' catch_test
plant catch-first-wave-off gc_challenge.c 's/return gc_int((gc_run \*)run)->me_teleport_catch || run->wave % 10 == 1;/return gc_int((gc_run *)run)->me_teleport_catch || run->wave % 10 == 0;/' catch_test
plant teleport-no-catch-flag gc_teleport.c 's/    gc_int(run)->me_teleport_catch = true;.*$/    (void)0;/' mystery_teleport_test 134
plant teleport-flag-not-cleared gc_encounter.c 's/    in->me_teleport_catch = false;/    (void)0;/' mystery_teleport_test 134
# (the "no member is hard-valid" branch of gc_challenge_party_ok is not reachable with Fresh Start alone: every member is hard-valid or refused first;
#  it gets its test and plant with Single Type, the first challenge with a soft-valid member)
echo "challenge host plants: $total run, $fails not caught"
[ "$fails" = 0 ]
