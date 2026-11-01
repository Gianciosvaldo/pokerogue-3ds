#!/bin/sh
# The player's Terastallize command: show the checks can fail. Replays one trace (from tools/teracmddiff.sh) through copies of game-core, each with one
# line of src/gc_battle.c or src/gc_reward.c changed, and runs both the differential (tests/tera_command_diff_test.c) and the host test
# (tests/tera_command_test.c) on it. Every copy must fail at least one of them (exit 1); the first column says which caught it.
# Not planted (equivalent under both): the "is_player" test of gc_can_tera_command (a player slot is always the player's), the comments.
# Usage: tools/teracmdplant.sh <trace>   (TERA_PLANT_FROM=<name> starts at that plant)
set -e
TRACE=$(CDPATH= cd -- "$(dirname "${1:?trace}")" && pwd)/$(basename "$1")
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-tera-plant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=${3:-gc_battle.c}
  [ "$NAME" != "${TERA_PLANT_FROM:-}" ] || started=1
  [ -z "${TERA_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do ln -s "$ROOT/$d" "$W/$d"; done
  # the other cores are not changed: start from the libraries already built (make rebuilds what is out of date)
  mkdir -p "$W/build"
  for d in ai-core reward-core run-core; do [ ! -d "$HERE/build/$d" ] || cp -a "$HERE/build/$d" "$W/build/$d"; done
  sed -i "$EXPR" "$W/game-core/src/$FILE"
  n=$(diff "$HERE/src/$FILE" "$W/game-core/src/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/tera_command_diff_test" "$W/build/tera_command_test" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  d=0; h=0
  "$W/build/tera_command_diff_test" "$TRACE" >"$W/diff.txt" 2>&1 || d=$?
  "$W/build/tera_command_test" >"$W/host.txt" 2>&1 || h=$?
  who=none; [ "$d" = 1 ] && who=diff; [ "$h" = 1 ] && { [ "$who" = none ] && who=host || who=diff+host; }
  echo "$NAME: caught by $who (diff exit $d: $(grep 'TOTAL' "$W/diff.txt" | cut -c1-70); host exit $h: $(tail -1 "$W/host.txt" | cut -c1-80))"
  total=$((total + 1))
  [ "$who" != none ] || fails=$((fails + 1))
}
plant limit-two             's/#define GC_MAX_TERAS_PER_ARENA 1/#define GC_MAX_TERAS_PER_ARENA 2/'
plant limit-off-by-one      's/return run->bt.player_teras_used < GC_MAX_TERAS_PER_ARENA \&\& gc_can_species_tera(run, p);/return run->bt.player_teras_used <= GC_MAX_TERAS_PER_ARENA \&\& gc_can_species_tera(run, p);/'
plant planned-ignored       's/const int planned = run->bt.pre_commands\[0\].command == COMMAND_TERA \&\& field > 0;/const int planned = 0;/'
plant planned-on-slot-0     's/const int planned = run->bt.pre_commands\[0\].command == COMMAND_TERA \&\& field > 0;/const int planned = run->bt.pre_commands[0].command == COMMAND_TERA;/'
plant tera-pre-skipped      's/pc->skip = !tera;   \/\* skip: command === Command.FIGHT \*\//pc->skip = true;/'
plant tera-pre-not-tera     's/pc->command = tera ? COMMAND_TERA : COMMAND_FIGHT;/pc->command = COMMAND_FIGHT;/'
plant tera-gate-removed     's/if (tera \&\& !gc_can_tera_command(run, field)) {/if (tera \&\& false) {/'
plant button-not-offered    's/s->can_tera = p \&\& gc_can_tera_command(run, field);/s->can_tera = false;/'
plant button-type-raw       's/s->tera_type = s->can_tera ? (int8_t)pr_get_tera_type(p) : (int8_t)POKEMON_TYPE_UNKNOWN;/s->tera_type = s->can_tera ? p->tera_type : (int8_t)POKEMON_TYPE_UNKNOWN;/'
plant orb-not-needed        's/if (gc_item_stack(run, "TERA_ORB") <= 0 || pr_is_mega(p) || pr_is_max(p)) return false;/if (pr_is_mega(p) || pr_is_max(p)) return false;/' gc_reward.c
plant mega-allowed          's/if (gc_item_stack(run, "TERA_ORB") <= 0 || pr_is_mega(p) || pr_is_max(p)) return false;/if (gc_item_stack(run, "TERA_ORB") <= 0 || pr_is_max(p)) return false;/' gc_reward.c
plant max-allowed           's/if (gc_item_stack(run, "TERA_ORB") <= 0 || pr_is_mega(p) || pr_is_max(p)) return false;/if (gc_item_stack(run, "TERA_ORB") <= 0 || pr_is_mega(p)) return false;/' gc_reward.c
plant ultra-allowed         's/if (p->species == SPECIES_ID_NECROZMA \&\& gc_species(p->species) \&\& !strcmp(gc_form(p->species, p->form)->form_key, "ultra"))/if (false)/' gc_reward.c
plant fusion-ultra-allowed  's/if (p->fusion_species == SPECIES_ID_NECROZMA \&\& gc_species(p->fusion_species) \&\&/if (false \&\& gc_species(p->fusion_species) \&\&/' gc_reward.c
echo "planted bugs caught: $((total - fails)) of $total"
[ "$fails" = 0 ]
