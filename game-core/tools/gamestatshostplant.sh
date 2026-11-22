#!/bin/sh
# The Game Stats hooks (not the seen / caught / hatched counters, which tools/gamestatsplant.sh covers): show tests/gamestats_host_test.c can fail.
# Each plant changes one line of a copy of the tree (game-core or battle-core) and the host test must then exit non-zero.
# Usage: tools/gamestatshostplant.sh   (GAMESTATS_HOST_PLANT_FROM=<name> starts at that plant)
set -e
HERE=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
ROOT=$(CDPATH= cd -- "$HERE/.." && pwd)
WORK=${WORK:-/tmp/game-core-gamestats-hostplant}
fails=0; total=0
started=0
plant() {
  NAME=$1; EXPR=$2; FILE=$3   # FILE relative to the repository root
  [ "$NAME" != "${GAMESTATS_HOST_PLANT_FROM:-}" ] || started=1
  [ -z "${GAMESTATS_HOST_PLANT_FROM:-}" ] || [ "$started" = 1 ] || return 0
  W="$WORK/$NAME"
  rm -rf "$W"; mkdir -p "$W/game-core" "$W/build"
  (cd "$HERE" && tar cf - --exclude=./build .) | tar xf - -C "$W/game-core"
  for d in battle-core run-core ai-core reward-core; do
    # run-core builds ../battle-core, so a battle-core plant needs its own copy of both (run-core's compiled objects are rebuilt by make)
    case "$FILE" in battle-core/*) cp -a "$ROOT/$d" "$W/$d" ;; *) ln -s "$ROOT/$d" "$W/$d" ;; esac
  done
  for d in ai-core reward-core run-core; do [ ! -d "$HERE/build/$d" ] || cp -a "$HERE/build/$d" "$W/build/$d"; done
  sed -i "$EXPR" "$W/$FILE"
  n=$(diff "$ROOT/$FILE" "$W/$FILE" | grep -c '^>' || true)
  [ "$n" = 1 ] || { echo "$NAME: the change didn't apply ($n lines)"; exit 2; }
  make -s -j2 -C "$W/game-core" BUILD="$W/build" "$W/build/gamestats_host_test" >/dev/null 2>&1 || { echo "$NAME: the changed copy doesn't build"; exit 2; }
  st=0; "$W/build/gamestats_host_test" >"$W/out.txt" 2>&1 || st=$?
  echo "$NAME: exit $st, $(tail -1 "$W/out.txt")"
  total=$((total + 1))
  [ "$st" != 0 ] || fails=$((fails + 1))
}
G=game-core/src
plant victory-not-counted        's/if (!gc_int(run)->me_no_stats) gc_stats_pokemon_defeated(run->sys);/(void)0;/' $G/gc_phases.c
plant victory-ignores-me-flag    's/if (!gc_int(run)->me_no_stats) gc_stats_pokemon_defeated(run->sys);/gc_stats_pokemon_defeated(run->sys);/' $G/gc_phases.c
plant level-up-not-counted       's/^    gc_stats_level_up(run->sys, p->c);/    (void)p;/' $G/gc_phases.c
plant battle-end-not-counted     's/gc_stats_battle_end(run->sys, run->run.mode, run->wave, victory, run->battle_type == BATTLE_TYPE_TRAINER);/(void)0;/' $G/gc_phases.c
plant battle-end-trainer-always  's/victory, run->battle_type == BATTLE_TYPE_TRAINER);/victory, true);/' $G/gc_phases.c
plant session-not-counted        's/gc_stats_session_started(run->sys, run->run.mode);/(void)0;/' $G/gc_run.c
plant damage-hook-not-installed  's/bt->game_stat = battle_game_stat;/bt->game_stat = NULL;/' $G/gc_battle.c
plant endless-wave-in-classic    's/if ((game_mode == GAME_MODES_ENDLESS || game_mode == GAME_MODES_SPLICED_ENDLESS) \&\& wave + 1 > 0)/if (wave + 1 > 0)/' $G/gc_gamestats.c
plant endless-wave-as-index      's/raise(&sys->gstats.highest_endless_wave, (double)wave + 1);/raise(\&sys->gstats.highest_endless_wave, (double)wave);/' $G/gc_gamestats.c
plant highest-lowers             's/if (to > (double)\*v) \*v =/if (to != (double)*v) *v =/' $G/gc_gamestats.c
plant lost-trainer-counted       's/if (victory \&\& trainer) inc/if (trainer) inc/' $G/gc_gamestats.c
plant endless-counted-as-classic 's/else inc(&sys->gstats.endless_sessions_played);/else inc(\&sys->gstats.classic_sessions_played);/' $G/gc_gamestats.c
plant damage-hook-player-only    's/if (user->is_player \&\& bt->game_stat)/if (bt->game_stat)/' battle-core/src/phases.c
plant heal-hook-player-only      's/if (p->is_player \&\& bt->game_stat) bt->game_stat(bt, PR_GAME_STAT_HEAL/if (bt->game_stat) bt->game_stat(bt, PR_GAME_STAT_HEAL/' battle-core/src/phases.c
plant heal-hook-removed          's/if (p->is_player \&\& bt->game_stat) bt->game_stat(bt, PR_GAME_STAT_HEAL/if (0 \&\& bt->game_stat) bt->game_stat(bt, PR_GAME_STAT_HEAL/' battle-core/src/phases.c
plant save-section-dropped       's/if (memcmp(&sys->gstats, &none, sizeof none)) {/if (0) {/' $G/gc_save.c
echo "plants: $total run, $fails not caught"
[ "$fails" = 0 ]
