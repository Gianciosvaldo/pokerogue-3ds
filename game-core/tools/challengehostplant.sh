#!/bin/sh
# tests/challenge_host_test.c (and, for the plants that name one, tests/catch_test.c or tests/mystery_teleport_test.c): show they can fail. Each plant
# changes one line of a copy of game-core (or, for a FILE under battle-core/, of battle-core); every copy's test must exit with the plant's status
# (1, the tests' CHECK; 134, an assert's abort).
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
  mkdir -p "$W/build"
  case $FILE in
  battle-core/*)
    # a battle-core plant: copies of every core (run-core and ai-core build battle-core from their own ../battle-core), built from scratch
    for d in battle-core run-core ai-core reward-core; do mkdir -p "$W/$d"; (cd "$ROOT/$d" && tar cf - --exclude=./build --exclude=./node_modules .) | tar xf - -C "$W/$d"; done
    SRC="$ROOT/$FILE"; DST="$W/$FILE" ;;
  *)
    for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
    for d in ai-core reward-core run-core; do [ ! -d "$HERE/build/$d" ] || cp -a "$HERE/build/$d" "$W/build/$d"; done
    SRC="$HERE/src/$FILE"; DST="$W/game-core/src/$FILE" ;;
  esac
  sed -i "$EXPR" "$DST"
  n=$(diff "$SRC" "$DST" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j8 -C "$W/game-core" BUILD="$W/build" "$W/build/$TEST" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$W/build/$TEST" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME ($TEST): exit $st"
  total=$((total + 1)); [ "$st" = "$WANT" ] || fails=$((fails + 1))
}
plant played-mask-moveset-randomizer gc_challenge.c 's/| (1u << CHALLENGES_SINGLE_TYPE);/| (1u << CHALLENGES_SINGLE_TYPE) | (1u << CHALLENGES_MOVESET_RANDOMIZER);/'
plant battle-rules-not-on-the-run-battle gc_challenge.c 's/    gc_challenge_to_battle(run, \&run->bt);/    (void)0;/'
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
# Single Type (PR 4a): the admission, the party rule's soft step, isAllowedInChallenge, TurnInitPhase's switch, the party screens' refusal, ribbons
plant single-type-unplayed gc_challenge.c 's/ | (1u << CHALLENGES_HARDCORE) | (1u << CHALLENGES_SINGLE_TYPE);/ | (1u << CHALLENGES_HARDCORE);/'
plant type-not-on-the-battle gc_challenge.c 's/bt->challenge_type = challenge ? run->run.challenge_type : 0;/bt->challenge_type = 0;/'
plant party-soft-alone-ok gc_challenge.c 's/    if (!any_valid) { \*why = "starterSelectUiHandler:invalidParty"; return false; }/    (void)any_valid;/'
plant soft-no-item-forms gc_challenge.c 's/if (r->species == species \&\& choice_valid(set, species, r->to)) return true;/(void)r;/'
plant allowed-counts-tera battle-core/src/damage.c 's/pr_get_types(p, types, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);/pr_get_types(p, types, PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);/'
plant allowed-judges-the-foe battle-core/src/damage.c 's/    if (!bt || !p || !p->is_player) return true;/    if (!bt || !p) return true;/'
plant allowed-no-castform battle-core/src/damage.c 's/if (!of_type \&\& !(want == POKEMON_TYPE_NORMAL \&\& p->species == SPECIES_ID_CASTFORM)) return false;/if (!of_type) return false;/'
plant allowed-fainted battle-core/src/damage.c 's/return p \&\& p->hp > 0 \&\& pr_challenge_allowed(bt, p);/return p \&\& pr_challenge_allowed(bt, p);/'
plant turn-init-no-check battle-core/src/engine.c 's/    if (!any) return false;/    return false;/'
plant turn-init-leaves-only battle-core/src/engine.c 's/if (n_allowed >= battlers || (bt->double_battle \&\& !first_active)) {/if (0) {/'
plant turn-init-not-in-game-core gc_phases.c 's/if (illegal >= 0) { after_stop(run, illegal); return true; }/(void)illegal;/'
plant switch-refusal-no-energy gc_phases.c 's/: barred ? "partyUiHandler:cantBeUsed" : "partyUiHandler:noEnergy";/: "partyUiHandler:noEnergy";/'
plant switch-takes-the-barred gc_battle.c 's/if (k < 0 || k >= run->n_party || run->party\[k\].b.on_field || !gc_mon_allowed(run, \&run->party\[k\])) {/if (k < 0 || k >= run->n_party || run->party[k].b.on_field || run->party[k].b.hp <= 0) {/'
plant ribbons-for-the-barred gc_classic.c 's/if(!pr_challenge_allowed(\&run->bt,/if(0\&\&!pr_challenge_allowed(\&run->bt,/'
plant switch-screen-offers-the-barred gc_battle.c 's/s->pick_ok\[i\] = gc_mon_allowed(run, \&run->party\[i\]) \&\& !run->party\[i\].b.on_field \&\& \&run->party\[i\].b != leaving;/s->pick_ok[i] = run->party[i].b.hp > 0 \&\& !run->party[i].b.on_field \&\& \&run->party[i].b != leaving;/'
echo "challenge host plants: $total run, $fails not caught"
[ "$fails" = 0 ]
