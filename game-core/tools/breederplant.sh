#!/bin/sh
# The Expert Pokemon Breeder: show the comparator (and the host test) can fail. Replays one trace (from tools/breederdiff.sh) through copies of game-core, each with one line of src/gc_breeder.c (or gc_mystery.c, gc_phases.c, gc_encounter.c, gc_soothe.c, gc_gacha.c) changed, and prints each one's totals. Every copy must be caught: by tests/mystery_breeder_diff_test.c exiting 1, or, for what the oracle does not play (the battle itself, the shop, the loss through the game-over phase, the party coming back in order), by tests/mystery_breeder_test.c failing.
# Not planted (equivalent mutants or not reachable by the oracle): the Cleffa's Tera slot in the moveset (the oracle draws the same with and without it), the trainer's name string and the Cleffa nicknames (no engine field), the pool evolution wave of 60 (the oracle never starts a wave 60), the stub counters, and the availability check in gc_mystery.c (static; the trace never offers the encounter with fewer than three able Pokemon).
# Usage: tools/breederplant.sh <trace>   (BREEDER_PLANT_FROM=<name> starts at that plant, BREEDER_PLANT_ONLY="a b" runs those)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-breeder-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_breeder.c}
  [ "$NAME" != "${BREEDER_PLANT_FROM:-}" ] || started=1
  [ -z "${BREEDER_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${BREEDER_PLANT_ONLY:-}" ] || case " $BREEDER_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  case "$FILE" in
    battle-core/*) rm "$W/battle-core"; (cd "$ROOT" && tar cf - --exclude=./battle-core/build battle-core) | tar xf - -C "$W"; TARGET="$W/$FILE"; ORIG="$ROOT/$FILE";;
    *) TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE";;
  esac
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" BATTLE_CORE="$W/battle-core" "$W/build/mystery_breeder_diff_test" "$W/build/mystery_breeder_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_breeder_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_breeder_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}

EGGS='/^void gc_breeder_eggs/,/^}/'
plant eggs-bst-step "$EGGS"'s|(680 - bst) / 20.0|(680 - bst) / 25.0|'
plant eggs-bst-min "$EGGS"'s|if (from_bst < 1) from_bst = 1;|if (from_bst < 0) from_bst = 0;|'
plant eggs-bst-max "$EGGS"'s|if (from_bst > 18) from_bst = 18;|if (from_bst > 17) from_bst = 17;|'
plant eggs-cost-step "$EGGS"'s|from_cost = (7 - cost) \* 2;|from_cost = (7 - cost) * 3;|'
plant eggs-cost-max "$EGGS"'s|if (from_cost > 12) from_cost = 12;|if (from_cost > 11) from_cost = 11;|'
plant eggs-cost-min "$EGGS"'s|if (from_cost < 0) from_cost = 0;|(void)from_cost;|'
plant eggs-cost-root "$EGGS"'s|gc_starter_cost(pr_run_species_root(m->b.species))|gc_starter_cost(m->b.species)|'
plant eggs-total-cap "$EGGS"'s|if (total > 30) total = 30;|if (total > 29) total = 29;|'
plant eggs-common-five "$EGGS"'s|int c = total < 5 ? total : 5;|int c = total < 4 ? total : 4;|'
plant eggs-rare-step "$EGGS"'s|\*rares = total / 4;|*rares = total / 5;|'
plant eggs-leftover "$EGGS"'s|c += total % 4;|c += total % 3;|'
plant picks-allowed 's|if (gc_mon_allowed(\&run->party\[idx\])) list\[n++\] = idx;|list[n++] = idx;|'
plant picks-unstable 's|run->party\[list\[j\]\].b.friendship > run->party\[v\].b.friendship|run->party[list[j]].b.friendship >= run->party[v].b.friendship|'
plant picks-storage-order 's|const int idx = run->party_order\[k\];|const int idx = k;|'
plant picks-three 's|if (n < 3) return false;|if (n < 2) return false;|'
plant stage-first 's|#define FIRST_STAGE 45|#define FIRST_STAGE 46|'
plant stage-final 's|#define FINAL_STAGE 75|#define FINAL_STAGE 76|'
plant stage-cleffa 's|return wave < FIRST_STAGE ? SPECIES_ID_CLEFFA|return wave <= FIRST_STAGE ? SPECIES_ID_CLEFFA|'
plant stage-evolve 's|while (k > 0 \&\& c->l\[k\].wave \&\& c->l\[k\].wave > wave) k--;|while (k > 0 \&\& c->l[k].wave \&\& c->l[k].wave >= wave) k--;|'
plant pool-shuffle-end 's|for (int i = n - 1; i > 0; i--) {|for (int i = n - 1; i > 1; i--) {|'
plant pool-shuffle-range 's|const int j = pr_rng_integer_in_range(r, 0, i);|const int j = pr_rng_integer_in_range(r, 0, i - 1);|'
plant pool-last 's|const chain_t \*c = \&pool\[idx\[n - 1\]\];|const chain_t *c = \&pool[idx[0]];|'
plant pool-chain-start 's|int k = c->n - 1;|int k = c->n - 2 < 0 ? 0 : c->n - 2;|'
plant init-stream 's|pr_run_seed_offset_rng(\&run->run, run->wave, NULL);|pr_run_seed_offset_rng(\&run->run, run->wave + 1, NULL);|'
plant init-second-pool 's|out\[2\] = species_from_pool(POOL2, 7, run->wave, \&r);|out[2] = species_from_pool(POOL2, 6, run->wave, \&r);|'
plant init-first-pool 's|out\[1\] = species_from_pool(POOL1, 17, run->wave, \&r);|out[1] = species_from_pool(POOL1, 16, run->wave, \&r);|'
plant init-space 's|if (run->biome == BIOME_ID_SPACE) {|if (run->biome == BIOME_ID_CAVE) {|'
plant away-items-kept 's|^            in->breeder.kept\[in->breeder.n_kept++\] = run->items\[i\];|            (void)i;|'
plant back-items 's|^    for (int k = 0; k < in->breeder.n_kept \&\& run->n_items.*|    (void)in;|'
plant back-order 's|run->party_order\[run->n_party\] = (int8_t)run->n_party;|run->party_order[run->n_party] = (int8_t)(run->n_party ^ 1);|'
plant back-view 's|run->bt.n_party\[0\] = (uint8_t)run->n_party;|run->bt.n_party[0] = 1;|'
plant cfg-female 's|const bool female = pr_rng_seed_int(\&run->run.rng, 2, 0) != 0;|const bool female = pr_rng_seed_int(\&run->run.rng, 2, 0) == 0;|'
plant cfg-trainer-early 's|^    run->trainer_type = trainer;   /\* the trainer exists|    (void)trainer;   /* the trainer exists|'
plant cfg-template-size 's|if (tpl \&\& tpl->size == 3) {|if (tpl \&\& tpl->size == 4) {|'
plant cfg-nature 's|{ nature = NATURE_ADAMANT; ability = 1; mv = ace; }|{ nature = NATURE_JOLLY; ability = 1; mv = ace; }|'
plant cfg-ability-first 's|{ nature = NATURE_ADAMANT; ability = 1; mv = ace; }|{ nature = NATURE_ADAMANT; ability = 0; mv = ace; }|'
plant cfg-ability-third 's|else { nature = NATURE_BOLD; ability = 2; mv = the_great; variant = 2; }|else { nature = NATURE_BOLD; ability = 1; mv = the_great; variant = 2; }|'
plant cfg-variant-second 's|else if (e == 1) { nature = NATURE_MODEST; ability = 1; mv = great; variant = 1; }|else if (e == 1) { nature = NATURE_MODEST; ability = 1; mv = great; variant = 2; }|'
plant cfg-nature-third 's|else { nature = NATURE_BOLD; ability = 2; mv = the_great; variant = 2; }|else { nature = NATURE_CALM; ability = 2; mv = the_great; variant = 2; }|'
plant cfg-moves 's|static const int ace\[4\] = {MOVE_ID_FIRE_PUNCH, MOVE_ID_ICE_PUNCH, MOVE_ID_THUNDER_PUNCH, MOVE_ID_METEOR_MASH};|static const int ace[4] = {MOVE_ID_FIRE_PUNCH, MOVE_ID_ICE_PUNCH, MOVE_ID_THUNDER_PUNCH, MOVE_ID_SWORDS_DANCE};|'
plant cfg-great-moves 's|MOVE_ID_MYSTICAL_FIRE, MOVE_ID_ICE_BEAM, MOVE_ID_THUNDERBOLT};|MOVE_ID_MYSTICAL_FIRE, MOVE_ID_ICE_BEAM, MOVE_ID_THUNDER};|'
plant cfg-ivs 's|for (int i = 0; i < 6; i++) m->b.ivs\[i\] = 31;|for (int i = 0; i < 6; i++) m->b.ivs[i] = 30;|'
plant cfg-tera 's|if (e == 0) m->b.tera_type = POKEMON_TYPE_FAIRY;|if (e == 0) m->b.tera_type = POKEMON_TYPE_STEEL;|'
plant cfg-friendship 's|    m->b.friendship = 255;|    m->b.friendship = 254;|'
plant cfg-pool-members 's|if (e > 0 \&\& !space) return;|if (e > 1 \&\& !space) return;|'
plant cfg-shiny 's|        m->b.shiny = true;|        m->b.shiny = false;|'
plant fight-bell-stack 's|        bells\[e\].stack = 3;|        bells[e].stack = 2;|'
plant fight-bell-key 's|        bells\[e\].key = "SOOTHE_BELL";|        bells[e].key = "LEFTOVERS";|'
plant fight-bell-holder 's|bells\[e\].pokemon_id = run->enemy\[e\].b.id;|bells[e].pokemon_id = run->enemy[0].b.id;|'
plant fight-tera-slot 's|in->me_tera_static = 1;|in->me_tera_static = 2;|'
plant fight-wild-pool '/^    run->me_battle = true;$/{N;s|run->me_battle = true;|run->me_battle = false;|}'
plant fight-reward-func 's|in->me_reward.func = (int16_t)gc_reward_item_index("SOOTHE_BELL");|in->me_reward.func = (int16_t)gc_reward_item_index("LEFTOVERS");|'
plant fight-reward-fill 's|    in->me_reward.fill = true;|    in->me_reward.fill = false;|'
plant fight-reward-before 's|    in->me_reward.before = true;|    in->me_reward.before = false;|'
plant fight-reward-off 's|^    in->me_reward.active = true;|    in->me_reward.active = false;|'
plant fight-catch 's|run->me_catch_allowed = false;|run->me_catch_allowed = true;|'
plant fight-outro 's|    in->me_post.pending = true;   /\* the outro: gc_me_win|    in->me_post.pending = false;   /* the outro: gc_me_win|'
plant fight-outro-kind 's|^    in->me_post.b = 0;|    in->me_post.b = 1;|'
plant fight-variant 's|run->trainer_variant = female ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT;|run->trainer_variant = female ? TRAINER_VARIANT_DEFAULT : TRAINER_VARIANT_FEMALE;|'
plant fight-enemies 's|^    run->n_enemy = 3;|    run->n_enemy = 2;|'
plant win-friendship 's|gc_soothe_friendship(run, (int)(m - run->party), 20)|gc_soothe_friendship(run, (int)(m - run->party), 10)|'
plant win-others-back '/^void gc_breeder_reward/,/^}/s|^    restore_others(run);|    (void)run;|'
plant win-eggs-tier 's|const int tiers\[2\] = {0, 1};|const int tiers[2] = {1, 0};|'
plant win-eggs-source 's|gc_system_gift_egg_source(run->sys, 4, tiers\[t\], seed, 0);|gc_system_gift_egg_source(run->sys, 3, tiers[t], seed, 0);|'
plant win-eggs-count 's|const int count = t == 0 ? in->breeder.commons : in->breeder.rares;|const int count = t == 0 ? in->breeder.commons : in->breeder.rares + 1;|'
plant win-chosen 's|const int chosen = pick\[option\];|const int chosen = pick[0];|'
plant loss-friendship 's|if (m) m->b.friendship = 0;|if (m) m->b.friendship = 1;|'
plant loss-failed 's|in->breeder.failed = true;|in->breeder.failed = false;|'
plant loss-rewards 's|^    in->me_reward.active = false;$|    in->me_reward.active = in->me_reward.active;|'
plant loss-outro-kind 's|^    in->me_post.b = 1;|    in->me_post.b = 0;|'
plant loss-others-back '/^bool gc_breeder_game_over/,/^}/s|^    restore_others(run);|    (void)run;|'
plant loss-able 's|able = able \|\| gc_mon_allowed(\&run->party\[i\]);|able = true;|'
plant loss-battle-end 's|gc_rq_push(run, gc_rp(RP_BATTLE_END, 0, 0, 0));|gc_rq_push(run, gc_rp(RP_BATTLE_END, 1, 0, 0));|'
plant outro-failed-key 's|gc_ev_msg(run, NS ":outroFailed", 0);|gc_ev_msg(run, NS ":outro", 0);|'
plant outro-count 's|pr_arg_int("count", counts\[t\]), pr_arg_key("rarity", rarity\[t\]));|pr_arg_int("count", counts[t] + 1), pr_arg_key("rarity", rarity[t]));|'
plant outro-rarity 's|static const char \*const rarity\[2\] = {"egg:defaultTier", "egg:greatTier"};|static const char *const rarity[2] = {"egg:greatTier", "egg:defaultTier"};|'
plant outro-skip-none 's|if (counts\[t\] <= 0) continue;|if (counts[t] < 0) continue;|'
plant outro-chosen 's|if (m) gc_ev_msg(run, NS ":outro", 1, pr_arg_pokemon_name(\&run->bt, "chosenPokemon", \&m->b));|if (m) gc_ev_msg(run, NS ":outro", 1, pr_arg_int("chosenPokemon", 0));|'
MYS='/case GC_ME_THE_EXPERT_POKEMON_BREEDER: {/,/break;/'
plant ui-name-token "$MYS"'s|"pokemon2Name", "pokemon3Name"|"pokemon3Name", "pokemon2Name"|' gc_mystery.c
plant ui-common-token "$MYS"'s|screen_arg(s, pr_arg_int(commons\[k\], c));|screen_arg(s, pr_arg_int(commons[k], r));|' gc_mystery.c
plant ui-rare-token "$MYS"'s|screen_arg(s, pr_arg_int(rares\[k\], r));|screen_arg(s, pr_arg_int(rares[k], c + 1));|' gc_mystery.c
plant ui-species "$MYS"'s|pr_arg_pokemon_name(\&run->bt, names\[k\], \&run->party\[pick\[k\]\].b)|pr_arg_pokemon_name(\&run->bt, names[k], \&run->party[pick[0]].b)|' gc_mystery.c
plant dispatch-outro 's|if (p->a == GC_ME_THE_EXPERT_POKEMON_BREEDER) { gc_breeder_outro(run, p); return; }|if (p->a == GC_ME_THE_EXPERT_POKEMON_BREEDER) return;|' gc_mystery.c
plant leave-post 's|^    if (in->me_post.pending) {|    if (0 \&\& in->me_post.pending) {|' gc_mystery.c
plant phase-reward 's|gc_teleport_reward(run); gc_breeder_reward(run); }|gc_teleport_reward(run); }|' gc_phases.c
plant phase-game-over 's|if (!p->a \&\& gc_breeder_game_over(run)) return true;|if (0 \&\& gc_breeder_game_over(run)) return true;|' gc_phases.c
plant pool-me-battle 's|const int pool = run->battle_type == BATTLE_TYPE_TRAINER \&\& !run->me_battle ? 1 : 0;|const int pool = run->battle_type == BATTLE_TYPE_TRAINER ? 1 : 0;|' gc_encounter.c
plant soothe-base 's|!pr_reward_soothe_bell_gain(\&c,target,base,\&amount,NULL)|!pr_reward_soothe_bell_gain(\&c,target,3,\&amount,NULL)|' gc_soothe.c
plant egg-source 's|c.machine=source;|c.machine=0;|' gc_gacha.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
