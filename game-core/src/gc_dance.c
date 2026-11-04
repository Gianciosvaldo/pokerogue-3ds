/* Dancing Lessons (src/data/mystery-encounters/encounters/dancing-lessons-encounter.ts at pinned upstream 8555c08).
 *
 * onInit, in executeWithSeedOffset(.., waveIndex): an Oricorio of level getEncounterPokemonLevelForWave(1) made by `new EnemyPokemon(.., boss = false)`
 * (a wild enemy's generation, moveset and shiny roll included), Revelation Dance put into its moveset (appended under four moves, else in place of
 * the first), its form set by the biome (Baile in the Temple and Tall Grass, Pom-Pom on the Beach and Grass, Pa'u on the Island and in the Ruins, Sensu in
 * the Graveyard and the Badlands, else randSeedInt(4)), and put on the field again from its PokemonData. The encounter allows catching and not fleeing.
 *
 * Option 1, battle (the first await, the Oricorio's exit tween, takes everything after it off the option's stream: the run's own RNG): Revelation Dance
 * from the Oricorio at the player's lead starts the battle (IGNORE_PP); setEncounterRewards({BATON, fillRemaining: true}); the Oricorio fights as a boss
 * from its data source (a new id, the shield bars of getEncounterBossSegments, the held items rolled after the await), and its post-summon effect says
 * its text and raises Atk, Def, SpAtk and SpDef by one stage (not Spd; battle-core).
 * Option 2, learn its dance (any Pokemon of the party, fainted or not): LearnMovePhase gives it Revelation Dance, the encounter leaves with the heal shop.
 * Option 3, show it a dance (a Pokemon allowed in battle with one of the dance moves; then one of its dance moves): Revelation Dance is already known to the
 * Oricorio, the move shown is put into its moveset (appended under four moves, else in place of the fourth), the Oricorio is caught (no ball, no text) and
 * the encounter leaves with the heal shop.
 *
 * Not ported: the dance animation, the Oricorio's sprite and its exit tween. */
#include "internal.h"
#include <math.h>
#include <string.h>

typedef struct {
    gc_mon mon;
    int level;
} dance_t;

/* DANCING_MOVES (requirement-groups.ts) */
bool gc_is_dance_move(int move)
{
    return move == MOVE_ID_AQUA_STEP || move == MOVE_ID_CLANGOROUS_SOUL || move == MOVE_ID_DRAGON_DANCE || move == MOVE_ID_FEATHER_DANCE
        || move == MOVE_ID_FIERY_DANCE || move == MOVE_ID_LUNAR_DANCE || move == MOVE_ID_PETAL_DANCE || move == MOVE_ID_REVELATION_DANCE
        || move == MOVE_ID_QUIVER_DANCE || move == MOVE_ID_SWORDS_DANCE || move == MOVE_ID_TEETER_DANCE || move == MOVE_ID_VICTORY_DANCE;
}

/* MoveRequirement(DANCING_MOVES, true): allowed in battle, one of the moves among its moves */
bool gc_dance_has_move(const gc_mon *m)
{
    if (!gc_mon_allowed(m)) return false;
    for (int k = 0; k < m->b.n_moves; k++) if (m->b.moves[k] > 0 && gc_is_dance_move(m->b.moves[k])) return true;
    return false;
}

/* the first of its moves among the dance moves (the option's dialogue token) */
int gc_dance_first_move(const gc_mon *m)
{
    for (int k = 0; k < m->b.n_moves; k++) if (m->b.moves[k] > 0 && gc_is_dance_move(m->b.moves[k])) return m->b.moves[k];
    return 0;
}

/* the form by the biome: -1 = a random one */
static int form_of_biome(int biome)
{
    if (biome == BIOME_ID_TEMPLE || biome == BIOME_ID_TALL_GRASS) return 0;
    if (biome == BIOME_ID_BEACH || biome == BIOME_ID_GRASS) return 1;
    if (biome == BIOME_ID_ISLAND || biome == BIOME_ID_RUINS) return 2;
    if (biome == BIOME_ID_GRAVEYARD || biome == BIOME_ID_BADLANDS) return 3;
    return -1;
}

/* onInit on the stream of the wave seed; false when run-core has no roll for the biome and wave or the species */
static bool dance_init(gc_run *run, dance_t *d)
{
    if (run->n_party < 1) return false;
    const bool dbl = run->double_battle;
    run->double_battle = false;
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    d->level = gc_me_boosted_level(run, &r);
    const bool ok = gc_me_new_enemy(run, &r, SPECIES_ID_ORICORIO, d->level, false, false, &d->mon);
    run->double_battle = dbl;
    if (!ok) return false;
    gc_mon *o = &d->mon;
    bool knows = false;
    for (int k = 0; k < o->b.n_moves; k++) knows = knows || o->b.moves[k] == MOVE_ID_REVELATION_DANCE;
    if (!knows) {
        if (o->b.n_moves < 4) gc_mon_set_move(o, o->b.n_moves, MOVE_ID_REVELATION_DANCE);
        else gc_mon_set_move(o, 0, MOVE_ID_REVELATION_DANCE);
    }
    int form = form_of_biome(run->biome);
    if (form < 0) form = pr_rng_seed_int(&r, 4, 0);
    o->b.form = (uint8_t)form;
    /* the Pokemon as the form left it: its stats and a full HP */
    pr_calculate_stats(&run->bt, &o->b);
    o->b.hp = o->b.stats[STAT_HP];
    return true;
}

/* the dialogue token onInit sets (oricorioName) for gc_mystery_setup_screen */
void gc_dance_tokens(gc_run *run, pr_msg_arg *name)
{
    (void)run;
    *name = pr_arg_key("oricorioName", "pokemon:oricorio");
}

/* the Oricorio as onInit leaves it, for the sprite and the oracle's test */
bool gc_dance_roll(gc_run *run, gc_mon *mon)
{
    static dance_t d;
    if (!dance_init(run, &d)) return false;
    *mon = d.mon;
    return true;
}

/* option 1 */
static void fight(gc_run *run, const dance_t *d)
{
    gc_internal *in = gc_int(run);
    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = (int16_t)gc_reward_item_index("BATON");
    in->me_reward.fill = true;
    in->me_reward.before = false;
    gc_me_add_start_effect(run, 2, 0, MOVE_ID_REVELATION_DANCE);
    /* the exit tween is the first await: the rest is on the run's own RNG */
    gc_me_start_boss_battle_at_wave_level(run, &d->mon, PR_ME_POST_DANCE, &run->run.rng, GC_ME_DANCING_LESSONS);
}

/* catchPokemon(oricorio, null, POKEBALL, false): the Pokemon as an enemy, RP_CATCH (c = 2: no "obtained" text) */
static void catch_it(gc_run *run, const gc_mon *mon)
{
    run->enemy[0] = *mon;
    run->n_enemy = 1;
    /* the Oricorio comes from its data: it holds nothing */
    run->n_enemy_items = 0;
    gc_items_to_battle(run);
    gc_int(run)->thrown_ball = POKEBALL_TYPE_POKEBALL;
    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 2));
}

/* whether the party screen lets `idx` be picked for `option` (0 = learn its dance, 1 = show it a dance, as the screen numbers them from the second option) */
bool gc_dance_pick_ok(const gc_run *run, int option, int idx)
{
    if (idx < 0 || idx >= run->n_party) return false;
    if (option == 1) return true;   /* selectPokemonForOption(onPokemonSelected): no filter */
    /* not fainted, allowed in the challenge, and the option's requirements: a dance move among its moves */
    return option == 2 && gc_dance_has_move(&run->party[idx]);
}

/* the second list of the third option: the move slots of the member that are dance moves */
bool gc_dance_move_ok(const gc_run *run, int member, int slot)
{
    if (member < 0 || member >= run->n_party || slot < 0 || slot >= run->party[member].b.n_moves) return false;
    return run->party[member].b.moves[slot] > 0 && gc_is_dance_move(run->party[member].b.moves[slot]);
}

/* b = the party member (a storage index), c = the move slot of the third option */
bool gc_dance_execute(gc_run *run, int option, int b, int c)
{
    static dance_t d;
    if (option != 0 && !gc_dance_pick_ok(run, option, b)) return false;
    if (option == 2 && !gc_dance_move_ok(run, b, c)) return false;
    if (!dance_init(run, &d)) {
        /* a species or biome run-core has no rolls for: nobody dances, the encounter ends as if declined */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        gc_me_leave_with_heal_shop(run);
        return true;
    }
    if (option == 0) { fight(run, &d); return true; }
    if (option == 1) {
        /* onPreOptionPhase: LearnMovePhase for the picked Pokemon; then the exit tween and leaveEncounterWithoutBattle(true) */
        gc_rq_unshift(run, gc_rp(RP_LEARN_MOVE, b, MOVE_ID_REVELATION_DANCE, 0));
        gc_me_leave_with_heal_shop(run);
        return true;
    }
    /* the Oricorio is made again from its data and gets the move the player showed */
    const int move = run->party[b].b.moves[c];
    gc_mon *o = &d.mon;
    bool knows = false;
    for (int k = 0; k < o->b.n_moves; k++) knows = knows || o->b.moves[k] == move;
    if (!knows) {
        if (o->b.n_moves < 4) gc_mon_set_move(o, o->b.n_moves, move);
        else gc_mon_set_move(o, 3, move);
    }
    catch_it(run, o);
    gc_me_leave_with_heal_shop(run);
    return true;
}
