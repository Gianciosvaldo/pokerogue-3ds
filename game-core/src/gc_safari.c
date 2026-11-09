/* The Safari Zone (src/data/mystery-encounters/encounters/safari-zone-encounter.ts at pinned upstream 8555c08).
 *
 * The encounter (a Great one, waves 10-180, in the Forest, Swamp and Jungle) has two options. Entering costs the wave's money amount times 2 (MoneyRequirement(0, 2), on the
 * encounter too) and starts a minigame of three Pokemon; leaving is leaveEncounterWithoutBattle(true), the heal shop. onInit rolls nothing: it only sets the {{numEncounters}}
 * token.
 *
 * Entering (onOptionPhase, inside executeWithSeedOffset(.., getSeedOffset() * 500) up to its first await, which takes everything after it out of the offset: the run's RNG):
 * continuousEncounter, three Pokemon remaining, the entry fee ("You paid"), then summonSafariPokemon(). Each Pokemon:
 *   - the message safari.remainingCount with the count BEFORE it goes down;
 *   - in executeWithSeedOffset(.., wave * 1000 * remaining), so the same Pokemon whatever the player does: the level of the wave (getLevelForWave), then
 *     getRandomEncounterPokemon: a species from getRandomSpeciesByStarterCost([0, 5], NON_LEGEND_PARADOX_POKEMON, no types, no sub-legendary, legendary or mythical), the
 *     EnemyPokemon constructor's rolls, one more shiny roll (trySetShinySeed(64, true, 0): the Shiny Charm raises it) and one more hidden ability roll
 *     (tryRerollHiddenAbilitySeed(256): the Ability Charm raises it). With no timed event active there is no event species, so no roll for one;
 *   - it is marked seen, the catch and flee stages go back to 0, one less remains.
 * The player then picks one of four actions (initSubsequentOptionSelect), each an option phase that starts inside executeWithSeedOffset but reaches all its rolls after an await
 * (a throw's animation), so every roll below is on the run's RNG:
 *   - Safari Ball: catchRate = round(species.catchRate * 1.5 * (2 + clamp(catchStage, 0, 6)) / (2 - clamp(catchStage, -6, 0))); the ball twitches round(1048560 /
 *     sqrt(sqrt(16711680 / catchRate))) / 65536 of the time, and it takes three twitches in a row (randSeedInt(65536) < rate, each drawn only if the one before held). It
 *     breaks free (battle:pokemonBrokeFree) and the turn ends (cursor on the ball) or it is caught: catchPokemon says battle:pokemonCaught, marks the species caught, adds it to
 *     the party (or asks what to release) and queues a VictoryPhase that is "exp only" and finds no participant in a wave without a battle: no experience and no friendship.
 *     Then the next Pokemon (cursor on the ball) or, with none left, continuousEncounter off and the heal shop;
 *   - Bait: catch stage +2 always; flee stage +1 when randSeedInt(10) < 8, which says safari.eating, else safari.busyEating; the turn ends (cursor on the bait);
 *   - Mud: flee stage -2 always; catch stage -1 when randSeedInt(10) < 8, which says safari.angry, else safari.besideItselfAngry; the turn ends (cursor on the mud);
 *   - Run: battle:playerFled, then the next Pokemon with the cursor on Run, or the way out.
 * Ending the turn: the Pokemon flees when randSeedInt(256) < ((255^2 - catchRate^2) / 255 / 2) * fleeModifier (the species' catch rate, the same clamp on the flee stage):
 * battle:pokemonFled, then the next Pokemon (cursor where it was) or the way out; else safari.watching and the four actions again, the cursor where it was.
 *
 * The screen of the minigame is a MYSTERY screen with me_stage GC_ME_STAGE_SAFARI (options 0..3 as above, `slot` the cursor, enemy[0] the Pokemon); a catch queues RP_CATCH
 * (c = 3) and RP_SAFARI, which goes on from there once the player has answered the full-party prompt.
 *
 * Not ported: the Pokemon's and the trainer's animations and sounds, the IV Scanner's ScanIvsPhase, the VictoryPhase's pokemonDefeated count (this port keeps none), the Limited
 * Catch challenge's refusal of the catch. The minigame is not saved: the encounter's only save is its start. */
#include "internal.h"
#include <math.h>
#include <string.h>

#define NS "mysteryEncounters/safariZone"
#define NUM_SAFARI_ENCOUNTERS 3
#define SAFARI_MONEY_MULTIPLIER 2.0

enum { ACT_BALL, ACT_BAIT, ACT_MUD, ACT_RUN };

/* NON_LEGEND_PARADOX_POKEMON (src/data/balance/special-species-groups.ts) */
static const uint16_t paradox[] = {
    SPECIES_ID_GREAT_TUSK, SPECIES_ID_SCREAM_TAIL, SPECIES_ID_BRUTE_BONNET, SPECIES_ID_FLUTTER_MANE, SPECIES_ID_SLITHER_WING,
    SPECIES_ID_SANDY_SHOCKS, SPECIES_ID_ROARING_MOON, SPECIES_ID_WALKING_WAKE, SPECIES_ID_GOUGING_FIRE, SPECIES_ID_RAGING_BOLT,
    SPECIES_ID_IRON_TREADS, SPECIES_ID_IRON_BUNDLE, SPECIES_ID_IRON_HANDS, SPECIES_ID_IRON_JUGULIS, SPECIES_ID_IRON_MOTH,
    SPECIES_ID_IRON_THORNS, SPECIES_ID_IRON_VALIANT, SPECIES_ID_IRON_LEAVES, SPECIES_ID_IRON_BOULDER, SPECIES_ID_IRON_CROWN,
};

double gc_safari_cost(const gc_run *run) { return pr_run_get_wave_money_amount(run->wave, SAFARI_MONEY_MULTIPLIER); }

/* updatePlayerMoney(-amount): the wallet is clamped to 0..Number.MAX_SAFE_INTEGER, and the "paid" message is queued */
static void pay(gc_run *run, double cost)
{
    run->money = fmin(fmax(run->money - cost, 0.0), 9007199254740991.0);
    gc_ev_msg(run, "mysteryEncounterMessages:paidMoney", 1, pr_arg_int("amount", (int32_t)cost));
}

/* randSeedInt(max) on the run's RNG, noted for the differential (the rolls of the step: ball twitches, bait and mud chances, the flee roll) */
static int roll(gc_run *run, int max)
{
    gc_internal *in = gc_int(run);
    const int v = pr_rng_seed_int(&run->run.rng, max, 0);
    if (in->safari.n_trace < 12) {
        in->safari.trace_max[in->safari.n_trace] = max;
        in->safari.trace_val[in->safari.n_trace++] = v;
    }
    return v;
}

/* (2 + clamp(stage, 0, 6)) / (2 - clamp(stage, -6, 0)): the catch and the flee stages count like stat stages */
static double stage_modifier(int stage)
{
    const int up = stage < 0 ? 0 : stage > 6 ? 6 : stage;
    const int down = stage > 0 ? 0 : stage < -6 ? -6 : stage;
    return (double)(2 + up) / (double)(2 - down);
}

/* species.catchRate */
static double species_catch_rate(const gc_mon *e)
{
    const pr_form *f = gc_form(e->b.species, 0);
    return f ? f->catch_rate : 255;
}

/* the four actions: initSubsequentOptionSelect({overrideOptions, startingCursorIndex, hideDescription}) */
static void show_actions(gc_run *run, int cursor)
{
    const gc_internal *in = gc_int(run);
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_MYSTERY;
    s->mystery_type = GC_ME_SAFARI_ZONE;
    s->mystery_tier = GC_ME_GREAT;
    s->me_stage = GC_ME_STAGE_SAFARI;
    s->n_options = 4;
    for (int i = 0; i < 4; i++) s->option_ok[i] = true;
    for (int i = 0; i < 3; i++) s->mc_trainer[i] = -1;
    s->slot = (int8_t)cursor;
    s->me_args[0] = gc_m_mon(run, "pokemonName", 1, 0);
    s->me_args[1] = pr_arg_int("remainingCount", in->safari.token_remaining);
    s->n_me_args = 2;
}

/* the way out: continuousEncounter off, leaveEncounterWithoutBattle(true) */
static void leave(gc_run *run)
{
    gc_int(run)->safari.active = false;
    run->n_enemy = 0;
    gc_me_leave_with_heal_shop(run);
}

/* summonSafariPokemon(): false if the species is one run-core has no rolls for */
static bool summon(gc_run *run)
{
    gc_internal *in = gc_int(run);
    in->safari.token_remaining = in->safari.remaining;
    gc_ev_msg(run, NS ":safari.remainingCount", 1, pr_arg_int("remainingCount", in->safari.remaining));

    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave * 1000 * in->safari.remaining, NULL);
    const int level = pr_run_get_level_for_wave(&run->run, &r, run->wave);
    const int species = gc_species_by_starter_cost(&r, 0, 5, paradox, (int)(sizeof paradox / sizeof paradox[0]), NULL, 0, false, false, false);
    gc_mon *e = &run->enemy[0];
    if (!gc_me_new_enemy(run, &r, species, level, false, false, e)) return false;
    pr_calculate_stats(&run->bt, &e->b);
    e->b.hp = e->b.stats[STAT_HP];
    gc_try_set_shiny_seed(run, &r, e, 64, 0);
    gc_try_reroll_hidden_ability(run, &r, e, 256);

    if (run->sys) gc_system_set_pokemon_seen(run->sys, e, true, false);
    in->safari.catch_stage = in->safari.flee_stage = 0;
    in->safari.remaining--;
    run->n_enemy = 1;
    return true;
}

/* the Pokemon after a catch or a flight, or the way out: true when it left (the phase is done), false when the four actions wait */
bool gc_safari_next(gc_run *run, int cursor)
{
    gc_internal *in = gc_int(run);
    if (in->safari.remaining <= 0) {
        leave(run);
        return true;
    }
    if (!summon(run)) {
        /* a species run-core has no rolls for: nobody to catch, the encounter ends as if declined */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        leave(run);
        return true;
    }
    show_actions(run, cursor);
    return false;
}

/* doEndTurn(cursorIndex) */
static bool end_turn(gc_run *run, int cursor)
{
    gc_internal *in = gc_int(run);
    const double rate = species_catch_rate(&run->enemy[0]);
    const double flee_rate = ((255.0 * 255.0 - rate * rate) / 255.0 / 2.0) * stage_modifier(in->safari.flee_stage);
    const int flee_roll = roll(run, 256);
    if (flee_roll < flee_rate) {
        gc_ev_msg(run, "battle:pokemonFled", 1, gc_m_mon_name(run, "pokemonName", 1, 0));
        return gc_safari_next(run, cursor);
    }
    gc_ev_msg(run, NS ":safari.watching", 1, gc_m_mon(run, "pokemonName", 1, 0));
    show_actions(run, cursor);
    return false;
}

/* tryChangeCatchStage / tryChangeFleeStage: with a chance (out of 10) the change fails when randSeedInt(10) >= chance */
static bool change_stage(gc_run *run, int8_t *stage, int change, int chance)
{
    if (chance && roll(run, 10) >= chance) return false;
    const int v = *stage + change;
    *stage = (int8_t)(v < -6 ? -6 : v > 6 ? 6 : v);
    return true;
}

static bool throw_ball(gc_run *run)
{
    gc_internal *in = gc_int(run);
    const double catch_rate = js_round(species_catch_rate(&run->enemy[0]) * 1.5 * stage_modifier(in->safari.catch_stage));
    const int twitch = (int)js_round(1048560.0 / sqrt(sqrt(16711680.0 / catch_rate)));
    in->safari.last_twitch = twitch;
    bool caught = true;
    for (int shake = 0; shake < 3 && caught; shake++) caught = roll(run, 65536) < twitch;
    if (!caught) {
        gc_ev_msg(run, "battle:pokemonBrokeFree", 1, gc_m_mon_name(run, "pokemonName", 1, 0));
        return end_turn(run, ACT_BALL);
    }
    /* catchPokemon, then (after the catch's prompt, if any) the next Pokemon or the way out */
    in->thrown_ball = POKEBALL_TYPE_POKEBALL;
    gc_rq_unshift(run, gc_rp(RP_CATCH, 0, 0, 3));
    gc_rq_unshift(run, gc_rp(RP_SAFARI, ACT_BALL, 0, 0));
    return true;
}

static bool throw_bait(gc_run *run)
{
    gc_internal *in = gc_int(run);
    change_stage(run, &in->safari.catch_stage, 2, 0);
    const bool more = change_stage(run, &in->safari.flee_stage, 1, 8);
    gc_ev_msg(run, more ? NS ":safari.eating" : NS ":safari.busyEating", 1, gc_m_mon(run, "pokemonName", 1, 0));
    return end_turn(run, ACT_BAIT);
}

static bool throw_mud(gc_run *run)
{
    gc_internal *in = gc_int(run);
    change_stage(run, &in->safari.flee_stage, -2, 0);
    const bool angrier = change_stage(run, &in->safari.catch_stage, -1, 8);
    gc_ev_msg(run, angrier ? NS ":safari.angry" : NS ":safari.besideItselfAngry", 1, gc_m_mon(run, "pokemonName", 1, 0));
    return end_turn(run, ACT_MUD);
}

static bool run_away(gc_run *run)
{
    gc_ev_msg(run, "battle:playerFled", 1, gc_m_mon_name(run, "pokemonName", 1, 0));
    return gc_safari_next(run, ACT_RUN);
}

/* option 1 of the encounter: continuousEncounter, three Pokemon, the fee, the first Pokemon */
static bool enter(gc_run *run)
{
    gc_internal *in = gc_int(run);
    in->safari.active = true;
    in->safari.remaining = NUM_SAFARI_ENCOUNTERS;
    pay(run, gc_safari_cost(run));
    run->n_enemy = 0;
    return gc_safari_next(run, 0);
}

bool gc_safari_execute(gc_run *run, int option)
{
    gc_int(run)->safari.n_trace = 0;
    gc_int(run)->safari.last_twitch = -1;
    if (run->screen.me_stage == GC_ME_STAGE_SAFARI) {
        switch (option) {
        case ACT_BALL: return throw_ball(run);
        case ACT_BAIT: return throw_bait(run);
        case ACT_MUD: return throw_mud(run);
        default: return run_away(run);
        }
    }
    if (option == 0) return enter(run);
    leave(run);
    return true;
}
