/* The Expert Pokemon Breeder (src/data/mystery-encounters/encounters/the-expert-pokemon-breeder-encounter.ts at pinned upstream 8555c08).
 *
 * onInit, in executeWithSeedOffset(.., waveIndex): the trainer's party (getPartyConfig) is a Cleffa of the wave's stage (Cleffa below wave 45, Clefairy below
 * 75, Clefable after) that always has the same moves, nature, IVs, ability slot, Tera type and friendship, and two more: in the Space biome two more Cleffa
 * of their own (shiny, variants 1 and 2); anywhere else one from the first pool and one from the second, each randSeedShuffle'd and popped, then evolved
 * as far as the wave allows. The player is offered the three Pokemon allowed in battle with the least friendship (Array.sort is stable: the party's order
 * breaks ties), and for each the eggs the fight would earn: min(starter points + BST points, 30) points, the first 5 to Common eggs, then a Rare egg per 4.
 *
 * An option (on the option's stream): setEncounterRewards({SOOTHE_BELL, fillRemaining}, the eggs, the cleanup callback); the party is the chosen Pokemon
 * alone (the others and their held items wait, and the chosen one moves to the front: the party is [chosen, ...the rest with the old lead in the chosen
 * one's place] when they are back); initBattleWithEnemyConfig: the trainer's gender (randSeedInt(2)), the party levels of the trainer's template (3 weak
 * members), and the three Pokemon made by addEnemyPokemon with the config's fields (the Cleffa's Tera type also lists its slot in the trainer's
 * instantTeras), each holding three Soothe Bells; the item rolls after the first await are not made (every Pokemon names its items).
 * Win: the rewards phase runs the callback (the chosen Pokemon gets 20 friendship, the others come back), opens the shop with the Soothe Bell, gives the
 * eggs, and says the outro. Loss (the chosen Pokemon fainted: GameOverPhase asks onGameOver): the others come back, the chosen one's friendship is 0 and it
 * stays fainted, the rewards are cleared, and the encounter ends with the heal-only shop and the failure outro.
 *
 * Not ported: the Cleffa's nickname (Ace, Clefablest, "Clefable the Great"; the engine has no nicknames), the intro and outro speaker names, the trainer's
 * and Cleffa's sprites, the unseeded egg rolls (the new Egg's species comes from randomString(24): the eggs here are drawn from a seed of the wave). */
#include "internal.h"
#include "gc/gacha.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

#define NS "mysteryEncounters/theExpertPokemonBreeder"
#define FIRST_STAGE 45
#define SECOND_STAGE 60
#define FINAL_STAGE 75

/* a chain of BreederSpeciesEvolution: the first link is a plain species (wave 0), the others evolve at their wave */
typedef struct { uint8_t n; struct { uint16_t species; uint8_t wave; } l[3]; } chain_t;

static const chain_t POOL1[17] = {
    {3, {{SPECIES_ID_PICHU, 0}, {SPECIES_ID_PIKACHU, FIRST_STAGE}, {SPECIES_ID_RAICHU, FINAL_STAGE}}},
    {3, {{SPECIES_ID_PICHU, 0}, {SPECIES_ID_PIKACHU, FIRST_STAGE}, {SPECIES_ID_ALOLA_RAICHU, FINAL_STAGE}}},
    {3, {{SPECIES_ID_IGGLYBUFF, 0}, {SPECIES_ID_JIGGLYPUFF, FIRST_STAGE}, {SPECIES_ID_WIGGLYTUFF, FINAL_STAGE}}},
    {3, {{SPECIES_ID_TOGEPI, 0}, {SPECIES_ID_TOGETIC, FIRST_STAGE}, {SPECIES_ID_TOGEKISS, FINAL_STAGE}}},
    {2, {{SPECIES_ID_TYROGUE, 0}, {SPECIES_ID_HITMONLEE, SECOND_STAGE}}},
    {2, {{SPECIES_ID_TYROGUE, 0}, {SPECIES_ID_HITMONCHAN, SECOND_STAGE}}},
    {2, {{SPECIES_ID_TYROGUE, 0}, {SPECIES_ID_HITMONTOP, SECOND_STAGE}}},
    {2, {{SPECIES_ID_SMOOCHUM, 0}, {SPECIES_ID_JYNX, FIRST_STAGE}}},
    {3, {{SPECIES_ID_AZURILL, 0}, {SPECIES_ID_MARILL, FIRST_STAGE}, {SPECIES_ID_AZUMARILL, FINAL_STAGE}}},
    {3, {{SPECIES_ID_BUDEW, 0}, {SPECIES_ID_ROSELIA, FIRST_STAGE}, {SPECIES_ID_ROSERADE, FINAL_STAGE}}},
    {2, {{SPECIES_ID_CHINGLING, 0}, {SPECIES_ID_CHIMECHO, SECOND_STAGE}}},
    {2, {{SPECIES_ID_BONSLY, 0}, {SPECIES_ID_SUDOWOODO, SECOND_STAGE}}},
    {2, {{SPECIES_ID_MIME_JR, 0}, {SPECIES_ID_MR_MIME, SECOND_STAGE}}},
    {3, {{SPECIES_ID_MIME_JR, 0}, {SPECIES_ID_GALAR_MR_MIME, SECOND_STAGE}, {SPECIES_ID_MR_RIME, FINAL_STAGE}}},
    {3, {{SPECIES_ID_HAPPINY, 0}, {SPECIES_ID_CHANSEY, FIRST_STAGE}, {SPECIES_ID_BLISSEY, FINAL_STAGE}}},
    {2, {{SPECIES_ID_MANTYKE, 0}, {SPECIES_ID_MANTINE, SECOND_STAGE}}},
    {2, {{SPECIES_ID_TOXEL, 0}, {SPECIES_ID_TOXTRICITY, SECOND_STAGE}}},
};

static const chain_t POOL2[7] = {
    {1, {{SPECIES_ID_DITTO, 0}}},
    {3, {{SPECIES_ID_ELEKID, 0}, {SPECIES_ID_ELECTABUZZ, FIRST_STAGE}, {SPECIES_ID_ELECTIVIRE, FINAL_STAGE}}},
    {3, {{SPECIES_ID_MAGBY, 0}, {SPECIES_ID_MAGMAR, FIRST_STAGE}, {SPECIES_ID_MAGMORTAR, FINAL_STAGE}}},
    {2, {{SPECIES_ID_WYNAUT, 0}, {SPECIES_ID_WOBBUFFET, SECOND_STAGE}}},
    {2, {{SPECIES_ID_MUNCHLAX, 0}, {SPECIES_ID_SNORLAX, SECOND_STAGE}}},
    {2, {{SPECIES_ID_RIOLU, 0}, {SPECIES_ID_LUCARIO, SECOND_STAGE}}},
    {1, {{SPECIES_ID_AUDINO, 0}}},
};

/* getSpeciesFromPool: randSeedShuffle(pool), the last chain, its last link that the wave has reached */
static int species_from_pool(const chain_t *pool, int n, int wave, pr_rng *r)
{
    int idx[17];
    for (int i = 0; i < n; i++) idx[i] = i;
    for (int i = n - 1; i > 0; i--) {
        const int j = pr_rng_integer_in_range(r, 0, i);
        const int t = idx[i];
        idx[i] = idx[j];
        idx[j] = t;
    }
    const chain_t *c = &pool[idx[n - 1]];
    int k = c->n - 1;
    while (k > 0 && c->l[k].wave && c->l[k].wave > wave) k--;
    return c->l[k].species;
}

static int cleffa_of_wave(int wave)
{
    return wave < FIRST_STAGE ? SPECIES_ID_CLEFFA : wave < FINAL_STAGE ? SPECIES_ID_CLEFAIRY : SPECIES_ID_CLEFABLE;
}

/* the party's order: storage indexes of the Pokemon allowed in battle, the least friendship first, ties in party order */
bool gc_breeder_picks(const gc_run *run, int out[3])
{
    int list[PR_MAX_PARTY], n = 0;
    for (int k = 0; k < run->n_party; k++) {
        const int idx = run->party_order[k];
        if (gc_mon_allowed(&run->party[idx])) list[n++] = idx;
    }
    for (int i = 1; i < n; i++) {   /* insertion sort: stable */
        const int v = list[i];
        int j = i - 1;
        while (j >= 0 && run->party[list[j]].b.friendship > run->party[v].b.friendship) { list[j + 1] = list[j]; j--; }
        list[j + 1] = v;
    }
    if (n < 3) return false;
    for (int k = 0; k < 3; k++) out[k] = list[k];
    return true;
}

/* calculateEggRewardsForPokemon: [common eggs, rare eggs] */
void gc_breeder_eggs(const gc_mon *m, int *commons, int *rares)
{
    const pr_form *f = gc_form(m->b.species, m->b.form);
    const int bst = f ? f->base_total : 0;
    /* 1 point for every 20 points below 680 BST (max 18, min 1) */
    int from_bst = (int)floor((680 - bst) / 20.0);
    if (from_bst < 1) from_bst = 1;
    if (from_bst > 18) from_bst = 18;
    /* 2 points for every 1 below 7 that the starter cost of the root species is (max 12, min 0) */
    int from_cost = 0;
    const int cost = gc_starter_cost(pr_run_species_root(m->b.species));
    if (cost >= 0) {
        from_cost = (7 - cost) * 2;
        if (from_cost < 0) from_cost = 0;
        if (from_cost > 12) from_cost = 12;
    }
    int total = from_cost + from_bst;
    if (total > 30) total = 30;
    int c = total < 5 ? total : 5;   /* the first 5 points are Common eggs */
    total -= c;
    *rares = total / 4;              /* then a Rare egg for every 4 points */
    c += total % 4;                  /* and a Common egg for every point left over */
    *commons = c;
}

/* the party config of onInit: three species (the Cleffa and two more), on the wave's stream */
void gc_breeder_party_species(const gc_run *run, int out[3])
{
    out[0] = cleffa_of_wave(run->wave);
    if (run->biome == BIOME_ID_SPACE) {
        out[1] = out[2] = out[0];
        return;
    }
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    out[1] = species_from_pool(POOL1, 17, run->wave, &r);
    out[2] = species_from_pool(POOL2, 7, run->wave, &r);
}

/* removePokemonFromPartyAndStoreHeldItems */
static void remove_others(gc_run *run, int chosen)
{
    gc_internal *in = gc_int(run);
    int ord[PR_MAX_PARTY], n = run->n_party;
    for (int k = 0; k < n; k++) ord[k] = run->party_order[k];
    int pos = 0;
    for (int k = 0; k < n; k++) if (ord[k] == chosen) pos = k;
    ord[pos] = ord[0];   /* party[chosenIndex] = party[0]; party[0] = chosen (the chosen one is alone in the party, so only the old lead's new place matters) */
    in->breeder.n_rest = (uint8_t)(n - 1);
    in->breeder.n_kept = 0;
    in->breeder.chosen_id = run->party[chosen].b.id;
    for (int k = 1; k < n; k++) {
        gc_mon *m = &in->breeder.rest[k - 1];
        *m = run->party[ord[k]];
        for (int i = 0; i < run->n_items; i++) {
            if (run->items[i].pokemon_id != m->b.id || !m->b.id) continue;
            if (in->breeder.n_kept >= (int)(sizeof in->breeder.kept / sizeof *in->breeder.kept)) { gc_stub(run, GC_STUB_REWARD_APPLY); break; }
            in->breeder.kept[in->breeder.n_kept++] = run->items[i];
        }
    }
    for (int k = 0; k < in->breeder.n_rest; k++) {
        int idx = -1;
        for (int i = 0; i < run->n_party; i++) if (run->party[i].b.id == in->breeder.rest[k].b.id) idx = i;
        gc_party_remove(run, idx);
    }
    in->breeder.away = true;
    gc_me_prune_items(run);
    gc_me_update_modifiers(run);
}

/* restorePartyAndHeldItems: the others at the end of the party in the order they left, their items at the end of the list */
static void restore_others(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->breeder.away) return;
    in->breeder.away = false;
    for (int k = 0; k < in->breeder.n_rest && run->n_party < PR_MAX_PARTY; k++) {
        run->party[run->n_party] = in->breeder.rest[k];
        run->party_order[run->n_party] = (int8_t)run->n_party;
        run->n_party++;
    }
    for (int k = 0; k < in->breeder.n_kept && run->n_items < GC_MAX_ITEMS && run->n_items < PR_MAX_MODIFIERS; k++) run->items[run->n_items++] = in->breeder.kept[k];
    in->breeder.n_rest = in->breeder.n_kept = 0;
    gc_me_update_modifiers(run);
    /* the battle's view of the party (the shop and the level-ups after it read it) has them again */
    run->bt.n_party[0] = (uint8_t)run->n_party;
    for (int k = 0; k < run->n_party; k++) run->bt.party[0][k] = &run->party[run->party_order[k]].b;
}

static gc_mon *chosen_mon(gc_run *run)
{
    const uint32_t id = gc_int(run)->breeder.chosen_id;
    for (int i = 0; i < run->n_party; i++) if (run->party[i].b.id == id) return &run->party[i];
    return NULL;
}

/* one of the trainer's Pokemon: addEnemyPokemon(species, level, TRAINER) and the config's fields, on the option's stream */
static bool slot0_listed;   /* the process-wide state upstream keeps in the trainer config's instantTeras: set by the first fight, kept by every later one */
int gc_breeder_generated[3][4];   /* each member's moveset as the constructor made it, before the config's own (for the differential) */

static bool make_member(gc_run *run, int e, int species, int level, gc_mon *out)
{
    pr_enemy_gen_ctx ec;
    gc_enemy_ctx(run, &ec);
    ec.species = species;
    ec.level = level;
    ec.trainer_slot = TRAINER_SLOT_TRAINER;
    ec.boss = false;
    ec.shiny_lock = false;
    pr_enemy_gen g;
    memset(&g, 0, sizeof g);
    g.fusion_species = -1;
    g.friendship = -1;
    if (pr_run_enemy_pokemon_base(&run->run, &ec, &g) < 0) return false;
    gc_mon_new_enemy(run, out, species, level, &g, ec.trainer_slot);
    gc_internal *in = gc_int(run);
    /* willTerastallize() reads the shared trainer config's instantTeras, and initBattleWithEnemyConfig adds the Cleffa's slot to it for good once its Tera
     * type is set (the config object is never reset): the first Breeder fight of a session makes the Cleffa's moveset without the Tera type, every later one with it */
    in->ctor_will_tera = e == 0 && slot0_listed;
    const bool made = gc_generate_moveset(run, &run->run.rng, out);
    in->ctor_will_tera = false;
    if (!made) gc_port_enemy_moveset(run, out);
    if (e >= 0 && e < 3) for (int k = 0; k < 4; k++) gc_breeder_generated[e][k] = k < out->b.n_moves ? out->b.moves[k] : 0;
    for (int k = 0; k < 4; k++) g.moveset[k] = k < out->b.n_moves ? out->b.moves[k] : 0;
    if (pr_run_enemy_pokemon_finish(&run->run, &ec, &g) < 0) return false;
    gc_refresh_enemy(run, out, species, level, &g, ec.trainer_slot);
    return true;
}

static void set_moves(gc_mon *m, const int *mv)
{
    memset(m->b.moves, 0, sizeof m->b.moves);
    memset(m->b.pp_used, 0, sizeof m->b.pp_used);
    memset(m->b.pp_up, 0, sizeof m->b.pp_up);
    m->b.n_moves = 0;
    for (int k = 0; k < 4; k++) { m->b.max_pp_override[k] = -1; gc_mon_set_move(m, k, mv[k]); }
}

/* EnemyPokemonConfig of the party: the fields initBattleWithEnemyConfig puts on the Pokemon after it is made */
static void apply_config(gc_run *run, int e, gc_mon *m)
{
    static const int ace[4] = {MOVE_ID_FIRE_PUNCH, MOVE_ID_ICE_PUNCH, MOVE_ID_THUNDER_PUNCH, MOVE_ID_METEOR_MASH};
    static const int great[4] = {MOVE_ID_DAZZLING_GLEAM, MOVE_ID_MYSTICAL_FIRE, MOVE_ID_ICE_BEAM, MOVE_ID_THUNDERBOLT};
    static const int the_great[4] = {MOVE_ID_TRI_ATTACK, MOVE_ID_STORED_POWER, MOVE_ID_CALM_MIND, MOVE_ID_MOONLIGHT};
    const bool space = run->biome == BIOME_ID_SPACE;
    if (e > 0 && !space) return;   /* the pool members name only their species and items */
    int nature, ability, variant = -1;
    const int *mv;
    if (e == 0) { nature = NATURE_ADAMANT; ability = 1; mv = ace; }
    else if (e == 1) { nature = NATURE_MODEST; ability = 1; mv = great; variant = 1; }
    else { nature = NATURE_BOLD; ability = 2; mv = the_great; variant = 2; }
    if (variant >= 0) {   /* shiny: true, variant */
        m->b.shiny = true;
        m->variant = (uint8_t)variant;
        m->b.variant = (uint8_t)variant;
    } else {
        m->b.shiny = false;
    }
    m->b.nature = (uint8_t)nature;
    m->base_nature = (uint8_t)nature;
    for (int i = 0; i < 6; i++) m->b.ivs[i] = 31;
    gc_mon_publish_ivs(run, m);
    m->ability_index = (uint8_t)ability;
    m->b.ability_index = (int8_t)ability;
    m->b.ability = m->b.base_ability = (uint16_t)gc_form_ability(m->b.species, gc_form(m->b.species, m->b.form), ability);
    set_moves(m, mv);
    if (e == 0) m->b.tera_type = POKEMON_TYPE_FAIRY;
    m->b.friendship = 255;
}

static void say_trainer_fight(gc_run *run, gc_internal *in)
{
    gc_event *ev = gc_ev_new(run, GC_EV_TRAINER);
    if (ev) { ev->a = run->trainer_type; ev->b = run->trainer_variant; }
    gc_ev_msg(run, "battle:trainerAppeared", 1, gc_m_trainer(run, "trainerName", GC_TNAME_TITLED));
    const bool sent_out = gc_battle_setup_field(run, true);
    gc_ev_msg(run, "battle:trainerSendOut", 2, gc_m_trainer(run, "trainerName", GC_TNAME_TRAINER), gc_m_mon_name(run, "pokemonName", 1, 0));
    if (sent_out) gc_ev_msg(run, "battle:playerGo", 1, gc_m_mon_name(run, "pokemonName", 0, gc_player_active_index(run)));
    if (run->eng.awaiting_confirm) gc_rq_unshift(run, gc_rp(RP_CHECK_SWITCH, 0, 0, 0));
    in->at_save_point = false;   /* the encounter's one save is the one at its start */
    gc_rq_push(run, gc_rp(RP_TURN_INIT, 0, 0, 0));
}

bool gc_breeder_execute(gc_run *run, int option)
{
    gc_internal *in = gc_int(run);
    int pick[3];
    if (option < 0 || option > 2 || !gc_breeder_picks(run, pick)) { gc_me_leave_with_heal_shop(run); return true; }
    const int chosen = pick[option];
    int commons, rares;
    gc_breeder_eggs(&run->party[chosen], &commons, &rares);

    /* onInit's party, on the wave's stream; the option's own stream follows */
    int species[3];
    gc_breeder_party_species(run, species);
    pr_rng r = gc_me_option_rng(run);

    /* setEncounterRewards(.., eggs, cleanup); the party is the chosen Pokemon alone from here on */
    remove_others(run, chosen);
    in->breeder.commons = (uint8_t)commons;
    in->breeder.rares = (uint8_t)rares;
    in->breeder.failed = false;

    const pr_rng saved = run->run.rng;
    run->run.rng = r;
    bool ok = false;
    const bool female = pr_rng_seed_int(&run->run.rng, 2, 0) != 0;   /* partyConfig.female == null: !!randSeedInt(2) */
    const int trainer = TRAINER_TYPE_EXPERT_POKEMON_BREEDER;
    run->trainer_type = trainer;   /* the trainer exists before its Pokemon do (the moveset reads its class) */
    run->battle_type = BATTLE_TYPE_TRAINER;
    run->double_battle = false;
    const pr_party_template *tpl = gc_party_template_of(run, trainer, 0, run->wave);
    int levels[PR_MAX_PARTY_TEMPLATE];
    gc_mon made[3];
    if (tpl && tpl->size == 3) {
        uint8_t strengths[8];
        for (int i = 0; i < tpl->size && i < 8; i++) strengths[i] = (uint8_t)tpl->strengths[i];
        const int n = pr_run_get_party_levels(&run->run, run->wave, tpl->size, strengths, tpl->strength, false, levels);
        ok = n == 3;
        for (int e = 0; ok && e < 3; e++) {
            ok = make_member(run, e, species[e], levels[e], &made[e]);
            if (ok) apply_config(run, e, &made[e]);
        }
    }
    run->run.rng = saved;
    if (ok) slot0_listed = true;
    if (!ok) {   /* a species or a trainer run-core cannot make: nobody fights, the others come back and the encounter ends */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        restore_others(run);
        gc_me_leave_with_heal_shop(run);
        return true;
    }

    run->battle_type = BATTLE_TYPE_TRAINER;
    run->double_battle = false;
    run->trainer_type = trainer;
    run->trainer_variant = female ? TRAINER_VARIANT_FEMALE : TRAINER_VARIANT_DEFAULT;
    run->trainer_name = run->trainer_partner_name = -1;
    snprintf(in->custom_trainer_name, sizeof in->custom_trainer_name, "$t(trainerNames:expertPokemonBreeder)");   /* config.name */
    in->custom_team_on = false;
    run->n_enemy = 3;
    for (int e = 0; e < 3; e++) run->enemy[e] = made[e];

    /* modifierConfigs: three Soothe Bells for each Pokemon (generateEnemyModifiers names every Pokemon's items: no item rolls) */
    static gc_item bells[3];
    for (int e = 0; e < 3; e++) {
        memset(&bells[e], 0, sizeof bells[e]);
        bells[e].key = "SOOTHE_BELL";
        bells[e].arg = -1;
        bells[e].stack = 3;
        bells[e].pokemon_id = run->enemy[e].b.id;
    }
    run->me_battle = true;
    gc_enemy_finish_party_holders(run, bells, 3);
    run->enemy[0].tera_instant = false;
    in->me_tera_static = 1;   /* config.tera: battle.trainer.config.setInstantTera(0) */

    in->me_post.pending = true;   /* the outro: gc_me_win_rewards leaves after the shop, which queues it */
    in->me_post.a = GC_ME_THE_EXPERT_POKEMON_BREEDER;
    in->me_post.b = 0;
    in->me_post.c = 0;
    in->me_reward.active = true;
    in->me_reward.has_item = false;
    in->me_reward.func = (int16_t)gc_reward_item_index("SOOTHE_BELL");
    in->me_reward.fill = true;
    in->me_reward.before = true;
    in->me_no_stats = false;

    run->me_catch_allowed = false;
    run->mystery_type = run->mystery_tier = -1;
    run->in_battle = true;
    say_trainer_fight(run, in);
    return true;
}

/* doEncounterRewards: the callback (doPostEncounterCleanup: 20 friendship, the others back), the shop (queued by the win), then the eggs */
void gc_breeder_reward(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->breeder.away) return;
    gc_mon *m = chosen_mon(run);
    /* chosenPokemon.addFriendship(FRIENDSHIP_ADDED): the Soothe Bell's multiplier first (the Pokemon is the party's only one, so its items are in the list) */
    if (m && !gc_soothe_friendship(run, (int)(m - run->party), 20)) {
        gc_stub(run, GC_STUB_REWARD_APPLY);
        gc_system_add_friendship(run->sys, run, m, 20, false);
    }
    restore_others(run);
    in->custom_trainer_name[0] = 0;
    in->me_tera_static = 0;
    if (run->sys) {
        const int tiers[2] = {0, 1};   /* EggTier.COMMON, RARE */
        for (int t = 0; t < 2; t++) {
            const int count = t == 0 ? in->breeder.commons : in->breeder.rares;
            for (int i = 0; i < count; i++) {
                char seed[64];
                snprintf(seed, sizeof seed, "breeder-%d-%d-%d-%s", run->wave, t, i, run->battle_seed);
                gc_system_gift_egg_source(run->sys, 4, tiers[t], seed, 0);
            }
        }
    }
}

/* GameOverPhase asks the encounter (onGameOver): the others come back, the chosen one stays fainted with no friendship, nothing is earned */
bool gc_breeder_game_over(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->breeder.away) return false;
    gc_mon *m = chosen_mon(run);
    restore_others(run);
    m = chosen_mon(run);
    if (m) m->b.friendship = 0;
    in->breeder.failed = true;
    in->me_reward.active = false;
    in->custom_trainer_name[0] = 0;
    in->me_tera_static = 0;
    gc_rq_clear(run);
    /* handleMysteryEncounterBattleFailed(true): nobody left to fight is a lost game */
    bool able = false;
    for (int i = 0; i < run->n_party; i++) able = able || gc_mon_allowed(&run->party[i]);
    if (!able) {
        gc_rq_unshift(run, gc_rp(RP_GAME_OVER, 0, 0, 0));
        return false;
    }
    in->me_post.pending = true;
    in->me_post.a = GC_ME_THE_EXPERT_POKEMON_BREEDER;
    in->me_post.b = 1;
    in->me_post.c = 0;
    gc_rq_push(run, gc_rp(RP_BATTLE_END, 0, 0, 0));
    gc_me_leave_with_heal_shop(run);
    return true;
}

/* PostMysteryEncounterPhase: the outro (b = 1: the failed one) */
void gc_breeder_outro(gc_run *run, const gc_rphase *p)
{
    gc_internal *in = gc_int(run);
    if (p->b == 1) {
        gc_ev_msg(run, NS ":outroFailed", 0);
        return;
    }
    gc_mon *m = chosen_mon(run);
    if (m) gc_ev_msg(run, NS ":outro", 1, pr_arg_pokemon_name(&run->bt, "chosenPokemon", &m->b));
    const int counts[2] = {in->breeder.commons, in->breeder.rares};
    static const char *const rarity[2] = {"egg:defaultTier", "egg:greatTier"};
    for (int t = 0; t < 2; t++) {
        if (counts[t] <= 0) continue;
        gc_ev_msg(run, NS ":gainedEggs", 3, (pr_msg_arg){"numEggs", PR_ARG_TEMPLATE, 0, "$t(" NS ":numEggs)", 0},
                  pr_arg_int("count", counts[t]), pr_arg_key("rarity", rarity[t]));
    }
}
