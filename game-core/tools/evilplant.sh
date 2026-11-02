#!/bin/sh
# The evil teams' fixed fights in the run: show the checks can fail. Replays one trace (from tools/evildiff.sh) through copies of game-core and run-core,
# each with one line of src/newbattle.c (run-core) or src/gc_encounter.c (game-core) changed, and runs both the differential (tests/gym_e4_diff_test.c)
# and the host test (tests/gym_e4_test.c) on it. Every copy must fail at least one of them (exit 1); the first column says which caught it.
# Admin and boss plants cover the group choice (pool, offset, shuffle), the gender roll, the boss pool index and form, the flags and the closure members.
# Post-process plants cover the ops interpreter (guards, each field, the random form and move, the move slots, the regeneration), the pending member RNG and
# the game-core glue. Moveset plants cover the native generator's egg pool (level, weight, cap, counts, ordering against the TMs), the instant Tera slots, and
# the regeneration's own state (final IVs for Hidden Power, the constructor's stats, the boss status, the current form, ability and gender). The real-generator
# trace is the argument; a trace of evil fights followed by one made with DIFF_MODE=gym (cat both) also reaches the levels where the rare egg moves begin. The
# fallback for a member the generator declines (Keldeo) is not planted: no evil fight has one and its moves are not compared.
# Not planted (equivalent): a generateName, the wave-equal guard alone (with the stub the two branches of a wave if/else end in the same form).
# Not planted (the game data never reaches it, measured on 12000 cases): an egg-enabled member at exactly the egg level 80 (the levels jump past it), an egg-enabled
# boss at the rare-egg level 170 (the highest boss is 156), the regeneration's ctor-time gender instead of the final one (only gender abilities like Rivalry read it,
# and no regenerated member has one), and the Tera type's STAB bonus (every instant-Tera member's Tera type is already one of its own types).
# Not planted (needs a rare combination the traces do not reach): Snowscape/Hail and Sandstorm with an instant Tera, Tera Blast's STAB, Revelation Dance.
# Not planted (equivalent under both): the order of the ctx assignments, the comments, the party template roll in trainer_ctor (the Youngster and the grunts have
# one template, so randSeedItem rolls nothing).
# Usage: tools/evilplant.sh <trace>   (EVIL_PLANT_FROM=<name> starts at that plant; DIFF_MODE=gym traces cover the gym leaders' and
# Elite Four's constructor movesets: pass one as <trace> to run the moveset plants against it)
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
plant regen-skipped         's/if (ctx->post_regen \&\& ctx->post_regen(ctx->post_regen_user, out, rng) < 0) return -1;/;/' run-core:evil_team.c
plant ctor-rng-ambient      's/pr_rng \*rng = g->evil_pending ? \&g->evil_rng : \&r->rng;/pr_rng *rng = \&r->rng;/'
plant ctor-moves-not-applied 's/^    post_apply_moves(m, g);.*$/    ;/'
plant regen-ctor-ivs        's/^    for (int i = 0; i < 6; i++) p->ivs\[i\] = (uint8_t)g->ivs\[i\];.*$/    ;/'
plant regen-fresh-stats     's/\&cur, st->m, st->will_tera/\&cur, NULL, st->will_tera/'
plant regen-not-boss        's/    p->is_boss = g->boss_segments > 0;/    p->is_boss = false;/'
plant regen-stale-form      's/    p->form = (uint8_t)g->form;/    p->form = st->m->b.form;/'
plant regen-stale-ability   's/gc_form(p->species, g->form), g->ability_index);/gc_form(p->species, g->form), st->m->ability_index);/'
plant tera-slot-ignored     's/const bool will_tera = tc \&\& (tc->tera_static \& (1u << member));.*$/const bool will_tera = false;/'
# the Elite Four's and Champions' postProcess: the ops added for them, the closure attached to the member gym_e4.c makes, the regeneration's Tera type
plant jmp-ignored           's/        case PR_EOP_JMP: i += op->a; break;/        case PR_EOP_JMP: break;/' run-core:evil_team.c
plant tera-op-ignored       's/        case PR_EOP_TERA: out->tera_type = op->a; break;/        case PR_EOP_TERA: break;/' run-core:evil_team.c
plant tera-species-type1    's/const int t = pr_run_species_type(ctx->species, op->a);/const int t = pr_run_species_type(ctx->species, 1);/' run-core:evil_team.c
plant species-eq2-first     's/ctx->species == op->gv || ctx->species == (uint16_t)op->b;/ctx->species == op->gv;/' run-core:evil_team.c
plant species-ne2-either    's/ctx->species != op->gv \&\& ctx->species != (uint16_t)op->b;/ctx->species != op->gv || ctx->species != (uint16_t)op->b;/' run-core:evil_team.c
plant species-lacks-species 's/        if (ctx->species != op->gv) return false;/        ;/' run-core:evil_team.c
plant species-lacks-move    's/if (g->moveset\[k\] == op->b) return false;/if (0) return false;/' run-core:evil_team.c
plant attach-dropped        's/    if (func_key >= 0) pr_run_evil_attach(run, type, func_key, out);/    ;/' run-core:gym_e4.c
plant species-type2-as-1    's/return f \&\& which <= f->n_types ? f->types\[which - 1\] : -1;/return f \&\& which <= f->n_types ? f->types[0] : -1;/' run-core:pokemongen.c
plant regen-stale-tera      's/^    p->tera_type = g->tera_type;.*$/    ;/'
M=gc_movegen.c
plant egg-pool-dropped      's/if (!c->eggs_allowed || c->p.level < gc_mg_trainer_const.egg_level) return true;/if (1) return true;/' $M
plant egg-weight-unrounded  's/egg_to_level_weight + .5)/egg_to_level_weight)/' $M
plant egg-weight-uncapped   's/, gc_mg_trainer_const.egg_weight_max);/, 1e9);/' $M
plant egg-rare-always       's/const bool exclude_rare = c->boss || c->p.level < gc_mg_trainer_const.rare_egg_level;/const bool exclude_rare = false;/' $M
plant egg-rare-never        's/const bool exclude_rare = c->boss || c->p.level < gc_mg_trainer_const.rare_egg_level;/const bool exclude_rare = true;/' $M
plant egg-rare-weight       's/const double rare = normal \* (5.0 \/ 6);/const double rare = normal;/' $M
plant egg-tm-duplicate      's/ || pool_find(\&egg, move) >= 0 || pool_find(\&tm, move) >= 0) continue;/ || pool_find(\&tm, move) >= 0) continue;/' $M
plant egg-superceded-tm     's/filter_superceded_extra(\&tm, level, \&egg);/filter_superceded_extra(\&tm, level, NULL);/' $M
plant egg-source-untagged   's/c->source_of\[egg.e\[k\].move\] = 2;/c->source_of[egg.e[k].move] = 0;/' $M
plant egg-count-dropped     's/else if (c->source_of\[move\] == 2) c->egg_count++;/else if (0) c->egg_count++;/' $M
plant egg-cap-ignored       's/ || (nonlevel >= egg_cap \&\& c->source_of\[move\] == 2)) continue;/) continue;/' $M
plant nonlevel-tm-only      's/const int nonlevel = c->tm_count + c->egg_count;/const int nonlevel = c->tm_count;/' $M
plant tera-ignored          's/c.will_tera = trainer \&\& will_tera;/c.will_tera = false;/' $M
plant tera-blast-untyped    's/if (move == MOVE_ID_TERA_BLAST) return c->p.tera_type;/if (move == MOVE_ID_TERA_BLAST) return type;/' $M
plant tera-existing-types   's/const uint32_t existing = existing_damage_types(c, true);/const uint32_t existing = existing_damage_types(c, false);/' $M
plant keldeo-declined        's/if (!sp || (trainer \&\& !(run->trainer_type >= TRAINER_TYPE_LORELEI \&\& run->trainer_type <= TRAINER_TYPE_KIERAN) \&\& (sp->form_change_triggers/if (!sp || (trainer \&\& (sp->form_change_triggers/' $M
plant regen-boss-declined   's/ || (m->b.is_boss \&\& !stats_of) || m->b.passive_flag/ || m->b.is_boss || m->b.passive_flag/' $M
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
