#!/bin/sh
# Teleporting Hijinks: show the comparator (and the host test) can fail. Replays one trace (from tools/teleportdiff.sh) through copies of
# game-core, each with one line of src/gc_teleport.c (or gc_mystery.c) changed, and prints each one's totals. Every copy must be caught: by
# tests/mystery_teleport_diff_test.c exiting 1, or, for what the oracle does not play (the old arena left behind, the helper's experience after the win), by tests/mystery_teleport_test.c failing.
# Not planted: the arena tags and player faints cleared by the new Arena (the next battle's setup clears them for any mystery encounter anyway: nothing differs), battle-core's post-summon handler itself (its stage change and message are compared through the stages and the text event), the capsule sprite, the tween and the music (the app's).
# Usage: tools/teleportplant.sh <trace>   (TELEPORT_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-teleport-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_teleport.c}
  [ "$NAME" != "${TELEPORT_PLANT_FROM:-}" ] || started=1
  [ -z "${TELEPORT_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  [ -z "${TELEPORT_PLANT_ONLY:-}" ] || case " $TELEPORT_PLANT_ONLY " in *" $NAME "*) ;; *) return 0;; esac
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  TARGET="$W/game-core/src/$FILE"; ORIG="$HERE/src/$FILE"
  sed -i "$EXPR" "$TARGET"
  n=$(diff "$ORIG" "$TARGET" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j4 -C "$W/game-core" BUILD="$W/build" "$W/build/mystery_teleport_diff_test" "$W/build/mystery_teleport_test" >/dev/null 2>&1
  st=0; "$W/build/mystery_teleport_diff_test" "$TRACE" >"$W/out.txt" 2>&1 || st=$?
  by=comparator
  if [ "$st" = 0 ]; then
    st=0; "$W/build/mystery_teleport_test" >"$W/host.txt" 2>&1 || st=$?
    [ "$st" = 0 ] || { st=1; by=host-test; }
  fi
  echo "$NAME: exit $st ($by), $(grep 'TOTAL' "$W/out.txt" | tail -1 | sed 's/.*TOTAL/TOTAL/')"
  total=$((total + 1))
  [ "$st" = 1 ] || fails=$((fails + 1))
  rm -rf "$W"
}
plant price-multiplier 's|#define MONEY_COST_MULTIPLIER 1.75|#define MONEY_COST_MULTIPLIER 1.5|'
plant biome-order 's|{BIOME_ID_SPACE, BIOME_ID_FAIRY_CAVE, |{BIOME_ID_FAIRY_CAVE, BIOME_ID_SPACE, |'
plant biome-dojo-missing 's|, BIOME_ID_DOJO};|, BIOME_ID_ISLAND};|'
plant biome-current-allowed 's|if (CANDIDATES\[i\] != run->biome) filtered|if (1) filtered|'
plant biome-stream '/^static void teleport/,/^}/ s|pr_rng stream = gc_me_option_rng(run);|pr_rng stream = run->run.rng;|'
plant biome-min 's|filtered\[pr_rng_seed_int(\&stream, n, 0)\]|filtered[pr_rng_seed_int(\&stream, n, 1)]|'
plant pays-nothing 's|run->money = fmin(fmax(run->money - price, 0.0), 9007199254740991.0);|(void)price;|'
plant pays-twice 's|run->money = fmin(fmax(run->money - price, 0.0), 9007199254740991.0);|run->money = fmin(fmax(run->money - 2 * price, 0.0), 9007199254740991.0);|'
plant biome-not-set 's|    run->biome = biome;|    (void)biome;|'
plant weather-kept 's|    bt->weather = WEATHER_TYPE_NONE;|    (void)bt;|'
plant terrain-kept 's|    bt->terrain = TERRAIN_NONE;|    (void)bt;|'
plant teras-kept 's|    bt->player_teras_used = 0;|    (void)bt;|'
plant transport-text-lost 's|    gc_ev_msg(run, "mysteryEncounters/teleportingHijinks:transport", 0);|    (void)run;|'
plant attacked-text-lost 's|    gc_ev_msg(run, "mysteryEncounters/teleportingHijinks:attacked", 0);|    (void)run;|'
plant text-order 's|gc_ev_msg(run, "mysteryEncounters/teleportingHijinks:attacked", 0);|gc_ev_msg(run, "mysteryEncounters/teleportingHijinks:transport", 0);|'
plant rng-not-kept 's|    run->run.rng = r;|    (void)r;|'
plant boss-stream-inspect '/^static void inspect/,/^}/ s|pr_rng stream = gc_me_option_rng(run);|pr_rng stream = run->run.rng;|'
plant post-summon-none 's|run->wave < 50 ? PR_ME_POST_TELEPORT_LOW : PR_ME_POST_TELEPORT_HIGH|PR_ME_POST_NONE|'
plant post-summon-swapped 's|run->wave < 50 ? PR_ME_POST_TELEPORT_LOW : PR_ME_POST_TELEPORT_HIGH|run->wave < 50 ? PR_ME_POST_TELEPORT_HIGH : PR_ME_POST_TELEPORT_LOW|'
plant post-summon-threshold 's|run->wave < 50 ? PR_ME_POST_TELEPORT_LOW|run->wave < 60 ? PR_ME_POST_TELEPORT_LOW|'
plant post-summon-berries 's|run->wave < 50 ? PR_ME_POST_TELEPORT_LOW : PR_ME_POST_TELEPORT_HIGH|run->wave < 50 ? PR_ME_POST_ENRAGED_LOW : PR_ME_POST_ENRAGED_HIGH|'
plant helper-storage-order 's|if (machine_type(\&run->party\[run->party_order\[k\]\])) return run->party_order\[k\];|if (machine_type(\&run->party[k])) return k;|'
plant helper-steel-only 's|types\[i\] == POKEMON_TYPE_ELECTRIC \|\| types\[i\] == POKEMON_TYPE_STEEL|types[i] == POKEMON_TYPE_STEEL|'
plant helper-electric-only 's|types\[i\] == POKEMON_TYPE_ELECTRIC \|\| types\[i\] == POKEMON_TYPE_STEEL|types[i] == POKEMON_TYPE_ELECTRIC|'
plant helper-fainted-ok 's|if (m->b.hp > 0 \&\& machine_type(m)) return true;|if (machine_type(m)) return true;|'
plant helper-none 's|        teleport(run, first_helper(run));|        teleport(run, -1);|'
plant exp-amount 's|    in->me_exp.base = 100; |    in->me_exp.base = 50; |'
plant exp-wave 's|    in->me_exp.wave = true;|    in->me_exp.wave = false;|'
plant exp-pending 's|    in->me_exp.pending = primary >= 0;|    in->me_exp.pending = false;|'
plant exp-callback 's|    in->me_reward.before = primary >= 0;|    in->me_reward.before = false;|'
plant exp-primary 's|    in->me_exp.primary = (int8_t)primary;|    in->me_exp.primary = 0;|'
plant reward-forgotten 's|    in->me_reward.active = true;|    in->me_reward.active = false;|'
plant reward-no-fill 's|    in->me_reward.fill = true;|    in->me_reward.fill = false;|'
plant reward-func 's|    in->me_reward.func = -1;|    in->me_reward.func = 0;|'
plant boosters-steel 's|more\[0\] = pr_reward_pregen_option(ctx, PR_ITEM_ATTACK_TYPE_BOOSTER, POKEMON_TYPE_STEEL);|more[0] = pr_reward_pregen_option(ctx, PR_ITEM_ATTACK_TYPE_BOOSTER, POKEMON_TYPE_ELECTRIC);|'
plant boosters-electric 's|more\[1\] = pr_reward_pregen_option(ctx, PR_ITEM_ATTACK_TYPE_BOOSTER, POKEMON_TYPE_ELECTRIC);|more[1] = pr_reward_pregen_option(ctx, PR_ITEM_ATTACK_TYPE_BOOSTER, POKEMON_TYPE_STEEL);|'
plant boosters-count 's|PR_ME_POST_NONE, more, 2, -1);|PR_ME_POST_NONE, more, 1, -1);|'
plant boosters-order 's|more\[0\] = pr_reward_pregen_option(ctx, PR_ITEM_ATTACK_TYPE_BOOSTER, POKEMON_TYPE_STEEL);|more[1] = pr_reward_pregen_option(ctx, PR_ITEM_ATTACK_TYPE_BOOSTER, POKEMON_TYPE_STEEL);|'
plant inspect-teleports 's|        inspect(run);|        teleport(run, -1);|'
plant catch-off 's|    case GC_ME_FIGHT_OR_FLIGHT: case GC_ME_TELEPORTING_HIJINKS: case GC_ME_DARK_DEAL: case GC_ME_MYSTERIOUS_CHEST:|    case GC_ME_FIGHT_OR_FLIGHT: case GC_ME_DARK_DEAL: case GC_ME_MYSTERIOUS_CHEST:|' gc_mystery.c
plant money-strict 's|        s->option_ok\[0\] = run->money >= price;|        s->option_ok[0] = run->money > price;|' gc_mystery.c
plant helper-option-always-on 's|        s->option_ok\[1\] = gc_teleport_helper_ok(run);|        s->option_ok[1] = true;|' gc_mystery.c
plant price-token '/case GC_ME_TELEPORTING_HIJINKS: {/,/break;/ s|        screen_arg(s, pr_arg_money_en_us("price", price));|        (void)price;|' gc_mystery.c
[ "$fails" = 0 ] || { echo "$fails of $total planted bug(s) not caught"; exit 1; }
echo "all $total planted bugs caught"
