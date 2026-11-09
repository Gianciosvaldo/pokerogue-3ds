#!/bin/sh
# Weird Dream (src/gc_weird.c): show the comparator can fail. Replays one trace (from tools/weirddiff.sh) through copies of game-core, each with one line of src/gc_weird.c
# (or gc_movegen.c) changed, and prints each one's totals. Every copy must be caught by tests/mystery_weird_diff_test.c exiting 1.
# Not planted, equivalent: the lower clamp of the range (a base stat total is never below 1, so 0 and 1 give the same list), the held-item merge and its fit test (a new Pokemon holds no
# copy of what it is given, and the Old Gateau's maximum is one), the Comatose guard of the status copy (no species the dream makes in the traced runs has Comatose: Komala is not drawn),
# the single-species candidate edges (exactly 21 candidates; a base stat total of exactly 601), the form of the old Pokemon in the range (the traced party has no form with another total),
# the shiny test of the dex unlock (a shiny over 570 is never drawn), the third egg-move retry and the favored move's known test (never reached in the traced runs: a known move must come up
# three times in a row / first in the generated list).
# Not planted (equivalent or out of the oracle's reach): the 200-pass guard of the species loop and its return 0 (upstream would loop for ever), the stub paths for a species the
# data does not cover, the Item-limit note (engine limit), the on_field / switch_out_status flags of the new Pokemon (no battle follows), the fusion branch of the luck sum (the party has no
# fusions), and the Keldeo branch of the player moveset generator (Keldeo cannot be made by the dream).
# build-gimmighoul is caught only by seeds 2024, 31337 and 9 (a Gimmighoul is not drawn in the seed 777 trace): run it on those traces too.
# Usage: tools/weirdplant.sh <trace>   (WEIRD_PLANT_FROM=<name> starts at that plant, WEIRD_PLANT_ONLY="a b" runs those)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-weird-plant}
fails=0; total=0
started=0
# one working copy for all plants: its library is built once, then each plant changes one line, compiles that file again, runs and puts the line back
W="$WORK/tree"
rm -rf "$W"; mkdir -p "$W/game-core"
(cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
make -s -j4 -C "$W/game-core" BUILD="$W/build" BATTLE_CORE="$W/battle-core" "$W/build/mystery_weird_diff_test" >/dev/null 2>&1

plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_weird.c}
  [ "$NAME" != "${WEIRD_PLANT_FROM:-}" ] || started=1
  [ -z "${WEIRD_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${WEIRD_PLANT_ONLY:-}" ] || case " $WEIRD_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
  TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE"
  cp "$ORIG" "$TARGET"
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" BATTLE_CORE="$W/battle-core" "$W/build/mystery_weird_diff_test" "$W/build/mystery_weird_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_weird_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_weird_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  cp "$ORIG" "$TARGET"
}

TRANS='/^static int transformed_species/,/^}/'
PLAN='/^bool gc_weird_plan/,/^}/'
BUILD='/^static bool build_all/,/^}/'
EGG='/^static int add_egg_move/,/^}/'
FAV='/^static void add_favored_move/,/^}/'
STARTER='/^static bool unlocks_new_starter/,/^}/'
POST='/^static bool post_process/,/^}/'
HELD='/^static void add_held/,/^}/'
XFORM='/^static void transform/,/^}/'
LOSE='/^static void lose_levels/,/^}/'

plant range-high "$TRANS"'s#original_bst + rng_range\[1\];#original_bst + rng_range[1] - 1;#'
plant range-edge-low "$TRANS"'s#bst >= min \&\& bst <= cap#bst > min \&\& bst <= cap#'
plant range-edge-high "$TRANS"'s#bst >= min \&\& bst <= cap#bst >= min \&\& bst < cap#'
plant legend-gate "$TRANS"'s#bst < NON_LEGENDARY_BST || bst > SUPER_LEGENDARY_BST#bst <= NON_LEGENDARY_BST || bst > SUPER_LEGENDARY_BST#'
plant super-gate "$TRANS"'s#(!has_super || bst <= SUPER_LEGENDARY_BST)#(!has_super || bst < SUPER_LEGENDARY_BST)#'
plant widen-low "$TRANS"'s#rng_range\[0\] -= 10;#rng_range[0] -= 5;#'
plant widen-high "$TRANS"'s#rng_range\[1\] += 10;#rng_range[1] += 5;#'
plant shuffle-from "$TRANS"'s#for (int i = n - 1; i > 0; i--) {#for (int i = n - 2; i > 0; i--) {#'
plant shuffle-range "$TRANS"'s#pr_rng_integer_in_range(r, 0, i)#pr_rng_integer_in_range(r, 0, i - 1)#'
plant shuffle-pick "$TRANS"'s#const int id = list\[--n\];#const int id = list[n >= 2 ? --n - 1 : --n];#'
plant used-check "$TRANS"'s#if (used\[k\] == id) { seen = true; break; }#if (used[k] == id \&\& k > 0) { seen = true; break; }#'
plant exclude-arceus 's#SPECIES_ID_ARCEUS, SPECIES_ID_ETERNATUS,#SPECIES_ID_ETERNATUS, SPECIES_ID_ETERNATUS,#'
plant exclude-ub 's#SPECIES_ID_NIHILEGO, SPECIES_ID_BUZZWOLE,#SPECIES_ID_BUZZWOLE, SPECIES_ID_BUZZWOLE,#'
plant exclude-paradox 's#SPECIES_ID_IRON_BOULDER, SPECIES_ID_IRON_CROWN,#SPECIES_ID_IRON_BOULDER, SPECIES_ID_IRON_BOULDER,#'
plant exclude-last 's#SPECIES_ID_OGERPON, SPECIES_ID_OKIDOGI, SPECIES_ID_MUNKIDORI, SPECIES_ID_FEZANDIPITI, SPECIES_ID_TERAPAGOS,#SPECIES_ID_OGERPON, SPECIES_ID_OKIDOGI, SPECIES_ID_MUNKIDORI, SPECIES_ID_FEZANDIPITI, SPECIES_ID_OGERPON,#'
plant exclude-trio 's#SPECIES_ID_PHIONE, SPECIES_ID_TYPE_NULL,#SPECIES_ID_TYPE_NULL, SPECIES_ID_TYPE_NULL,#'
plant plan-offset "$PLAN"'s#run->wave, NULL#run->wave + 1, NULL#'
plant plan-shuffle-range "$PLAN"'s#pr_rng_integer_in_range(\&r, 0, i)#pr_rng_integer_in_range(\&r, 0, i - 1)#'
plant plan-used-party "$PLAN"'s#used\[n_used++\] = run->party\[run->party_order\[k\]\].b.species;#n_used += 0;#'
plant plan-high-count "$PLAN"'s#range\[i < 2 ? 0 : 1\]#range[i < 1 ? 0 : 1]#'
plant plan-high-count-3 "$PLAN"'s#range\[i < 2 ? 0 : 1\]#range[i < 3 ? 0 : 1]#'
plant plan-legend-flag "$PLAN"'s#new_bst >= NON_LEGENDARY_BST) has_legend = true;#new_bst > NON_LEGENDARY_BST) has_legend = true;#'
plant build-ability-count "$BUILD"'s#f->ability_hidden ? 3 : 2#f->ability_hidden ? 2 : 2#'
plant build-gimmighoul "$BUILD"'s#species == SPECIES_ID_GIMMIGHOUL ? 1 : -2#species == SPECIES_ID_GIMMIGHOUL ? 0 : -2#'
plant build-tera "$BUILD"'s#pr_rng_seed_int(\&r, 18, 0)#pr_rng_seed_int(\&r, 17, 0)#'
plant build-level "$BUILD"'s#prev->b.level, ability#prev->b.level + 1, ability#'
plant hidden-chance "$POST"'s#double chance = 256;#double chance = 128;#'
plant hidden-charm "$POST"'s#chance \*= js_pow(2, -1 - charms);#chance *= js_pow(2, -charms);#'
plant hidden-index "$POST"'s#base->ability2 ? 2 : 1#base->ability2 ? 1 : 1#'
plant hidden-skip "$POST"'s#if (m->ability_index < hidden_index) {#if (m->ability_index <= hidden_index) {#'
plant iv-range "$POST"'s#pr_rng_seed_int(\&run->run.rng, 31, 0);#pr_rng_seed_int(\&run->run.rng, 32, 0);#'
plant iv-max "$POST"'s#if (v > p->ivs\[k\]) p->ivs\[k\] = (uint8_t)v;#p->ivs[k] = (uint8_t)v;#'
plant nature-count "$POST"'s#pr_rng_seed_int(\&run->run.rng, 5, 0)\];#pr_rng_seed_int(\&run->run.rng, 4, 0)];#'
plant nature-list "$POST"'s#NATURE_HARDY, NATURE_DOCILE,#NATURE_HARDY, NATURE_HARDY,#'
plant dex-bst "$POST"'s#base_total <= NON_LEGENDARY_BST#base_total < NON_LEGENDARY_BST#'
plant dex-ivs "$POST"'s#gc_species_root_for_starter(p->species), p->ivs);#gc_species_root_for_starter(p->species), prev->b.ivs);#'
plant dex-message "$POST"'s#if (is_new_starter) {#if (is_new_starter \&\& 0) {#'
plant pokerus "$POST"'s#m->pokerus = prev->pokerus;#m->pokerus = false;#'
plant luck "$POST"'s#m->luck = (int8_t)(prev->luck + #m->luck = (int8_t)(0 + #'
plant iv-keep "$POST"'s#if (prev->b.ivs\[k\] > p->ivs\[k\]) p->ivs\[k\] = prev->b.ivs\[k\];#if (prev->b.ivs[k] > p->ivs[k] + 1) p->ivs[k] = prev->b.ivs[k];#'
plant candy "$POST"'s#gc_system_add_starter_candy(run->sys, (uint16_t)root, 1);#gc_system_add_starter_candy(run->sys, (uint16_t)root, 2);#'
plant moves-pp "$POST"'s#memcpy(p->pp_used, prev->b.pp_used, sizeof p->pp_used);#(void)0;#'
plant moves-ppup "$POST"'s#memcpy(p->pp_up, prev->b.pp_up, sizeof p->pp_up);#(void)0;#'
plant custom-type-first "$POST"'s#p->custom_types\[0\] = 0;#p->custom_types[0] = 1;#'
plant custom-type-count "$POST"'s#pr_rng_seed_int(\&run->run.rng, 18, 0) + 1#pr_rng_seed_int(\&run->run.rng, 17, 0) + 1#'
plant passive-copy "$POST"'s#p->passive_flag = prev->b.passive_flag;#p->passive_flag = false;#'
plant egg-shuffle "$EGG"'s#for (int i = 3; i > 0; i--) {#for (int i = 2; i > 0; i--) {#'
plant egg-slot "$EGG"'s#egg_index = pr_rng_seed_int(\&run->run.rng, 4, 0);#egg_index = pr_rng_seed_int(\&run->run.rng, 3, 0);#'
plant egg-unlock "$EGG"'s#run->sys->starters\[root\].egg_moves |= (uint8_t)(1u << idx);#run->sys->starters[root].egg_moves |= (uint8_t)(1u << (idx + 1));#'
plant egg-caught "$EGG"'s#run->sys->dex\[root\].caught_attr) run->sys->starters#run->sys->dex[root].caught_attr == 0) run->sys->starters#'
plant fav-type "$FAV"'s#pr_moves\[move\].type == types\[0\] \&\& !knows(m, move)#pr_moves[move].type == types[0]#'
plant fav-slot "$FAV"'s#int slot = pr_rng_seed_int(\&run->run.rng, 4, 0);#int slot = pr_rng_seed_int(\&run->run.rng, 3, 0);#'
plant fav-reroll "$FAV"'s#while (egg_index >= 0 \&\& slot == egg_index)#while (0 \&\& slot == egg_index)#'
plant starter-walk "$STARTER"'s#if (run->sys->dex\[species\].caught_attr == 0 \&\& gc_starter_cost(species) >= 0) return true;#if (run->sys->dex[species].caught_attr == 0) return true;#'
plant held-tracker "$HELD"'s#m->b.species != SPECIES_ID_GIMMIGHOUL \&\& m->b.fusion_species != SPECIES_ID_GIMMIGHOUL) return;#m->b.species != SPECIES_ID_GIMMIGHOUL \&\& m->b.fusion_species != SPECIES_ID_GIMMIGHOUL \&\& 0) return;#'
plant held-arg "$HELD"'s#have->arg == it.arg \&\& ##'
plant held-order "$XFORM"'s#for (int i = 0; i < held_n\[k\]; i++) add_held#for (int i = held_n[k] - 1; i >= 0; i--) add_held#'
plant gateau-bst "$XFORM"'s#base_total < NON_LEGENDARY_BST) {#base_total <= NON_LEGENDARY_BST) {#'
plant gateau-nontransfer "$XFORM"'s#.stack = 1, .non_transferable = true#.stack = 1, .non_transferable = false#'
plant hp-ceil "$XFORM"'s#m->b.hp = (int32_t)ceil(old_ratio \* m->b.stats\[0\]);#m->b.hp = (int32_t)(old_ratio * m->b.stats[0]);#'
plant hp-faint "$XFORM"'s#if (old_ratio > 0) {#if (old_ratio >= 0) {#'
plant status-turns "$XFORM"'s#m->b.status_toxic_turns = prev->b.status_toxic_turns;#(void)0;#'
plant passive-pick "$XFORM"'s#pr_rng_seed_int(\&run->run.rng, n_without, 0)#pr_rng_seed_int(\&run->run.rng, n_without > 1 ? n_without - 1 : 1, 0)#'
plant passive-set "$XFORM"'s#m->b.passive_flag = true;#m->b.passive_flag = m->b.passive_flag;#'
plant heal-queue "$XFORM"'s#gc_rq_unshift(run, gc_rp(RP_PARTY_HEAL, 0, 0, 0));#(void)0;#'
plant reward-mushroom "$XFORM"'s#"MEMORY_MUSHROOM", "ROGUE_BALL", "MINT", "MINT", "MINT", "MINT"#"MEMORY_MUSHROOM", "ROGUE_BALL", "MINT", "MINT", "MINT", "BERRY"#'
plant reward-fill "$XFORM"'s#c->fill_remaining = false;#c->fill_remaining = true;#'
plant message-cutscene "$XFORM"'s#"mysteryEncounters/weirdDream:option.1.cutscene"#"mysteryEncounters/weirdDream:option.1.dreamComplete"#'
plant message-order "$XFORM"'s#gc_ev_msg(run, "mysteryEncounters/weirdDream:option.1.dreamComplete", 0);#(void)0;#'
plant lose-pct "$LOSE"'s#((100 - 10) / 100.0)#((100 - 11) / 100.0)#'
plant lose-floor "$LOSE"'s#ceil(((100 - 10) / 100.0) \* m->b.level)#floor(((100 - 10) / 100.0) * m->b.level)#'
plant lose-min "$LOSE"'s#(level > 1 ? level : 1)#(level > 2 ? level : 2)#'
plant lose-exp "$LOSE"'s#m->exp = gc_level_total_exp(m->b.level, sp ? sp->growth_rate : 0);#(void)0;#'
plant lose-recalc "$LOSE"'s#gc_mon_recalc(run, m);#(void)0;#'

echo "$total planted, $fails not caught"
[ "$fails" = 0 ]
