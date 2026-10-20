/* Fun and Games! (src/data/mystery-encounters/encounters/fun-and-games-encounter.ts), the second option and the minigame of
 * the first. Not a battle that is won: the player has three turns against a Wobbuffet (the player's level, 0 IVs, a Mild
 * nature, never shiny) that does not move, and how little of its HP is left at the end picks the prize. A KO loses the game.
 *
 * What the option does, in upstream's order. MysteryEncounterOptionSelectedPhase runs onOptionPhase inside
 * executeWithSeedOffset(.., getSeedOffset() * 500), but the function is async: only what comes before its first await runs
 * there (turnsRemaining = 3, updatePlayerMoney(-cost, true, false), the paid message), so everything after the await runs on
 * the run's own RNG: summonPlayerPokemon swaps the chosen Pokemon into the lead, makes the Wobbuffet with
 * addEnemyPokemon(Wobbuffet, level, NONE, false, true) (the Pokemon constructor's rolls, its generated moveset, no shiny),
 * sets its IVs to 0 and its nature to Mild, calculateStats, setPokemonSeen, and the player's PostSummonPhase is queued. The
 * queue then runs dry, which starts TurnInitPhase (see gc_run_until_screen).
 *
 * TurnInitPhase asks handleNextTurn (encounter.onTurnStart) before it queues the commands: with turns left it queues the
 * turn messages, counts one down and goes on; with none left it picks the prize by the Wobbuffet's HP ratio, ends the
 * encounter (leaveEncounterWithoutBattle) and skips the rest of the phase. EnemyCommandPhase still asks the AI for the
 * Wobbuffet's move (getNextMove draws on the battle's RNG) but marks the command skipped (withSkipEnemyBattleTurns); the
 * player's CommandPhase goes straight to the FIGHT menu (withSkipToFightInput) and fleeing is not allowed.
 *
 * If the Wobbuffet faints, VictoryPhase gives the experience and handleMysteryEncounterVictory pushes BattleEndPhase(true) and
 * the MysteryEncounterRewardsPhase, which finds doContinueEncounter (handleLoseMinigame) set: the Wobbuffet is gone, the
 * encounter leaves with the heal-only shop and the revive cost (1.5 x the wave's money) comes off.
 *
 * The moveset is the one thing upstream rolls that game-core has no general port of: generateMoveset is ported here for the
 * wild, non-boss Wobbuffet alone (gc_fg_wobbuffet_moveset), because the AI's choice among those moves draws on the battle's
 * RNG and so moves the player's damage rolls. */
#include "internal.h"
#include <math.h>
#include <string.h>

#define FG_NS "mysteryEncounters/funAndGames"

bool gc_fg_active(const gc_run *run) { return gc_int((gc_run *)run)->fg.active; }

/* the party screen's selectableFilter: isPokemonValidForEncounterOptionSelection, any Pokemon allowed in battle */
bool gc_fg_pick_ok(const gc_run *run, int idx)
{
    return idx >= 0 && idx < run->n_party && gc_mon_allowed(&run->party[idx]);
}

/* ---- Wobbuffet's moveset ---- */

/* getLevelMoves({startingLevel: 1, includeEvolutionMoves, includePrevolutionMoves}) of Wobbuffet, in the order the learnset
 * lists them (traced from the pinned upstream: Wynaut adds nothing). Counter and Mirror Coat are its FORCED_SIGNATURE_MOVES. */
static const struct { uint16_t move; uint8_t level; } wobbuffet_learnset[] = {
    {MOVE_ID_AMNESIA, 1}, {MOVE_ID_SPLASH, 1}, {MOVE_ID_CHARM, 1}, {MOVE_ID_ENCORE, 1},
    {MOVE_ID_COUNTER, 15}, {MOVE_ID_DESTINY_BOND, 15}, {MOVE_ID_SAFEGUARD, 15}, {MOVE_ID_MIRROR_COAT, 15},
};
#define N_WOBBUFFET_LEARNSET ((int)(sizeof wobbuffet_learnset / sizeof wobbuffet_learnset[0]))

/* generateMoveset(pokemon) for a wild Pokemon that is neither a boss nor a trainer's, as the 8555c08 source runs for Wobbuffet:
 *  1. the level pool: each move learned by the level, weight = learn level + 20 (BASE_LEVEL_WEIGHT_OFFSET); no TM or egg moves
 *     (only trainers' Pokemon get them) and no superseded filter;
 *  2. filterMovePool takes out nothing for a wild Pokemon but unimplemented moves;
 *  3. adjustDamageMoveWeights: Counter and Mirror Coat have no power (power -1 is an effective power of 0), so they are
 *     0 / 40 clamped to a quarter of their weight, and the one of them in the category of the lower of Attack and Special Attack
 *     (the constructor's stats, before the IVs and nature are changed) is scaled by min((lower / higher)^3 * 2, 1);
 *  4. every weight becomes ceil(weight^1.6 * 100);
 *  5. forceSignatureMove: when Counter or Mirror Coat is in the pool a 60% coin flip (randSeedInt(100) < 60) adds one, picked
 *     with randSeedItem among those in the pool (nothing is drawn for one, randSeedInt(2) for both);
 *  6. forceStabMove: only when no Psychic damaging signature was added, and it never finds a move: the only damaging moves
 *     are Counter and Mirror Coat, which are in STAB_BLACKLIST;
 *  7. fillInRemainingMovesetSlots: while fewer than four moves, randSeedInt(total weight) walks the remaining moves in pool
 *     order (rand > weight, not >=) and the pick leaves the pool;
 *  8. filterUselessMoves removes none of these moves (no weather, no sleep or Aurora Veil conditions, no single-stat boost
 *     without a use: Amnesia raises Special Defense, which is not Attack or Special Attack). */
bool gc_fg_wobbuffet_moveset(gc_run *run, gc_mon *m)
{
    /* the constructor's stats (the same copy gc_enemy_moveset takes): before enemy items and summon effects */
    static pr_battle constructor;
    memset(&constructor, 0, sizeof constructor);
    constructor.game_mode = run->run.mode;
    constructor.wave_index = run->wave;
    pr_pokemon p = m->b;
    pr_update_passive(&constructor, &p);
    pr_calculate_stats(&constructor, &p);
    const double attack = p.stats[STAT_ATK], special_attack = p.stats[STAT_SPATK];
    if (attack < 1 || special_attack < 1) return false;

    struct { int move; double weight; bool in_pool; } pool[N_WOBBUFFET_LEARNSET];
    int n = 0;
    for (int i = 0; i < N_WOBBUFFET_LEARNSET; i++) {
        if (m->b.level < wobbuffet_learnset[i].level) break;
        const int move = wobbuffet_learnset[i].move;
        if (pr_moves[move].unimplemented) continue;
        pool[n].move = move;
        pool[n].weight = wobbuffet_learnset[i].level + 20;
        pool[n].in_pool = true;
        n++;
    }

    const double lower = fmin(attack, special_attack), higher = fmax(attack, special_attack);
    const int worse_category = attack > special_attack ? MOVE_CATEGORY_SPECIAL : MOVE_CATEGORY_PHYSICAL;
    const double adjustment = fmin(js_pow(lower / higher, 3) * 2, 1);
    for (int i = 0; i < n; i++) {
        const int category = pr_moves[pool[i].move].category;
        if (category == MOVE_CATEGORY_STATUS) continue;
        pool[i].weight *= 0.25;
        if (category == worse_category) pool[i].weight *= adjustment;
    }
    for (int i = 0; i < n; i++) pool[i].weight = ceil(js_pow(pool[i].weight, 1.6) * 100);

    int moves[4], n_moves = 0;
    int signature[2], n_signature = 0;
    for (int i = 0; i < n; i++)
        if (pool[i].move == MOVE_ID_COUNTER || pool[i].move == MOVE_ID_MIRROR_COAT) signature[n_signature++] = i;
    if (n_signature > 0 && pr_rng_seed_int(&run->run.rng, 100, 0) < 60) {
        const int pick = n_signature == 1 ? signature[0] : signature[pr_rng_seed_int(&run->run.rng, n_signature, 0)];
        pool[pick].in_pool = false;
        moves[n_moves++] = pool[pick].move;
    }

    while (n_moves < 4) {
        double total = 0;
        int remaining = 0;
        for (int i = 0; i < n; i++) if (pool[i].in_pool) { total += pool[i].weight; remaining++; }
        if (!remaining) break;
        double rand = pr_rng_seed_int(&run->run.rng, (int32_t)total, 0);
        int index = -1;
        for (int i = 0; i < n; i++) {
            if (!pool[i].in_pool) continue;
            index = i;
            if (!(rand > pool[i].weight)) break;
            rand -= pool[i].weight;
        }
        pool[index].in_pool = false;
        moves[n_moves++] = pool[index].move;
    }

    memset(m->b.moves, 0, sizeof m->b.moves);
    memset(m->b.pp_used, 0, sizeof m->b.pp_used);
    memset(m->b.pp_up, 0, sizeof m->b.pp_up);
    m->b.n_moves = 0;
    for (int i = 0; i < 4; i++) m->b.max_pp_override[i] = -1;
    for (int i = 0; i < n_moves; i++) gc_mon_set_move(m, i, moves[i]);
    return true;
}

/* ---- the option ---- */

static double revive_cost(const gc_run *run) { return pr_run_get_wave_money_amount(run->wave, 1.5); }

/* updatePlayerMoney(-amount, true, false): the wallet is clamped to 0..Number.MAX_SAFE_INTEGER */
static void pay(gc_run *run, double cost)
{
    run->money = fmin(fmax(run->money - cost, 0.0), 9007199254740991.0);
}

/* the Wobbuffet: addEnemyPokemon(Wobbuffet, level, NONE, false, true), then ivs.fill(0), setNature(MILD), calculateStats,
 * setPokemonSeen. false = a species run-core could not make */
static bool make_wobbuffet(gc_run *run, int level)
{
    pr_enemy_gen_ctx ec;
    gc_enemy_ctx(run, &ec);
    ec.species = SPECIES_ID_WOBBUFFET;
    ec.level = level;
    ec.trainer_slot = 0;
    ec.boss = false;
    ec.shiny_lock = true;
    pr_enemy_gen g;
    memset(&g, 0, sizeof g);
    if (pr_run_enemy_pokemon_base(&run->run, &ec, &g) < 0) return false;
    gc_mon *w = &run->enemy[0];
    gc_mon_new_enemy(run, w, SPECIES_ID_WOBBUFFET, level, &g, 0);
    if (!gc_fg_wobbuffet_moveset(run, w)) return false;
    if (pr_run_enemy_pokemon_finish(&run->run, &ec, &g) < 0) return false;
    gc_refresh_enemy(run, w, SPECIES_ID_WOBBUFFET, level, &g, 0);
    run->n_enemy = 1;
    for (int i = 0; i < 6; i++) w->b.ivs[i] = 0;
    w->b.nature = NATURE_MILD;
    pr_update_passive(&run->bt, &w->b);
    pr_calculate_stats(&run->bt, &w->b);
    w->b.hp = w->b.stats[0];
    if (run->sys) gc_system_set_pokemon_seen(run->sys, w, true, false);
    return true;
}

void gc_fg_start(gc_run *run, int pick)
{
    gc_internal *in = gc_int(run);
    /* the party screen would not let a Pokemon the filter refuses be picked (the app has none for encounters yet): a
     * refused pick is taken as the first that passes */
    int mon = gc_fg_pick_ok(run, pick) ? pick : -1;
    for (int i = 0; mon < 0 && i < run->n_party; i++) if (gc_fg_pick_ok(run, i)) mon = i;

    /* onOptionPhase, up to its first await */
    const double cost = revive_cost(run);
    pay(run, cost);
    gc_ev_msg(run, "mysteryEncounterMessages:paidMoney", 1, pr_arg_int("amount", (int32_t)cost));

    /* summonPlayerPokemon: the chosen Pokemon and the lead swap places in the party */
    const int slot = gc_battle_slot_of(run, mon);
    if (slot > 0) {
        const int8_t t = run->party_order[0];
        run->party_order[0] = run->party_order[slot];
        run->party_order[slot] = t;
    }
    if (!make_wobbuffet(run, run->party[mon].b.level)) {
        /* a species run-core has no rolls for: nothing to play against, the encounter ends as if declined */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        run->n_enemy = 0;
        gc_me_leave_with_heal_shop(run);
        return;
    }
    gc_ai_enemy_party(run);

    run->me_battle = true;
    run->me_catch_allowed = false;
    run->mystery_type = run->mystery_tier = -1;
    run->in_battle = true;
    run->battle_type = BATTLE_TYPE_WILD;
    /* no CheckSwitchPhase: the player's Pokemon is sent out by the encounter, not the wave's start */
    const bool ask_switch = run->ask_switch;
    run->ask_switch = false;
    gc_battle_setup_field(run, true);
    run->ask_switch = ask_switch;

    /* the encounter's one save is the one at its start */
    in->at_save_point = false;
    in->fg.active = true;
    in->fg.turns_remaining = 3;
    gc_rq_push(run, gc_rp(RP_TURN_INIT, 0, 0, 0));
}

/* ---- the turns ---- */

/* the Wobbuffet leaves the field and the enemy party is emptied (wobbuffet.leaveField(), enemyParty = []) */
static void remove_wobbuffet(gc_run *run)
{
    if (run->enemy[0].b.on_field) run->enemy[0].b.on_field = false;
    run->bt.field[2] = run->bt.field[3] = NULL;
    run->bt.n_party[1] = 0;
    run->n_enemy = 0;
}

static pr_msg_arg wobbuffet_name(gc_run *run)
{
    return pr_arg_pokemon_name(&run->bt, "wobbuffetName", &run->enemy[0].b);
}

/* handleLoseMinigame, run by MysteryEncounterRewardsPhase (RP_FG_KO) after the Wobbuffet fainted or left the field: the
 * encounter leaves with the heal-only shop, and reviving the Wobbuffet costs 1.5 x the wave's money */
void gc_fg_lose(gc_run *run)
{
    gc_internal *in = gc_int(run);
    gc_ev_msg(run, FG_NS ":ko", 1, wobbuffet_name(run));
    in->fg.active = false;
    remove_wobbuffet(run);
    gc_rq_clear(run);
    pay(run, revive_cost(run));
    gc_me_leave_with_heal_shop(run);
}

/* handleMysteryEncounterVictory once the minigame's battle is over (a VictoryPhase for the Wobbuffet, or it left the field):
 * BattleEndPhase(victory) and the rewards phase, which takes handleLoseMinigame */
void gc_fg_battle_end(gc_run *run, bool victory)
{
    gc_rq_push(run, gc_rp(RP_BATTLE_END, victory, 0, 0));
    gc_rq_push(run, gc_rp(RP_FG_KO, 0, 0, 0));
}

/* handleNextTurn, from TurnInitPhase: true = skip the rest of the phase (no commands are queued) */
bool gc_fg_turn_init(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->fg.active) return false;
    pr_pokemon *wobbuffet = run->bt.field[2];
    if (!wobbuffet || !wobbuffet->on_field || wobbuffet->hp <= 0) {
        /* "should never be triggered": the Wobbuffet is gone */
        gc_fg_lose(run);
        return true;
    }
    if (in->fg.turns_remaining <= 0) {
        /* the Wobbuffet's HP ratio picks the prize */
        const double ratio = (double)wobbuffet->hp / wobbuffet->stats[0];
        const char *item = NULL, *result;
        if (ratio < 0.03) { item = "MULTI_LENS"; result = FG_NS ":bestResult"; }
        else if (ratio < 0.15) { item = "SCOPE_LENS"; result = FG_NS ":greatResult"; }
        else if (ratio < 0.33) { item = "WIDE_LENS"; result = FG_NS ":goodResult"; }
        else result = FG_NS ":badResult";
        const pr_msg_arg name = wobbuffet_name(run);
        gc_ev_msg(run, FG_NS ":endGame", 1, name);
        gc_ev_msg(run, result, 1, name);
        in->fg.active = false;
        remove_wobbuffet(run);
        gc_rq_clear(run);
        if (item) gc_me_leave_with_shop(run, item);
        else gc_me_leave_with_heal_shop(run);
        return true;
    }
    if (in->fg.turns_remaining < 3) gc_ev_msg(run, FG_NS ":chargingContinue", 0);
    static const char *const turn_keys[4] = {NULL, FG_NS ":turnRemaining1", FG_NS ":turnRemaining2", FG_NS ":turnRemaining3"};
    gc_ev_msg(run, turn_keys[in->fg.turns_remaining], 0);
    in->fg.turns_remaining--;
    return false;
}
