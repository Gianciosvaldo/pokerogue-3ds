/* Catching in the run. Two halves of upstream's catch:
 *  - what CommandPhase decides before a ball is thrown (checkCanUseBall, handleBallCommand's boss rule);
 *  - what AttemptCapturePhase.catch() does after the shake rolls: the dex, the "added as a starter" message,
 *    the full-party prompt, addToParty, the enemy's held items, removePokemon. The rolls themselves, the
 *    "Gotcha!" text and the enemy leaving the field are battle-core's (phases.c attempt_capture_phase), which
 *    then stops at its VictoryPhase; after_stop (gc_phases.c) queues RP_CATCH in front of the run's
 *    VictoryPhase, as upstream's catch() unshifts the VictoryPhase only when it is done.
 * Not ported, and said so in the README: the achievements catch() validates, Pokemon.addToParty's form change
 * trigger, and the Limited Catch challenge (the one rule applyChallenges(POKEMON_ADD_TO_PARTY) has). */
#include "internal.h"

static bool classic_mode(int mode) { return mode == GAME_MODES_CLASSIC || mode == GAME_MODES_CHALLENGE; }
static bool endless_mode(int mode) { return mode == GAME_MODES_ENDLESS || mode == GAME_MODES_SPLICED_ENDLESS; }

/* gameData.getStarterCount(d => !!d.caughtAttr) < getAllStarters().length - 1 */
static bool missing_multiple_starters(const gc_run *run)
{
    int caught = 0;
    for (int i = 0; i < gc_starter_cost_count; i++) {
        const uint16_t id = gc_starter_costs[i].id;
        if (run->sys && id <= GC_MAX_SPECIES_ID && run->sys->dex[id].caught_attr) caught++;
    }
    return caught < gc_starter_cost_count - 1;
}

/* the boss rule's Wonder Guard exemption: hasAbility(WONDER_GUARD, false, true), the ability the species gives
 * (a Transform override ignored) or the passive */
static bool has_wonder_guard(const pr_pokemon *p)
{
    return p->base_ability == ABILITY_ID_WONDER_GUARD || p->passive == ABILITY_ID_WONDER_GUARD;
}

/* CommandPhase.checkCanUseBall() and, for a ball type below the five the command takes, handleBallCommand's
 * rule for a boss that still has a shield. NULL = the throw is allowed; else the locale key of the text upstream
 * shows instead (battle:noPokeball*). Doubles are played as singles here, so "more than one Pokemon on the
 * field" (noPokeballMulti) can't come up. */
const char *gc_catch_ball_refusal(const gc_run *run, int ball)
{
    const pr_run *rr = &run->run;
    const int mode = rr->mode, wave = run->wave;
    const bool wild = run->battle_type == BATTLE_TYPE_WILD && !run->me_battle;
    const bool classic = classic_mode(mode), endless = endless_mode(mode), daily = mode == GAME_MODES_DAILY;
    const bool final_boss = pr_run_is_battle_classic_final_boss(rr, wave);
    const bool minor_boss = pr_run_is_endless_minor_boss(rr, wave);
    /* isFullFreshStartChallenge(): the Fresh Start challenge with its value 1 */
    const bool full_fresh = mode == GAME_MODES_CHALLENGE && (rr->challenge_flags & PR_CHALLENGE_FRESH_START)
                            && rr->challenge_fresh_start == 1;
    /* isDailyFinalBoss() && getDailyEventSeedBoss()?.catchable: an event seed's boss is a timed event's, not ported */
    const bool catchable_daily_boss = false;
    int active=0;const pr_pokemon *foe=NULL;
    for(int k=2;k<4;k++)if(run->bt.field[k]&&run->bt.field[k]->on_field&&run->bt.field[k]->hp>0&&!run->bt.field[k]->switch_out_status){foe=run->bt.field[k];active++;}

    if (run->biome == BIOME_ID_END && wild) {
        bool uncaught_on_field = false;
        if (foe && foe->hp > 0)
            uncaught_on_field = !run->sys || foe->species > GC_MAX_SPECIES_ID || !run->sys->dex[foe->species].caught_attr;
        if ((classic && !final_boss && uncaught_on_field) || (full_fresh && !final_boss) || (endless && !minor_boss))
            return "battle:noPokeballForce";
        if ((classic && final_boss && missing_multiple_starters(run)) || (full_fresh && final_boss)
            || (endless && minor_boss) || (daily && !catchable_daily_boss))
            return "battle:noPokeballForceFinalBoss";
    } else if (run->battle_type == BATTLE_TYPE_TRAINER) {
        return "battle:noPokeballTrainer";
    } else if (run->me_battle && !run->me_catch_allowed) {
        return "battle:noPokeballMysteryEncounter";
    }

    if(active>1)return "battle:noPokeballMulti";
    /* targetPokemon = getEnemyPokemon(false): an active one (not fainted) that isn't switching out */
    if (ball >= 0 && ball < 5 && foe && foe->hp > 0 && !foe->switch_out_status && foe->is_boss
        && foe->boss_segment_index >= 1 && !has_wonder_guard(foe)) {
        /* hasAnyChallenges(): a Challenge Mode run lists every challenge, set or not */
        const bool challenge_active = mode == GAME_MODES_CHALLENGE || rr->has_challenges;
        if (final_boss && (ball < POKEBALL_TYPE_MASTER_BALL || (ball == POKEBALL_TYPE_MASTER_BALL && challenge_active)))
            return "battle:noPokeballForceFinalBossCatchable";
        if (catchable_daily_boss || ball < POKEBALL_TYPE_MASTER_BALL) return "battle:noPokeballStrong";
    }
    return NULL;
}

/* An enemy of a wild battle that is still standing when battle-core stops at its VictoryPhase was caught:
 * a faint leaves hp at 0 */
bool gc_catch_is_catch(const gc_run *run, int enemy_idx)
{
    return enemy_idx >= 0 && run->battle_type == BATTLE_TYPE_WILD && run->enemy[enemy_idx].b.hp > 0;
}

/* ---- the Pokemon changing sides ---- */

/* AttemptCapturePhase.catch()'s removePokemon: hp 0, FAINT, the enemy's held items cleared. (The score
 * addFaintedEnemyScore adds is battle-core's, added with the roll.) */
static void remove_pokemon(gc_run *run, gc_mon *e)
{
    e->b.hp = 0;
    e->b.status = STATUS_EFFECT_FAINT;
    gc_items_drop_enemy_held(run);
}

/* Pokemon.addToParty(pokeball, slotIndex) and what catch() does with it: the Pokemon takes the ball and where it
 * was met, goes into the party (`released` = the storage index of the member just released, whose place it
 * takes; -1 = at the end), the enemy's held items become the player's, then removePokemon */
static void add_to_party(gc_run *run, int enemy_idx, int ball, int released)
{
    pr_battle *bt = &run->bt;
    gc_mon *e = &run->enemy[enemy_idx];
    e->pokeball = (uint8_t)ball;
    e->met_level = e->b.level;
    e->met_biome = (int8_t)run->biome;
    e->met_wave = (int16_t)run->wave;
    e->met_species = e->b.species;

    /* addPlayerPokemon(species, level, abilityIndex, formIndex, gender, shiny, variant, ivs, nature, dataSource):
     * the id, hp, stats, status, moveset and the rest come from the enemy as they are (no new stat roll); it is
     * a PlayerPokemon, so no boss (its shields and the boss passive stay behind); leaveField(true) then clears
     * its summon and turn data */
    gc_mon caught = *e;
    caught.b.is_player = true;
    caught.b.on_field = false;
    caught.b.switch_out_status = true;
    caught.b.is_boss = false;
    caught.b.boss_segments = caught.b.boss_segment_index = 0;
    pr_reset_summon_data(&caught.b);
    pr_reset_turn_data(&caught.b);
    pr_update_passive(bt, &caught.b);

    if (released < 0) {
        const int slot = run->n_party;
        run->party[slot] = caught;
        run->party_order[slot] = (int8_t)slot;
        run->n_party++;
    } else {
        /* the party screen's RELEASE mode removes the member, then the Pokemon is spliced in at the cursor, which
         * is the released place - except that releasing the last place leaves the cursor one before the new end,
         * so the new Pokemon goes in front of the member that was there */
        int pos = -1;
        for (int k = 0; k < run->n_party; k++) if (run->party_order[k] == released) pos = k;
        for(int field=0;field<2;field++)if(bt->field[field]==&run->party[released].b){
            if(bt->field[field]->on_field){gc_event *ev=gc_ev_new(run,GC_EV_RECALL);if(ev){ev->a=0;ev->b=released;ev->c=field;}}
            bt->field[field]=NULL;
        }
        run->party[released] = caught;
        if (pos == run->n_party - 1 && pos > 0) {
            const int8_t t = run->party_order[pos - 1];
            run->party_order[pos - 1] = run->party_order[pos];
            run->party_order[pos] = t;
        }
    }
    bt->n_party[0] = (uint8_t)run->n_party;
    for (int k = 0; k < run->n_party; k++) bt->party[0][k] = &run->party[run->party_order[k]].b;

    gc_items_take_enemy_held(run);
    remove_pokemon(run, e);
}

/* ---- RP_CATCH: a = the enemy's index, b = the ball ---- */

bool gc_catch_start(gc_run *run, gc_rphase *p)
{
    gc_internal *in = gc_int(run);
    gc_mon *e = &run->enemy[p->a];
    p->b = in->thrown_ball;

    if (run->sys) {
        /* catch(): updateSpeciesDexIvs(species.getRootSpeciesId(true), ivs), then (after the text) setPokemonCaught */
        gc_system_update_species_dex_ivs(run->sys, gc_species_root_for_starter(e->b.species), e->b.ivs);
    }
    /* encounter-pokemon-utils catchPokemon(.., isObtain) (c = 1: a Mystery Encounter's Pokemon, e.g. the Salesman's) shows
     * battle:pokemonObtained after the IVs and before the dex */
    if (p->c == 1) gc_ev_msg(run, "battle:pokemonObtained", 1, gc_m_mon_name(run, "pokemonName", 1, p->a));
    if (run->sys) {
        gc_caught_report rep;
        gc_system_set_pokemon_caught_ex(run->sys, e, true, false, true, run->run.mode == GAME_MODES_DAILY, &rep);
        for (int i = 0; i < rep.n_started; i++) {
            /* battle:addedAsAStarter names the species it unlocked: the caught one, then its prevolutions */
            pr_msg_arg name = {"pokemonName", PR_ARG_POKEMON_NAME, pr_msg_pokemon(rep.started[i], 0, 0), NULL};
            gc_ev_msg(run, "battle:addedAsAStarter", 1, name);
        }
    }

    if (run->n_party < 6) {
        add_to_party(run, p->a, p->b, -1);
        return true;
    }
    /* the party is full: "release one to make room?" (Summary and Pokedex only look). Yes opens the party screen
     * in RELEASE mode, No lets the Pokemon go. Here one screen answers both: pick the member to release, or skip */
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_SWITCH;
    s->slot = 0;
    s->party = (int8_t)p->a;
    for (int i = 0; i < run->n_party; i++) s->pick_ok[i] = true;
    s->can_skip = true;
    gc_ev_msg(run, "battle:partyFull", 1, gc_m_mon_name(run, "pokemonName", 1, p->a));
    /* the app's marker for this prompt (run_screen.c) */
    gc_ev_msg(run, "menu:swapOrRelease", 1, gc_m_mon_name(run, "pokemonName", 1, p->a));
    return false;
}

/* PartyUiHandler.getReleaseMessage: one of ten farewells (randInt, Math.random: not a seeded stream) */
static const char *release_key(gc_run *run)
{
    const uint32_t r = gc_stub_rand(run, 128);
    if (r < 20) return "partyUiHandler:goodbye";
    if (r < 40) return "partyUiHandler:byebye";
    if (r < 60) return "partyUiHandler:farewell";
    if (r < 80) return "partyUiHandler:soLong";
    if (r < 100) return "partyUiHandler:thisIsWhereWePart";
    if (r < 108) return "partyUiHandler:illMissYou";
    if (r < 116) return "partyUiHandler:illNeverForgetYou";
    if (r < 124) return "partyUiHandler:untilWeMeetAgain";
    if (r < 127) return "partyUiHandler:sayonara";
    return "partyUiHandler:smellYaLater";
}

bool gc_catch_choose(gc_run *run, gc_rphase *p, const gc_choice *c, const char **why, bool *ok)
{
    *ok = false;
    if (c->kind != GC_CH_PICK) { *why = "pick a Pokemon"; return false; }
    const gc_screen *s = &run->screen;
    gc_mon *e = &run->enemy[p->a];
    if (c->a == -1) {
        if (!s->can_skip) { *why = "partyUiHandler:noEnergy"; return false; }
        *ok = true;
        /* No: removePokemon(), then the VictoryPhase */
        remove_pokemon(run, e);
        return true;
    }
    if (c->a < 0 || c->a >= run->n_party || !s->pick_ok[c->a]) {
        *why = "partyUiHandler:noEnergy";
        return false;
    }
    *ok = true;
    /* RELEASE mode needs no confirmation: doRelease says goodbye, drops the member's held items (a Lucky Egg
     * goes with it) and the member, then addToParty takes the place */
    gc_ev_msg(run, release_key(run), 1, gc_m_mon(run, "pokemonName", 0, c->a));
    gc_items_drop_held_of(run, run->party[c->a].b.id);
    add_to_party(run, p->a, p->b, c->a);
    return true;
}
