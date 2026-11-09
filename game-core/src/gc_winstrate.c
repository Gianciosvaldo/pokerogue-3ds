/* The Winstrate Challenge (src/data/mystery-encounters/encounters/the-winstrate-challenge-encounter.ts at pinned upstream 8555c08).
 *
 * ROGUE tier, waves 100..180, a party of 3 or more, at most once a run. onInit queues five trainer configs back to front and the fights pop them off the end,
 * so the order is Victor (2 Pokemon), Victoria (2), Vivi (3), Vicky (1) and Vito (5). Every team is fixed: the species, the form, the nature, the ability slot
 * (when the config names one), the four moves and the held items, most of them locked (isTransferable false); the levels are the trainer's party template at
 * the wave (getPartyLevels) and nothing else about a member is chosen.
 *
 * Option 1: doContinueEncounter is set, then the first await (transitionMysteryEncounterIntroVisuals): everything after it, the whole chain, runs outside the
 * option's seed offset, on the run's RNG. initBattleWithEnemyConfig for one trainer: the gender (randSeedInt(2): no `female` in the config), the party levels,
 * and each Pokemon made by addEnemyPokemon(species, level, TRAINER) with the config applied after it; generateEnemyModifiers names every Pokemon's items, so no
 * item is rolled. Each win is handleMysteryEncounterVictory for a trainer battle: BattleEndPhase, TrainerVictoryPhase (money, the trainer's victory line) and
 * the MysteryEncounterRewardsPhase, which runs doContinueEncounter instead of opening a shop: endTrainerBattleAndShowDialogue resets the arena effects, calls
 * the player's Pokemon back (the Eiscue Noice form change, the per-battle data, the PostBattleInit abilities) and makes the next trainer's team. After the fifth
 * trainer: the victory line, an Egg Voucher Premium, the second victory line, and a shop of one Macho Brace (tier MASTER) with no other rewards.
 * Option 2: PartyHealPhase, a shop of one Rarer Candy, no other rewards, and the encounter ends.
 *
 * Not ported: the speakers' names on the dialogue, the trainers' sprites and their slide-out, the COMMANDED tag lapse (Tatsugiri) before a Pokemon is called back. */
#include "internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NS "mysteryEncounters/theWinstrateChallenge"

typedef struct { const char *key; int16_t arg; int8_t stack; bool locked; } w_item;
typedef struct { uint16_t species; int8_t form, ability; uint8_t nature; uint16_t moves[4]; uint8_t n_items; w_item items[12]; } w_mon;   /* ability -1: not named by the config */
typedef struct { int16_t trainer; uint8_t n; w_mon mon[5]; } w_team;

#define LOCKED(k, a, s) {k, a, s, true}
#define FREE(k, a, s) {k, a, s, false}

/* in fight order: the config popped last is pushed first */
static const w_team TEAMS[GC_WINSTRATE_FIGHTS] = {
    {TRAINER_TYPE_VICTOR, 2, {
        {SPECIES_ID_SWELLOW, 0, 0, NATURE_ADAMANT, {MOVE_ID_FACADE, MOVE_ID_BRAVE_BIRD, MOVE_ID_PROTECT, MOVE_ID_QUICK_ATTACK}, 2,
         {LOCKED("FLAME_ORB", -1, 1), LOCKED("FOCUS_BAND", -1, 2)}},
        {SPECIES_ID_OBSTAGOON, 0, 1, NATURE_ADAMANT, {MOVE_ID_FACADE, MOVE_ID_OBSTRUCT, MOVE_ID_NIGHT_SLASH, MOVE_ID_FIRE_PUNCH}, 2,
         {LOCKED("FLAME_ORB", -1, 1), LOCKED("LEFTOVERS", -1, 2)}}}},
    {TRAINER_TYPE_VICTORIA, 2, {
        {SPECIES_ID_ROSERADE, 0, 0, NATURE_CALM, {MOVE_ID_SYNTHESIS, MOVE_ID_SLUDGE_BOMB, MOVE_ID_GIGA_DRAIN, MOVE_ID_SLEEP_POWDER}, 2,
         {LOCKED("SOUL_DEW", -1, 1), LOCKED("QUICK_CLAW", -1, 2)}},
        {SPECIES_ID_GARDEVOIR, 1, -1, NATURE_TIMID, {MOVE_ID_PSYSHOCK, MOVE_ID_MOONBLAST, MOVE_ID_SHADOW_BALL, MOVE_ID_WILL_O_WISP}, 2,
         {LOCKED("ATTACK_TYPE_BOOSTER", POKEMON_TYPE_PSYCHIC, 1), LOCKED("ATTACK_TYPE_BOOSTER", POKEMON_TYPE_FAIRY, 1)}}}},
    {TRAINER_TYPE_VIVI, 3, {
        {SPECIES_ID_SEAKING, 0, 3, NATURE_ADAMANT, {MOVE_ID_WATERFALL, MOVE_ID_MEGAHORN, MOVE_ID_KNOCK_OFF, MOVE_ID_REST}, 2,
         {LOCKED("BERRY", BERRY_TYPE_LUM, 2), LOCKED("BASE_STAT_BOOSTER", STAT_HP, 4)}},
        {SPECIES_ID_BRELOOM, 0, 1, NATURE_JOLLY, {MOVE_ID_SPORE, MOVE_ID_SWORDS_DANCE, MOVE_ID_SEED_BOMB, MOVE_ID_DRAIN_PUNCH}, 2,
         {LOCKED("BASE_STAT_BOOSTER", STAT_HP, 4), LOCKED("TOXIC_ORB", -1, 1)}},
        {SPECIES_ID_CAMERUPT, 1, -1, NATURE_CALM, {MOVE_ID_EARTH_POWER, MOVE_ID_FIRE_BLAST, MOVE_ID_YAWN, MOVE_ID_PROTECT}, 1,
         {LOCKED("QUICK_CLAW", -1, 3)}}}},
    {TRAINER_TYPE_VICKY, 1, {
        {SPECIES_ID_MEDICHAM, 1, -1, NATURE_IMPISH, {MOVE_ID_AXE_KICK, MOVE_ID_ICE_PUNCH, MOVE_ID_ZEN_HEADBUTT, MOVE_ID_BULLET_PUNCH}, 1,
         {LOCKED("SHELL_BELL", -1, 1)}}}},
    {TRAINER_TYPE_VITO, 5, {
        {SPECIES_ID_HISUI_ELECTRODE, 0, 0, NATURE_MODEST, {MOVE_ID_THUNDERBOLT, MOVE_ID_GIGA_DRAIN, MOVE_ID_FOUL_PLAY, MOVE_ID_THUNDER_WAVE}, 1,
         {LOCKED("BASE_STAT_BOOSTER", STAT_SPD, 2)}},
        {SPECIES_ID_SWALOT, 0, 2, NATURE_QUIET, {MOVE_ID_SLUDGE_BOMB, MOVE_ID_GIGA_DRAIN, MOVE_ID_ICE_BEAM, MOVE_ID_EARTHQUAKE}, 11,
         {FREE("BERRY", BERRY_TYPE_SITRUS, 2), FREE("BERRY", BERRY_TYPE_APICOT, 2), FREE("BERRY", BERRY_TYPE_GANLON, 2), FREE("BERRY", BERRY_TYPE_STARF, 2),
          FREE("BERRY", BERRY_TYPE_SALAC, 2), FREE("BERRY", BERRY_TYPE_LUM, 2), FREE("BERRY", BERRY_TYPE_LANSAT, 2), FREE("BERRY", BERRY_TYPE_LIECHI, 2),
          FREE("BERRY", BERRY_TYPE_PETAYA, 2), FREE("BERRY", BERRY_TYPE_ENIGMA, 2), FREE("BERRY", BERRY_TYPE_LEPPA, 2)}},
        {SPECIES_ID_DODRIO, 0, 2, NATURE_JOLLY, {MOVE_ID_DRILL_PECK, MOVE_ID_QUICK_ATTACK, MOVE_ID_THRASH, MOVE_ID_KNOCK_OFF}, 1,
         {LOCKED("KINGS_ROCK", -1, 2)}},
        {SPECIES_ID_ALAKAZAM, 1, -1, NATURE_BOLD, {MOVE_ID_PSYCHIC, MOVE_ID_SHADOW_BALL, MOVE_ID_FOCUS_BLAST, MOVE_ID_THUNDERBOLT}, 1,
         {LOCKED("WIDE_LENS", -1, 2)}},
        {SPECIES_ID_DARMANITAN, 0, 0, NATURE_IMPISH, {MOVE_ID_EARTHQUAKE, MOVE_ID_U_TURN, MOVE_ID_FLARE_BLITZ, MOVE_ID_ROCK_SLIDE}, 1,
         {LOCKED("QUICK_CLAW", -1, 2)}}}},
};

int gc_winstrate_generated[5][4];   /* each member's moveset as the constructor made it, before the config's own (for the differential) */

/* one of the trainer's Pokemon: addEnemyPokemon(species, level, TRAINER) on the run's RNG, then the config's form, nature, ability slot and moves */
static bool make_member(gc_run *run, int e, const w_mon *w, int level, gc_mon *out)
{
    pr_enemy_gen_ctx ec;
    gc_enemy_ctx(run, &ec);
    ec.species = w->species;
    ec.level = level;
    ec.trainer_slot = TRAINER_SLOT_TRAINER;
    ec.boss = false;
    ec.shiny_lock = false;
    pr_enemy_gen g;
    memset(&g, 0, sizeof g);
    g.fusion_species = -1;
    g.friendship = -1;
    if (pr_run_enemy_pokemon_base(&run->run, &ec, &g) < 0) return false;
    gc_mon_new_enemy(run, out, w->species, level, &g, ec.trainer_slot);
    const bool made = gc_generate_moveset(run, &run->run.rng, out);
    if (!made) gc_port_enemy_moveset(run, out);
    for (int k = 0; k < 4; k++) {
        g.moveset[k] = k < out->b.n_moves ? out->b.moves[k] : 0;
        gc_winstrate_generated[e][k] = g.moveset[k];
    }
    if (pr_run_enemy_pokemon_finish(&run->run, &ec, &g) < 0) return false;
    /* the config: formIndex, nature (set as the Pokemon's own), abilityIndex, moveSet */
    g.form = (uint8_t)w->form;
    if (w->ability >= 0) g.ability_index = (uint8_t)w->ability;
    g.nature = w->nature;
    gc_refresh_enemy(run, out, w->species, level, &g, ec.trainer_slot);
    out->base_nature = w->nature;   /* g.nature made b.nature already */
    memset(out->b.moves, 0, sizeof out->b.moves);
    memset(out->b.pp_used, 0, sizeof out->b.pp_used);
    memset(out->b.pp_up, 0, sizeof out->b.pp_up);
    out->b.n_moves = 0;
    for (int k = 0; k < 4; k++) { out->b.max_pp_override[k] = -1; gc_mon_set_move(out, k, w->moves[k]); }
    return true;
}

static void say_trainer_fight(gc_run *run, gc_internal *in)
{
    gc_event *ev = gc_ev_new(run, GC_EV_TRAINER);
    if (ev) { ev->a = run->trainer_type; ev->b = run->trainer_variant; }
    gc_ev_trainer_dialogue(run, 0);   /* MysteryEncounterBattlePhase: the trainer's line comes before "X would like to battle!" */
    gc_ev_msg(run, "battle:trainerAppeared", 1, gc_m_trainer(run, "trainerName", GC_TNAME_TITLED));
    const bool sent_out = gc_battle_setup_field(run, true);
    gc_ev_msg(run, "battle:trainerSendOut", 2, gc_m_trainer(run, "trainerName", GC_TNAME_TRAINER), gc_m_mon_name(run, "pokemonName", 1, 0));
    if (sent_out) gc_ev_msg(run, "battle:playerGo", 1, gc_m_mon_name(run, "pokemonName", 0, gc_player_active_index(run)));
    if (run->eng.awaiting_confirm) gc_rq_unshift(run, gc_rp(RP_CHECK_SWITCH, 0, 0, 0));
    in->at_save_point = false;   /* the encounter's one save is the one at its start */
    gc_rq_push(run, gc_rp(RP_TURN_INIT, 0, 0, 0));
}

/* initBattleWithEnemyConfig for the k-th trainer (false: a species or trainer run-core cannot make) */
static bool build_fight(gc_run *run, int k)
{
    gc_internal *in = gc_int(run);
    const w_team *t = &TEAMS[k];
    const bool female = pr_rng_seed_int(&run->run.rng, 2, 0) != 0;   /* partyConfig.female == null: !!randSeedInt(2) */
    run->trainer_type = t->trainer;   /* the trainer exists before its Pokemon do (the moveset reads its class) */
    run->battle_type = BATTLE_TYPE_TRAINER;
    run->double_battle = false;
    const pr_party_template *tpl = gc_party_template_of(run, t->trainer, 0, run->wave);
    if (!tpl || tpl->size != t->n) return false;
    uint8_t strengths[8];
    for (int i = 0; i < tpl->size && i < 8; i++) strengths[i] = (uint8_t)tpl->strengths[i];
    int levels[PR_MAX_PARTY_TEMPLATE];
    if (pr_run_get_party_levels(&run->run, run->wave, tpl->size, strengths, tpl->strength, false, levels) != t->n) return false;
    static gc_mon made[5];
    for (int e = 0; e < t->n; e++)
        if (!make_member(run, e, &t->mon[e], levels[e], &made[e])) return false;

    run->trainer_variant = female ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT;
    run->trainer_name = run->trainer_partner_name = -1;
    in->custom_trainer_name[0] = 0;
    in->custom_team_on = false;
    run->n_enemy = t->n;
    for (int e = 0; e < t->n; e++) run->enemy[e] = made[e];

    /* modifierConfigs: every Pokemon names its items (generateEnemyModifiers makes no item rolls) */
    static gc_item items[24];
    int n = 0;
    memset(items, 0, sizeof items);
    for (int e = 0; e < t->n; e++) {
        for (int i = 0; i < t->mon[e].n_items && n < 24; i++, n++) {
            items[n].key = t->mon[e].items[i].key;
            items[n].arg = t->mon[e].items[i].arg;
            items[n].stack = t->mon[e].items[i].stack;
            items[n].pokemon_id = run->enemy[e].b.id;
            items[n].non_transferable = t->mon[e].items[i].locked;
        }
    }
    run->me_battle = true;
    gc_enemy_finish_party_holders(run, items, n);

    in->me_post.pending = false;
    in->me_reward.active = false;
    in->me_no_stats = false;
    run->me_catch_allowed = false;
    run->mystery_type = run->mystery_tier = -1;
    run->in_battle = true;
    say_trainer_fight(run, in);
    return true;
}

/* spawnNextTrainerOrEndEncounter with nothing left to pop */
static void finish(gc_run *run)
{
    gc_internal *in = gc_int(run);
    in->winstrate.active = false;
    in->winstrate.next = 0;
    gc_ev_msg(run, NS ":victory", 0);
    gc_reward_gain(run, "VOUCHER_PREMIUM");   /* the 10x voucher: addModifier, "You received 1x Egg Voucher Premium!" */
    gc_ev_msg(run, NS ":victory2", 0);
    /* setEncounterRewards({guaranteedModifierTypeOptions: [Macho Brace with tier MASTER], fillRemaining: false}), then leaveEncounterWithoutBattle(false, NO_BATTLE) */
    pr_reward_custom *c = &in->reward.me_custom;
    memset(c, 0, sizeof *c);
    c->options[c->n_options] = pr_reward_pregen_option(gc_reward_ctx(run), PR_ITEM_MYSTERY_ENCOUNTER_MACHO_BRACE, -1);
    c->options[c->n_options++].tier = MODIFIER_TIER_MASTER;
    c->fill_remaining = false;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_me_end_shop(run);
}

/* the encounter's first fight fell through (or a later one): nobody is left to fight */
static void give_up(gc_run *run, bool first)
{
    gc_stub(run, GC_STUB_UNPORTED_ENEMY);
    gc_int(run)->winstrate.active = false;
    if (first) gc_me_leave_with_heal_shop(run);
    else finish(run);
}

void gc_winstrate_execute(gc_run *run, int option)
{
    gc_internal *in = gc_int(run);
    if (option != 0) {
        /* refuse the challenge: PartyHealPhase, a Rarer Candy and nothing else */
        gc_rq_unshift(run, gc_rp(RP_PARTY_HEAL, 0, 0, 0));
        gc_me_leave_with_shop(run, "RARER_CANDY");
        return;
    }
    in->winstrate.active = true;
    in->winstrate.next = 1;
    if (!build_fight(run, 0)) give_up(run, true);
}

/* MysteryEncounterRewardsPhase running doContinueEncounter: endTrainerBattleAndShowDialogue */
void gc_winstrate_next(gc_run *run)
{
    gc_internal *in = gc_int(run);
    const int k = in->winstrate.next;
    if (!in->winstrate.active) return;
    if (k >= GC_WINSTRATE_FIGHTS) { finish(run); return; }
    /* arena.resetArenaEffects(), then the player's Pokemon come back (PostBattleInit abilities, the per-battle data) before the next team is made */
    gc_arena_reset_effects(run);
    gc_battle_cleanup_players(run);
    in->winstrate.next = (uint8_t)(k + 1);
    if (!build_fight(run, k)) give_up(run, false);
}

/* the fight is won: handleMysteryEncounterVictory, then the MysteryEncounterRewardsPhase that runs doContinueEncounter (no EggLapsePhase while it is set) */
void gc_winstrate_victory(gc_run *run)
{
    gc_rq_push(run, gc_rp(RP_BATTLE_END, 1, 0, 0));
    gc_rq_push(run, gc_rp(RP_TRAINER_VICTORY, 0, 0, 0));
    gc_rq_push(run, gc_rp(RP_WINSTRATE, 0, 0, 0));
}
