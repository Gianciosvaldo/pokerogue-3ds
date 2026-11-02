#!/bin/sh
# The evil teams' fixed fights in the run: show the checks can fail. Replays one trace (from tools/evildiff.sh) through copies of game-core and run-core,
# each with one line of src/newbattle.c (run-core) or src/gc_encounter.c (game-core) changed, and runs both the differential (tests/gym_e4_diff_test.c)
# and the host test (tests/gym_e4_test.c) on it. Every copy must fail at least one of them (exit 1); the first column says which caught it.
# Admin and boss plants cover the group choice (pool, offset, shuffle), the gender roll, the boss pool index and form, the flags and the closure members.
# Post-process plants cover the ops interpreter (guards, each field, the random form and move, the move slots), the pending member RNG and the game-core
# glue. Not planted (equivalent): a skipped REGEN (the producer stub and the constructor both leave moves 1..4), a generateName, the wave-equal guard alone (with the stub the two branches of a
# wave if/else end in the same form), a found move making LACKS_MOVE false (no test move is in 1..4).
# Not planted (equivalent under both): the order of the ctx assignments, the comments, the party template roll in trainer_ctor (the Youngster and the grunts have
# one template, so randSeedItem rolls nothing).
# Usage: tools/evilplant.sh <trace>   (EVIL_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-evil-plant}
fails=0; total=0
started=0
# plant NAME SED-EXPRESSION [FILE under game-core/src, or run-core/src/<file> as "run-core:<file>"]
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_encounter.c}
  [ "$NAME" != "${EVIL_PLANT_FROM:-}" ] || started=1
  [ -z "${EVIL_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core" "$W/run-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  (cd "$ROOT/run-core" && tar cf - --exclude=./build .) | tar xf - -C "$W/run-core"
  for d in battle-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  # the libraries already built (make rebuilds what is out of date: the changed copy of run-core always is)
  mkdir -p "$W/build"
  for d in ai-core reward-core; do [ ! -d "$HERE/build/$d" ] || cp -a "$HERE/build/$d" "$W/build/$d"; done
  case "$FILE" in
    run-core:*) ORIG="$ROOT/run-core/src/${FILE#run-core:}"; COPY="$W/run-core/src/${FILE#run-core:}" ;;
    *) ORIG="$HERE/src/$FILE"; COPY="$W/game-core/src/$FILE" ;;
  esac
  sed -i "$EXPR" "$COPY"
  n=$(diff "$ORIG" "$COPY" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/gym_e4_diff_test" "$W/build/gym_e4_test" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  d=0; h=0
  "$W/build/gym_e4_diff_test" "$TRACE" >"$W/diff.txt" 2>&1 || d=$?
  "$W/build/gym_e4_test" >"$W/host.txt" 2>&1 || h=$?
  who=none; [ "$d" = 1 ] && who=diff; [ "$h" = 1 ] && { [ "$who" = none ] && who=host || who=diff+host; }
  echo "$NAME: caught by $who (diff exit $d: $(grep 'TOTAL' "$W/diff.txt"); host exit $h: $(tail -1 "$W/host.txt" | cut -c1-90))"
  total=$((total + 1))
  [ "$who" != none ] || fails=$((fails + 1))
}
N=run-core:newbattle.c
plant grunt-pool-short      's/evil_grunts\[seed_item(&r, 10)\]/evil_grunts[seed_item(\&r, 9)]/' $N
plant grunt-offset-per-wave 's/(fixed_evil == 1 ? wave : CLASSIC_FIXED_BOSS_WAVES_EVIL_GRUNT_1) << 8/wave << 8/' $N
plant youngster-offset      's/(fixed_evil == 1 ? wave :/(fixed_evil == 1 ? wave + 1 :/' $N
plant youngster-gender      's/out->variant = pr_rng_seed_int(&r, 2, 0) ?/out->variant = !pr_rng_seed_int(\&r, 2, 0) ?/' $N
plant gender-roll-inverted  's/ctx->evil_gender_roll == 0 ? TRAINER_VARIANT_FEMALE/ctx->evil_gender_roll != 0 ? TRAINER_VARIANT_FEMALE/' $N
plant double-roll-shifted   's/ctx->evil_double_roll == 0) out->variant/ctx->evil_double_roll == 1) out->variant/' $N
plant double-never          's/if (t->has_double \&\& ctx->evil_double_roll == 0)/if (0 \&\& t->has_double \&\& ctx->evil_double_roll == 0)/' $N
plant names-wrong-gender    's/(out->variant == TRAINER_VARIANT_FEMALE ? t->names_female : t->names_male)/(out->variant == TRAINER_VARIANT_FEMALE ? t->names_male : t->names_female)/' $N
plant grunts-not-fixed      's/const int fixed_evil = ctx->enable_evil_grunts ? evil_grunt_kind(run->mode, wave) : 0;/const int fixed_evil = 0;/' $N
plant evil-off              's/    ctx.enable_evil_grunts = true;/    ctx.enable_evil_grunts = false;/'
plant pin-ignored           's/in->evil_roll_pinned ? in->evil_roll\[0\] : //'
plant last-admin-no-roll    's/wave == CLASSIC_FIXED_BOSS_WAVES_EVIL_ADMIN_3) {/wave == 0) {/'
plant admin-pool-short      's/const evil_entry \*c = \&pool\[seed_item(r, 10)\];/const evil_entry *c = \&pool[seed_item(r, 9)];/' $N
plant admin-offset-ignored  's/if (seed_offset == 0 || c->n <= seed_offset) return/if (1) return/' $N
plant admin-unshuffled      's/return a\[seed_offset\];/return a[0];/' $N
plant admin-shuffle-seed    's/pr_rng r0 = pr_run_seed_offset_rng(run, 0, NULL);/pr_rng r0 = pr_run_seed_offset_rng(run, 1, NULL);/' $N
plant admin-gender-inverted 's/out->variant = ctx->evil_gender_roll ? TRAINER_VARIANT_DEFAULT : TRAINER_VARIANT_FEMALE;/out->variant = ctx->evil_gender_roll ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT;/' $N
plant boss-pool-short       's/evil_bosses\[fixed_leader - 6\]\[seed_item(\&r, 10)\]/evil_bosses[fixed_leader - 6][seed_item(\&r, 9)]/' $N
plant boss-first-form       's/evil_bosses\[fixed_leader - 6\]\[/evil_bosses[0][/' $N
plant boss-female           's/out->variant = TRAINER_VARIANT_DEFAULT;$/out->variant = TRAINER_VARIANT_FEMALE;/' $N
plant leaders-not-fixed     's/const int fixed_leader = ctx->enable_evil_bosses ? evil_leader_kind(run->mode, wave) : 0;/const int fixed_leader = 0;/' $N
plant bosses-off            's/    ctx.enable_evil_bosses = true;/    ctx.enable_evil_bosses = false;/'
plant closure-evolves       's/if (!cl->ignore_evolution) {/if (1) {/' run-core:evil_team.c
plant closure-never-evolves 's/if (!cl->ignore_evolution) {/if (0) {/' run-core:evil_team.c
plant closure-no-pick       's/const int pick = cl->n == 1 ? 0 : pr_rng_integer_in_range(\&run->rng, 0, cl->n - 1);/const int pick = 0;/' run-core:evil_team.c
plant ops-never-run         's/for (int i = 0; i < cl->op_n; i++) {/for (int i = 0; i < 0; i++) {/' run-core:evil_team.c
plant if-skip-short         's/if (!guard_holds(op, ctx, out)) i += op->a; break;/if (!guard_holds(op, ctx, out)) i += op->a - 1; break;/' run-core:evil_team.c
plant guard-species-flipped 's/case PR_EG_SPECIES_EQ: return ctx->species == op->gv;/case PR_EG_SPECIES_EQ: return ctx->species != op->gv;/' run-core:evil_team.c
plant guard-wave-ne-flipped 's/case PR_EG_WAVE_NE: return ctx->wave != op->gv;/case PR_EG_WAVE_NE: return ctx->wave == op->gv;/' run-core:evil_team.c
plant guard-lacks-never     '/if (g->moveset\[k\] == op->gv) return false;/{n;s/return true;/return false;/}' run-core:evil_team.c
plant boss-segments-ignored 's/case PR_EOP_BOSS: out->boss_segments = op->a; break;/case PR_EOP_BOSS: break;/' run-core:evil_team.c
plant ability-ignored       's/case PR_EOP_ABILITY: out->ability_index = op->a; break;/case PR_EOP_ABILITY: break;/' run-core:evil_team.c
plant gender-ignored        's/case PR_EOP_GENDER: out->gender = op->a; break;/case PR_EOP_GENDER: break;/' run-core:evil_team.c
plant form-ignored          's/case PR_EOP_FORM: out->form = op->a; break;/case PR_EOP_FORM: break;/' run-core:evil_team.c
plant friendship-ignored    's/case PR_EOP_FRIENDSHIP: out->friendship = op->a; break;/case PR_EOP_FRIENDSHIP: break;/' run-core:evil_team.c
plant form-rand-range       's/op->a - 1 + op->b); break;/op->a + op->b); break;/' run-core:evil_team.c
plant move-rand-no-roll     's/(n == 1 ? 0 : pr_rng_integer_in_range(rng, 0, n - 1))/0/' run-core:evil_team.c
plant move-slot-shifted     's/case PR_EOP_MOVE: out->moveset\[op->a\] = op->b; break;/case PR_EOP_MOVE: out->moveset[(op->a + 1) % 4] = op->b; break;/' run-core:evil_team.c
plant moveset-array-reversed 's/out->moveset\[k\] = pr_evil_op_items\[op->a + k\];/out->moveset[3 - k] = pr_evil_op_items[op->a + k];/' run-core:evil_team.c
plant pending-never-set     's/out->evil_pending = true;/out->evil_pending = false;/' run-core:evil_team.c
plant finish-ambient-rng    's/out->evil_pending ? \&out->evil_rng : \&run->rng/\&run->rng/' run-core:pokemongen.c
plant moves-not-applied     's/^        post_apply_moves(m, \&party.mons\[e\]);.*$/        ;/'
plant ctor-moveset-skipped  's/^        post_moveset(run, m, \&party.mons\[e\]);.*$/        ;/'
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
