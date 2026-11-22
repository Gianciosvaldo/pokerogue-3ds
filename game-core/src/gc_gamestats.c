/* game-core: upstream's GameStats (src/system/game-stats.ts), recorded where upstream records them. The counters live in the profile (gc_system: stats and
 * gstats, see gc/system.h for which field is which upstream counter). The seen, caught and hatched ones are written inside gc_system.c's
 * setPokemonSeen / setPokemonSpeciesCaught, which they belong to; this file has the rest. */
#include "gc/system.h"
#include "internal.h"

static void inc(uint32_t *v) { if (*v < UINT32_MAX) (*v)++; }
static void raise(uint32_t *v, double to) {
    if (to > (double)*v) *v = to >= (double)UINT32_MAX ? UINT32_MAX : (uint32_t)to;
}

void gc_stats_session_started(gc_system *sys, int game_mode) {
    if (!sys) return;
    if (game_mode == GAME_MODES_DAILY) inc(&sys->gstats.daily_run_sessions_played);   /* TitlePhase */
    else if (game_mode == GAME_MODES_CLASSIC || game_mode == GAME_MODES_CHALLENGE) inc(&sys->gstats.classic_sessions_played);   /* gameMode.isClassic */
    else inc(&sys->gstats.endless_sessions_played);
}

void gc_stats_battle_end(gc_system *sys, int game_mode, int wave, bool victory, bool trainer) {
    if (!sys) return;
    inc(&sys->stats.total_battles);
    if ((game_mode == GAME_MODES_ENDLESS || game_mode == GAME_MODES_SPLICED_ENDLESS) && wave + 1 > 0)   /* gameMode.isEndless */
        raise(&sys->gstats.highest_endless_wave, (double)wave + 1);
    if (victory && trainer) inc(&sys->gstats.trainers_defeated);
}

void gc_stats_pokemon_defeated(gc_system *sys) { if (sys) inc(&sys->gstats.pokemon_defeated); }
void gc_stats_level_up(gc_system *sys, int level) { if (sys && level > 0) raise(&sys->gstats.highest_level, (double)level); }
void gc_stats_pokemon_fused(gc_system *sys) { if (sys) inc(&sys->gstats.pokemon_fused); }
void gc_stats_damage(gc_system *sys, int32_t damage) { if (sys && damage > 0) raise(&sys->stats.highest_damage, (double)damage); }
void gc_stats_heal(gc_system *sys, int32_t amount) { if (sys && amount > 0) raise(&sys->gstats.highest_heal, (double)amount); }
void gc_stats_run_loaded(gc_system *sys, double money) { if (sys) raise(&sys->gstats.highest_money, money); }
void gc_stats_add_play_time(gc_system *sys, uint32_t seconds) {
    if (sys) sys->gstats.play_time = seconds > UINT32_MAX - sys->gstats.play_time ? UINT32_MAX : sys->gstats.play_time + seconds;
}
