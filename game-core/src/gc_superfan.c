/* The Bug-Type Superfan (src/data/mystery-encounters/encounters/bug-type-superfan-encounter.ts at pinned upstream 8555c08).
 *
 * The encounter spawns when a Pokemon of the whole party (fainted ones too) is a Bug type (TypeRequirement(BUG, false), the Tera type left out), or holds a transferable
 * Quick Claw or Grip Claw (HeldItemRequirement of BypassSpeedChanceModifier / ContactHeldItemTransferChanceModifier) or a transferable Bug attack type booster
 * (AttackTypeBoosterHeldItemTypeRequirement(BUG)): a Some(..) of the three. Option 1 (the fight, not ported yet: still the stand-in in gc_mystery.c) needs nothing.
 *
 * 2 Show Your Bug-types (TypeRequirement(BUG, false, 1) on the option): onPreOptionPhase, on the stream wave * 1000 + 512 of the option select, counts the party's Bug types
 * (isOfType(BUG): the Tera type counts, but nothing out of battle is Terastallized) and sets the rewards: 0 or 1 a Super Lure and a Great Ball, 2 or 3 a Quick Claw, a
 * Max Lure and an Ultra Ball, 4 or 5 a Grip Claw, a Max Lure and a Rogue Ball, 6 a Master Ball, a Mega Bracelet and a Dynamax Band the player does not have yet, and one
 * of the evolution / rare evolution / form change / rare form change items that can be made (each generator rolls in that order; then randSeedInt picks one of them).
 * The shop has exactly those (fillRemaining false); the text is option.2.selected0To1 / 2To3 / 4To5 / 6 (the app chooses by the count).
 * 3 Gift a Bug Item (a Pokemon holding a Quick Claw, a Grip Claw or a Bug attack type booster): the party screen's filter passes a Pokemon holding one (instanceof only, the
 * transfer flag is not asked), the second list names its transferable ones in getHeldItems() order. onOptionPhase: loseHeldItem(.., false) (one stack, the item with its last),
 * updateModifiers(true, true), and the shop has the Golden Bug Net (as a Rogue tier option) and a Reviver Seed (fillRemaining false). leaveEncounterWithoutBattle(true), but
 * a mystery encounter's own rewards replace the heal-only shop, so it is the same shop either way.
 *
 * 1 Offer to Battle: onInit builds the Superfan's team for the wave (getTrainerConfigForWave): 2 AVERAGE members below wave 30 (Beedrill and Butterfree), then
 * 3, 4, 5 AVERAGE ones (a Pool 1 and Pool 2 Bug type added), from wave 100 forms (Beedrill and Butterfree form 1, regenerating the moveset), a Pool 3 Pokemon that
 * onInit drew by randSeedShuffle on the wave's stream, then a STRONG Pool 4 one from wave 120, and bosses of 2 segments from wave 160. The option draws the four
 * Bug-type moves the winner may learn (one of each kind, randSeedInt on the option's stream), sets fillRemaining rewards and starts the fight (female trainer).
 * Each member is made on its own stream as Trainer.genPartyMember does (getRandomPartyMemberFunc with ignoreEvolution), the postProcess after the constructor.
 *
 * Not ported here: the Bug-type move tutor after the win, the intro sprites. */
#include "internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

int pr_reward_generate(pr_reward_ctx *ctx, int item);   /* reward-core/src/reward.c: generateType(party); -2 = the generator makes nothing */

/* AttackTypeBoosterModifier of the Bug type, BypassSpeedChanceModifier (Quick Claw), ContactHeldItemTransferChanceModifier (Grip Claw) */
static bool bug_item(const gc_item *it)
{
    return !strcmp(it->key, "QUICK_CLAW") || !strcmp(it->key, "GRIP_CLAW") || (!strcmp(it->key, "ATTACK_TYPE_BOOSTER") && it->arg == POKEMON_TYPE_BUG);
}

static bool is_bug(const gc_mon *m)
{
    int8_t ty[PR_MAX_TYPES];
    const int nt = pr_get_types(&m->b, ty, PR_TYPES_NO_TERA);
    for (int k = 0; k < nt; k++) if (ty[k] == POKEMON_TYPE_BUG) return true;
    return false;
}

int gc_superfan_num_bug_types(const gc_run *run)
{
    int n = 0;
    for (int i = 0; i < run->n_party; i++) n += is_bug(&run->party[i]);
    return n;
}

/* the party screen's filter (selectableFilter): the Pokemon holds a Quick Claw, a Grip Claw or a Bug attack type booster */
bool gc_superfan_pick_ok(const gc_run *run, int idx)
{
    if (idx < 0 || idx >= run->n_party) return false;
    const uint32_t id = run->party[idx].b.id;
    for (int i = 0; i < run->n_items; i++) if (run->items[i].pokemon_id == id && bug_item(&run->items[i])) return true;
    return false;
}

/* the second list (onPokemonSelected): the transferable ones, as run->items indexes in getHeldItems() order */
int gc_superfan_held_items(const gc_run *run, int idx, int out[GC_MAX_ITEMS])
{
    if (idx < 0 || idx >= run->n_party) return 0;
    const uint32_t id = run->party[idx].b.id;
    int n = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (it->pokemon_id == id && bug_item(it) && gc_gts_item_transferable(it)) out[n++] = i;
    }
    return n;
}

/* HeldItemRequirement / AttackTypeBoosterHeldItemTypeRequirement over the whole party: a Pokemon holds a transferable one */
static bool any_transferable_bug_item(const gc_run *run)
{
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (!it->pokemon_id || !bug_item(it) || !gc_gts_item_transferable(it)) continue;
        for (int k = 0; k < run->n_party; k++) if (run->party[k].b.id == it->pokemon_id) return true;
    }
    return false;
}

bool gc_superfan_spawnable(const gc_run *run)
{
    return gc_superfan_num_bug_types(run) > 0 || any_transferable_bug_item(run);
}

/* which options are on: the fight always, the Bug types for the second, an item for the third (the Some(..) of the two item requirements) */
bool gc_superfan_option_ok(const gc_run *run, int option)
{
    if (option == 0) return true;
    if (option == 1) return gc_superfan_num_bug_types(run) > 0;
    return any_transferable_bug_item(run);
}

/* setEncounterRewards({guaranteedModifierTypeOptions, guaranteedModifierTypeFuncs, fillRemaining: false}), then leaving without a battle */
static void shop(gc_run *run, const pr_reward_option *opts, int n_opts, const char *const *funcs, int n_funcs)
{
    pr_reward_custom *c = &gc_int(run)->reward.me_custom;
    memset(c, 0, sizeof *c);
    for (int i = 0; i < n_opts && c->n_options < PR_REWARD_MAX_OPTIONS; i++) c->options[c->n_options++] = opts[i];
    for (int i = 0; i < n_funcs && c->n_funcs < PR_REWARD_MAX_OPTIONS; i++) c->funcs[c->n_funcs++] = (uint16_t)gc_reward_item_index(funcs[i]);
    c->fill_remaining = false;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_int(run)->me_post.pending = true;   /* the outro, after the shop */
    gc_int(run)->me_post.a = GC_ME_BUG_TYPE_SUPERFAN;
    gc_int(run)->me_post.b = gc_int(run)->me_post.c = 0;
    gc_me_end_shop(run);
}

/* generateModifierTypeOption(func): the generator rolls on the current stream (a null result is no option), the tier is the item's from the player pool */
static bool generated_option(pr_reward_ctx *ctx, const char *key, pr_reward_option *out)
{
    const int item = gc_reward_item_index(key);
    if (item < 0) return false;
    const int arg = pr_reward_generate(ctx, item);
    if (arg == -2) return false;
    *out = pr_reward_pregen_option(ctx, item, arg);
    return true;
}

pr_rng gc_superfan_pre_rng;

/* option 2's onPreOptionPhase and the shop it leaves with */
static void show_bug_types(gc_run *run)
{
    const int n = gc_superfan_num_bug_types(run);
    gc_superfan_pre_rng = pr_run_seed_offset_rng(&run->run, run->wave * 1000 + 512, NULL);
    pr_reward_option opts[8];
    int n_opts = 0;
    if (n < 2) {
        static const char *const f[2] = {"SUPER_LURE", "GREAT_BALL"};
        shop(run, NULL, 0, f, 2);
    } else if (n < 4) {
        static const char *const f[3] = {"QUICK_CLAW", "MAX_LURE", "ULTRA_BALL"};
        shop(run, NULL, 0, f, 3);
    } else if (n < 6) {
        static const char *const f[3] = {"GRIP_CLAW", "MAX_LURE", "ROGUE_BALL"};
        shop(run, NULL, 0, f, 3);
    } else {
        pr_reward_ctx *ctx = gc_reward_ctx(run);
        const pr_rng saved = run->run.rng;
        run->run.rng = pr_run_seed_offset_rng(&run->run, run->wave * 1000 + 512, NULL);
        generated_option(ctx, "MASTER_BALL", &opts[n_opts++]);
        if (!gc_item_stack(run, "MEGA_BRACELET")) generated_option(ctx, "MEGA_BRACELET", &opts[n_opts++]);
        if (!gc_item_stack(run, "DYNAMAX_BAND")) generated_option(ctx, "DYNAMAX_BAND", &opts[n_opts++]);
        pr_reward_option special[4];
        int n_special = 0;
        static const char *const keys[4] = {"EVOLUTION_ITEM", "RARE_EVOLUTION_ITEM", "FORM_CHANGE_ITEM", "RARE_FORM_CHANGE_ITEM"};
        for (int k = 0; k < 4; k++) if (generated_option(ctx, keys[k], &special[n_special])) n_special++;
        if (n_special > 0) opts[n_opts++] = special[pr_rng_seed_int(&run->run.rng, n_special, 0)];
        gc_superfan_pre_rng = run->run.rng;
        run->run.rng = saved;
        shop(run, opts, n_opts, NULL, 0);
    }
}


/* the encounter's species pools (getRandomPartyMemberFunc's speciesPool) */
static const uint16_t POOL1[23] = {SPECIES_ID_PARASECT, SPECIES_ID_VENOMOTH, SPECIES_ID_LEDIAN, SPECIES_ID_ARIADOS, SPECIES_ID_YANMA, SPECIES_ID_BEAUTIFLY, SPECIES_ID_DUSTOX, SPECIES_ID_MASQUERAIN, SPECIES_ID_NINJASK, SPECIES_ID_VOLBEAT, SPECIES_ID_ILLUMISE, SPECIES_ID_ANORITH, SPECIES_ID_KRICKETUNE, SPECIES_ID_WORMADAM, SPECIES_ID_MOTHIM, SPECIES_ID_SKORUPI, SPECIES_ID_JOLTIK, SPECIES_ID_LARVESTA, SPECIES_ID_VIVILLON, SPECIES_ID_CHARJABUG, SPECIES_ID_RIBOMBEE, SPECIES_ID_SPIDOPS, SPECIES_ID_LOKIX};
static const uint16_t POOL2[23] = {SPECIES_ID_SCYTHER, SPECIES_ID_PINSIR, SPECIES_ID_HERACROSS, SPECIES_ID_FORRETRESS, SPECIES_ID_SCIZOR, SPECIES_ID_SHUCKLE, SPECIES_ID_SHEDINJA, SPECIES_ID_ARMALDO, SPECIES_ID_VESPIQUEN, SPECIES_ID_DRAPION, SPECIES_ID_YANMEGA, SPECIES_ID_LEAVANNY, SPECIES_ID_SCOLIPEDE, SPECIES_ID_CRUSTLE, SPECIES_ID_ESCAVALIER, SPECIES_ID_ACCELGOR, SPECIES_ID_GALVANTULA, SPECIES_ID_VIKAVOLT, SPECIES_ID_ARAQUANID, SPECIES_ID_ORBEETLE, SPECIES_ID_CENTISKORCH, SPECIES_ID_FROSMOTH, SPECIES_ID_KLEAVOR};
static const uint16_t POOL4[4] = {SPECIES_ID_GENESECT, SPECIES_ID_SLITHER_WING, SPECIES_ID_BUZZWOLE, SPECIES_ID_PHEROMOSA};
typedef struct { uint16_t species; uint8_t form; } pool3_t;   /* form 0: formIndex undefined */
static const pool3_t POOL3[8] = {
    {SPECIES_ID_PINSIR, 1}, {SPECIES_ID_SCIZOR, 1}, {SPECIES_ID_HERACROSS, 1}, {SPECIES_ID_ORBEETLE, 1},
    {SPECIES_ID_CENTISKORCH, 1}, {SPECIES_ID_DURANT, 0}, {SPECIES_ID_VOLCARONA, 0}, {SPECIES_ID_GOLISOPOD, 0}};

/* the Bug-type tutor's moves: a physical, a special, a status and a misc one are drawn */
static const uint16_t TUTOR[4][5] = {
    {MOVE_ID_MEGAHORN, MOVE_ID_ATTACK_ORDER, MOVE_ID_BUG_BITE, MOVE_ID_FIRST_IMPRESSION, MOVE_ID_LUNGE},
    {MOVE_ID_SILVER_WIND, MOVE_ID_SIGNAL_BEAM, MOVE_ID_BUG_BUZZ, MOVE_ID_POLLEN_PUFF, MOVE_ID_STRUGGLE_BUG},
    {MOVE_ID_STRING_SHOT, MOVE_ID_DEFEND_ORDER, MOVE_ID_RAGE_POWDER, MOVE_ID_STICKY_WEB, MOVE_ID_SILK_TRAP},
    {MOVE_ID_LEECH_LIFE, MOVE_ID_U_TURN, MOVE_ID_HEAL_ORDER, MOVE_ID_QUIVER_DANCE, MOVE_ID_INFESTATION}};

/* WAVE_LEVEL_BREAKPOINTS */
static const int BREAKPOINT[7] = {30, 50, 70, 100, 120, 140, 160};

enum { M_BEEDRILL, M_BUTTERFREE, M_POOL1, M_POOL2, M_POOL3A, M_POOL3B, M_POOL4 };
typedef struct { uint8_t kind; bool form, boss; } sf_slot;
/* the team by band of the wave (getTrainerConfigForWave); the last band's and the one before's 5th member are STRONG (the compound template) */
static const struct { uint8_t size, n_strong; sf_slot m[5]; } BAND[8] = {
    {2, 0, {{M_BEEDRILL, false, false}, {M_BUTTERFREE, false, false}}},
    {3, 0, {{M_BEEDRILL, false, false}, {M_BUTTERFREE, false, false}, {M_POOL1, false, false}}},
    {4, 0, {{M_BEEDRILL, false, false}, {M_BUTTERFREE, false, false}, {M_POOL1, false, false}, {M_POOL2, false, false}}},
    {5, 0, {{M_BEEDRILL, false, false}, {M_BUTTERFREE, false, false}, {M_POOL1, false, false}, {M_POOL2, false, false}, {M_POOL2, false, false}}},
    {5, 0, {{M_BEEDRILL, true, false}, {M_BUTTERFREE, true, false}, {M_POOL2, false, false}, {M_POOL2, false, false}, {M_POOL3A, false, false}}},
    {5, 0, {{M_BEEDRILL, true, false}, {M_BUTTERFREE, true, false}, {M_POOL2, false, false}, {M_POOL3A, false, false}, {M_POOL3B, false, false}}},
    {5, 1, {{M_BEEDRILL, true, false}, {M_BUTTERFREE, true, false}, {M_POOL2, false, false}, {M_POOL3A, false, false}, {M_POOL4, false, false}}},
    {5, 1, {{M_BEEDRILL, true, true}, {M_BUTTERFREE, true, true}, {M_POOL3A, false, false}, {M_POOL3B, false, false}, {M_POOL4, false, false}}},
};

static int band_of(int wave)
{
    int b = 0;
    while (b < 7 && wave >= BREAKPOINT[b]) b++;
    return b;
}

/* randSeedItem: one item needs no roll */
static int seed_item(pr_rng *r, int n) { return n == 1 ? 0 : pr_rng_integer_in_range(r, 0, n - 1); }

/* onInit's randSeedShuffle(POOL_3_POKEMON.slice()), on the wave's stream: pop() takes the last item, the next pop the one before */
static void pool3_picks(const gc_run *run, pool3_t out[2])
{
    pr_rng r = pr_run_seed_offset_rng((pr_run *)&run->run, run->wave, NULL);
    pool3_t a[8];
    memcpy(a, POOL3, sizeof a);
    for (int i = 7; i > 0; i--) {
        const int j = pr_rng_integer_in_range(&r, 0, i);
        const pool3_t t = a[i]; a[i] = a[j]; a[j] = t;
    }
    out[0] = a[7];
    out[1] = a[6];
}

int gc_superfan_band(int wave) { return band_of(wave); }
void gc_superfan_pool3(const gc_run *run, int species[2], int form[2])
{
    pool3_t p[2];
    pool3_picks(run, p);
    for (int k = 0; k < 2; k++) { species[k] = p[k].species; form[k] = p[k].form; }
}

int gc_superfan_generated[5][4];   /* each member's moveset as the constructor made it, before the postProcess (for the differential) */

/* one member: genPartyMember's genMon on the member's own stream: randSeedItem of the pool, addEnemyPokemon (ignoreEvolution), then the postProcess */
static bool make_member(gc_run *run, int e, const sf_slot *slot, const pool3_t *p3, int level, gc_mon *out)
{
    int species;
    switch (slot->kind) {
    case M_BEEDRILL: species = SPECIES_ID_BEEDRILL; break;
    case M_BUTTERFREE: species = SPECIES_ID_BUTTERFREE; break;
    case M_POOL1: species = POOL1[seed_item(&run->run.rng, 23)]; break;
    case M_POOL2: species = POOL2[seed_item(&run->run.rng, 23)]; break;
    case M_POOL4: species = POOL4[seed_item(&run->run.rng, 4)]; break;
    default: species = p3->species; break;
    }
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
    const bool made = gc_generate_moveset(run, &run->run.rng, out);
    if (!made) gc_port_enemy_moveset(run, out);
    for (int k = 0; k < 4; k++) g.moveset[k] = k < out->b.n_moves ? out->b.moves[k] : 0;
    if (e >= 0 && e < 5) for (int k = 0; k < 4; k++) gc_superfan_generated[e][k] = g.moveset[k];
    if (pr_run_enemy_pokemon_finish(&run->run, &ec, &g) < 0) return false;
    /* the postProcess: setBoss(true, 2); formIndex; generateAndPopulateMoveset() (the form's own moves, on the same stream) */
    int form = 0;
    if (slot->kind == M_POOL3A || slot->kind == M_POOL3B) form = p3->form;
    else if (slot->form) form = 1;
    if (slot->boss) g.boss_segments = 2;
    if (form) {
        g.form = (uint8_t)form;
        if (made && gc_member_regen(run, out, species, level, ec.trainer_slot, &g, &run->run.rng) < 0) return false;
    }
    gc_refresh_enemy(run, out, species, level, &g, ec.trainer_slot);
    return true;
}

static void build_template(int band, pr_party_template *t)
{
    memset(t, 0, sizeof *t);
    t->size = (int8_t)BAND[band].size;
    t->strength = PARTY_MEMBER_STRENGTH_AVERAGE;
    for (int i = 0; i < t->size; i++) {
        const bool strong = i >= t->size - BAND[band].n_strong;
        t->strengths[i] = strong ? PARTY_MEMBER_STRENGTH_STRONG : PARTY_MEMBER_STRENGTH_AVERAGE;
        t->offsets[i] = (int8_t)(strong ? t->size - BAND[band].n_strong : 0);
    }
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

/* option 1's onOptionPhase: the tutor's four moves, setEncounterRewards({fillRemaining: true}), initBattleWithEnemyConfig(the Superfan's team) */
static void fight(gc_run *run)
{
    gc_internal *in = gc_int(run);
    const int band = band_of(run->wave);
    pool3_t p3[2];
    pool3_picks(run, p3);

    /* onOptionPhase: the four Bug-type moves, on the option's stream */
    pr_rng opt = gc_me_option_rng(run);
    for (int k = 0; k < 4; k++) in->superfan.tutor[k] = TUTOR[k][pr_rng_seed_int(&opt, 5, 0)];
    /* everything else is initBattleWithEnemyConfig, after the first await: outside the option's seed offset, on the run's own RNG */

    pr_party_template tpl;
    build_template(band, &tpl);
    uint8_t strengths[8];
    for (int i = 0; i < tpl.size && i < 8; i++) strengths[i] = (uint8_t)tpl.strengths[i];
    int levels[PR_MAX_PARTY_TEMPLATE];
    const int n = pr_run_get_party_levels(&run->run, run->wave, tpl.size, strengths, tpl.strength, false, levels);

    const int trainer = TRAINER_TYPE_BUG_TYPE_SUPERFAN;
    run->trainer_type = trainer;   /* the trainer exists before its Pokemon do (the moveset reads its class) */
    run->battle_type = BATTLE_TYPE_TRAINER;
    run->double_battle = false;
    static gc_mon made[5];
    bool ok = n == tpl.size;
    const pr_rng saved = run->run.rng;
    for (int e = 0; ok && e < n; e++) {
        /* seedOffset: waveIndex + (derivedType << 10) + ((index + 1) << 8) */
        run->run.rng = pr_run_seed_offset_rng(&run->run, run->wave + (trainer << 10) + ((e + 1) << 8), NULL);
        const sf_slot *slot = &BAND[band].m[e];
        ok = make_member(run, e, slot, slot->kind == M_POOL3B ? &p3[1] : &p3[0], levels[e], &made[e]);
    }
    run->run.rng = saved;
    if (!ok) {   /* a species or a trainer run-core cannot make: nobody fights and the encounter ends */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        run->battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER;
        run->n_enemy = 0;
        run->trainer_type = run->trainer_variant = -1;
        gc_me_leave_with_heal_shop(run);
        return;
    }
    run->battle_type = BATTLE_TYPE_TRAINER;
    run->double_battle = false;
    run->trainer_type = trainer;
    run->trainer_variant = TRAINER_VARIANT_FEMALE;   /* the party config's female: true */
    run->trainer_name = run->trainer_partner_name = -1;
    snprintf(in->custom_trainer_name, sizeof in->custom_trainer_name, "$t(trainerNames:bugTypeSuperfan)");   /* config.name */
    in->custom_team_on = false;
    run->n_enemy = n;
    for (int e = 0; e < n; e++) run->enemy[e] = made[e];
    run->me_battle = true;
    in->me_no_stats = false;   /* the encounter does not prevent the game stats' updates */
    gc_enemy_finish_party_me(run);   /* no modifierConfigs: the ordinary item rolls */
    in->me_tera_static = 0;

    in->superfan.tutor_pending = true;   /* encounter.onRewards = doBugTypeMoveTutor */
    in->me_post.pending = true;   /* the outro, after the shop */
    in->me_post.a = GC_ME_BUG_TYPE_SUPERFAN;
    in->me_post.b = in->me_post.c = 0;
    in->me_reward.active = true;   /* setEncounterRewards({fillRemaining: true}) */
    in->me_reward.has_item = false;
    in->me_reward.func = -1;
    in->me_reward.fill = true;
    in->me_reward.before = true;   /* the callback before the rewards */
    in->me_reward.n_more = in->me_reward.n_tiers = 0;

    run->me_catch_allowed = false;
    run->mystery_type = run->mystery_tier = -1;
    run->in_battle = true;
    say_trainer_fight(run, in);
}

int gc_superfan_tutor_move(const gc_run *run, int k)
{
    return k >= 0 && k < 4 ? gc_int((gc_run *)run)->superfan.tutor[k] : 0;
}

/* doBugTypeMoveTutor: the battleWon dialogue, then selectOptionThenPokemon(the four moves, teachMovePrompt): a move, then the Pokemon (the cancel button teaches nothing) */
bool gc_superfan_tutor_start(gc_run *run)
{
    gc_internal *in = gc_int(run);
    if (!in->superfan.tutor_pending) return false;
    in->superfan.tutor_pending = false;
    gc_ev_msg(run, "mysteryEncounters/bugTypeSuperfan:battleWon", 0);
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_MYSTERY;
    s->mystery_type = GC_ME_BUG_TYPE_SUPERFAN;
    s->mystery_tier = 0;
    s->me_stage = GC_ME_STAGE_TUTOR;
    s->n_options = 5;
    for (int k = 0; k < 5; k++) s->option_ok[k] = true;
    for (int k = 0; k < 4; k++) s->option_pick[k] = GC_PICK_MEMBER;
    return true;
}

/* a = the move (0..3) or 4 for the cancel, b = the Pokemon: LearnMovePhase(selectedPokemonIndex, moveId) is unshifted */
bool gc_superfan_tutor_choose(gc_run *run, const gc_choice *c, const char **why, bool *ok)
{
    if (c->kind != GC_CH_MYSTERY) { *why = "choose a move to teach"; return false; }
    if (c->a < 0 || c->a > 4) { *why = "no such move"; return false; }
    if (c->a < 4 && (c->b < 0 || c->b >= run->n_party)) { *why = "no such Pokemon"; return false; }
    *ok = true;
    memset(&run->screen, 0, sizeof run->screen);
    if (c->a < 4) gc_rq_unshift(run, gc_rp(RP_LEARN_MOVE, c->b, gc_int(run)->superfan.tutor[c->a], 0));
    return true;
}

void gc_superfan_outro(gc_run *run)
{
    gc_ev_msg(run, "mysteryEncounters/bugTypeSuperfan:outro", 0);
}

bool gc_superfan_execute(gc_run *run, int option, int b, int c, const char **why)
{
    if (option == 0) { fight(run); return true; }
    if (option == 1) {
        if (gc_superfan_num_bug_types(run) <= 0) { *why = "mysteryEncounters/bugTypeSuperfan:option.2.disabledTooltip"; return false; }
        show_bug_types(run);
        return true;
    }
    if (option != 2) { *why = "no such option"; return false; }
    int held[GC_MAX_ITEMS];
    const int n = gc_superfan_held_items(run, b, held);
    if (!gc_superfan_pick_ok(run, b) || c < 0 || c >= n) { *why = "mysteryEncounters/bugTypeSuperfan:option.3.invalidSelection"; return false; }
    /* chosenPokemon.loseHeldItem(modifier, false): one stack, the item with its last; then updateModifiers(true, true) */
    const gc_item chosen = run->items[held[c]];
    for (int i = 0; i < run->n_items; i++) {
        gc_item *it = &run->items[i];
        if (strcmp(it->key, chosen.key) || it->arg != chosen.arg || it->pokemon_id != chosen.pokemon_id) continue;
        if (--it->stack <= 0) gc_item_remove_at(run, i);
        break;
    }
    gc_items_to_battle(run);
    gc_me_prune_items(run);
    gc_me_update_modifiers(run);
    pr_reward_option net = pr_reward_pregen_option(gc_reward_ctx(run), gc_reward_item_index("MYSTERY_ENCOUNTER_GOLDEN_BUG_NET"), -1);
    net.tier = 3;   /* bugNet.type.tier = ModifierTier.ROGUE */
    static const char *const f[1] = {"REVIVER_SEED"};
    shop(run, &net, 1, f, 1);
    return true;
}
