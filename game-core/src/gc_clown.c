/* Clowning Around (src/data/mystery-encounters/encounters/clowning-around-encounter.ts, upstream 8555c08).
 *
 * onInit (the wave's stream): the ability Blacephalon will have, RANDOM_ABILITY_POOL[randSeedInt(15)], then its two types: firstType = randSeedInt(18),
 * secondType = randSeedInt(17), plus one when it is not below firstType (never the same type twice). The dialogue tokens are the ability's name and
 * Blacephalon's name.
 *
 * Option 2 (Remain Unprovoked), onPreOptionPhase (executeWithSeedOffset(.., getSeedOffset())): the Pokemon holding the most transferable non-berry stacks
 * (the first one of the party on a tie) has its Berries taken and as many made again from the berry pool, then its Ultra and Rogue items (Lucky Egg and Soothe
 * Bell counting as Ultra, Golden Egg as Rogue) taken and as many made from the Ultra and Rogue pools. Every pool entry is drawn at most as often as the item
 * can stack (a pool entry is dropped at its limit). A Pokemon already at an item's maximum stack gets nothing for that draw (applyModifierTypeToPlayerPokemon).
 * The option then leaves with the heal shop.
 *
 * Option 3 (Return the Insults), onPreOptionPhase: every Pokemon of the party gets a new second type (the first slot is kept, null in customPokemonData.types):
 * a type of one of its attacking moves that it does not have yet when it has one (Set, sorted as strings - 0, 1, 10, 11 ... 17, 2, 3 ... 9 - and shuffled), else a random
 * regular type it does not have. A fused Pokemon's fusion half gets the same types. The option then leaves with the heal shop.
 *
 * Option 1 (Battle the Clown): setEncounterRewards({fillRemaining: true}), the three start-of-battle effects (Mr. Mime's Role Play on Blacephalon, then Taunt on both of the
 * player's Pokemon), then `await transitionMysteryEncounterIntroVisuals()`: everything after it runs on the RUN's RNG. initBattleWithEnemyConfig with the Harlequin trainer
 * (a double-only clone with a STRONG, STRONGER party): the female roll (drawn although a double trainer never uses it), the Trainer's name (randSeedItem of the class's names), the
 * levels, and the two configured members, addEnemyPokemon(species, level, TRAINER, boss): Mr. Mime and Blacephalon (the rolled ability and types in customPokemonData), their
 * constructor movesets (generated, then replaced by the config's), two boss bars each, the wild pool's items. The win is handleMysteryEncounterVictory for a trainer battle:
 * BattleEndPhase, TrainerVictoryPhase, then the rewards: the shop that fills itself. After the shop, onPostOptionPhase offers to teach one Pokemon the rolled ability for good
 * (a Yes/No, then a party pick that can be backed out of); the outro of all three options follows.
 */
#include "internal.h"
#include <stdio.h>
#include <string.h>

extern int pr_reward_generate(pr_reward_ctx *ctx, int item);   /* reward-core/src/reward_internal.h: generateType of a generator item; below 0 it makes nothing */

static const uint16_t ABILITY_POOL[15] = {
    ABILITY_ID_STURDY, ABILITY_ID_PICKUP, ABILITY_ID_INTIMIDATE, ABILITY_ID_GUTS, ABILITY_ID_DROUGHT, ABILITY_ID_DRIZZLE, ABILITY_ID_SNOW_WARNING,
    ABILITY_ID_SAND_STREAM, ABILITY_ID_ELECTRIC_SURGE, ABILITY_ID_PSYCHIC_SURGE, ABILITY_ID_GRASSY_SURGE, ABILITY_ID_MISTY_SURGE, ABILITY_ID_MAGICIAN,
    ABILITY_ID_SHEER_FORCE, ABILITY_ID_PRANKSTER,
};

/* onInit on the wave's stream (a local copy: asking draws nothing from the run) */
void gc_clown_roll(const gc_run *run, int *ability, int *first_type, int *second_type)
{
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave, NULL);
    const int a = pr_rng_seed_int(&r, 15, 0);
    const int first = pr_rng_seed_int(&r, 18, 0);
    int second = pr_rng_seed_int(&r, 17, 0);
    if (second >= first) second++;
    if (ability) *ability = ABILITY_POOL[a];
    if (first_type) *first_type = first;
    if (second_type) *second_type = second;
}

enum { TIER_ULTRA = 2, TIER_ROGUE = 3 };
typedef struct { const char *key; int16_t arg; int left; } pool_entry;

/* applyModifierTypeToPlayerPokemon: a Pokemon at the item's maximum stack of it gets nothing */
static bool give(gc_run *run, const gc_mon *to, const char *key, int16_t arg)
{
    int held = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (!strcmp(it->key, key) && it->arg == arg && it->pokemon_id == to->b.id) held += it->stack + it->virtual_stack;
    }
    if (held && held >= gc_item_max_held(run, key, arg, to)) return false;
    gc_item_add(run, key, arg, to->b.id, 1);
    return true;
}

/* generateItemsOfTier: `n` draws from a pool whose entries are removed at their stack limit, on the stream `r` (run->run.rng while a generator runs) */
static int generate_tier(gc_run *run, pr_rng *r, const gc_mon *to, pool_entry *pool, int len, int n)
{
    int added = 0;
    for (int i = 0; i < n; i++) {
        if (len == 0) return added;   /* out of items to spawn */
        const int idx = pr_rng_seed_int(r, len, 0);
        int16_t arg = pool[idx].arg;
        bool made = true;
        if (!strcmp(pool[idx].key, "ATTACK_TYPE_BOOSTER")) {
            /* generateModifierType(ATTACK_TYPE_BOOSTER): the generator picks the type, on the same stream */
            const pr_rng saved = run->run.rng;
            run->run.rng = *r;
            const int type = pr_reward_generate(gc_reward_ctx(run), PR_ITEM_ATTACK_TYPE_BOOSTER);
            *r = run->run.rng;
            run->run.rng = saved;
            made = type >= 0;   /* null (no attacking move in the party): upstream's apply throws, the draw still counts */
            arg = (int16_t)type;
        }
        if (made && give(run, to, pool[idx].key, arg)) added++;
        if (--pool[idx].left <= 0) {
            memmove(&pool[idx], &pool[idx + 1], (size_t)(len - idx - 1) * sizeof pool[0]);
            len--;
        }
    }
    return added;
}

static int transferable_stacks(const gc_run *run, const gc_mon *m)
{
    int n = 0;
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (it->pokemon_id == m->b.id && gc_gts_item_transferable(it) && strcmp(it->key, "BERRY")) n += it->stack;
    }
    return n;
}

/* option 2 */
static void shuffle_items(gc_run *run)
{
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave * 1000 + 512, NULL);
    const gc_mon *best = &run->party[run->party_order[0]];
    int count = transferable_stacks(run, best);
    for (int k = 0; k < run->n_party; k++) {
        const gc_mon *m = &run->party[run->party_order[k]];
        const int next = transferable_stacks(run, m);
        if (next > count) { best = m; count = next; }
    }
    const pr_msg_arg holder = pr_arg_pokemon_name(&run->bt, "switchPokemon", &best->b);
    const uint32_t id = best->b.id;

    /* the Berries: all of them taken, as many made again */
    int n_berries = 0;
    for (int i = 0; i < run->n_items;) {
        if (run->items[i].pokemon_id == id && !strcmp(run->items[i].key, "BERRY")) { n_berries += run->items[i].stack; gc_item_remove_at(run, i); }
        else i++;
    }
    pool_entry berries[11] = {
        {"BERRY", BERRY_TYPE_APICOT, 3}, {"BERRY", BERRY_TYPE_ENIGMA, 2}, {"BERRY", BERRY_TYPE_GANLON, 3}, {"BERRY", BERRY_TYPE_LANSAT, 3},
        {"BERRY", BERRY_TYPE_LEPPA, 2}, {"BERRY", BERRY_TYPE_LIECHI, 3}, {"BERRY", BERRY_TYPE_LUM, 2}, {"BERRY", BERRY_TYPE_PETAYA, 3},
        {"BERRY", BERRY_TYPE_SALAC, 2}, {"BERRY", BERRY_TYPE_SITRUS, 2}, {"BERRY", BERRY_TYPE_STARF, 3},
    };
    int added = generate_tier(run, &r, best, berries, 11, n_berries);

    /* the Ultra and Rogue items, taken one entry at a time (the tier of an entry is read with the earlier ones already gone) */
    int n_ultra = 0, n_rogue = 0;
    for (int i = 0; i < run->n_items;) {
        const gc_item *it = &run->items[i];
        if (it->pokemon_id != id || !gc_gts_item_transferable(it) || !strcmp(it->key, "BERRY")) { i++; continue; }
        int tier = gc_item_pool_tier(run, it);
        if (tier < 0) tier = TIER_ULTRA;   /* type.tier ?? ModifierTier.ULTRA */
        const int stack = it->stack;
        if (!strcmp(it->key, "GOLDEN_EGG") || tier == TIER_ROGUE) { n_rogue += stack; gc_item_remove_at(run, i); }
        else if (!strcmp(it->key, "LUCKY_EGG") || !strcmp(it->key, "SOOTHE_BELL") || tier == TIER_ULTRA) { n_ultra += stack; gc_item_remove_at(run, i); }
        else i++;
    }
    pool_entry ultra[5] = {
        {"REVIVER_SEED", -1, 1}, {"GOLDEN_PUNCH", -1, 5}, {"ATTACK_TYPE_BOOSTER", -1, 99}, {"QUICK_CLAW", -1, 3}, {"WIDE_LENS", -1, 3},
    };
    pool_entry rogue[8] = {
        {"LEFTOVERS", -1, 4}, {"SHELL_BELL", -1, 4}, {"SOUL_DEW", -1, 10}, {"SCOPE_LENS", -1, 1}, {"BATON", -1, 1}, {"FOCUS_BAND", -1, 5},
        {"KINGS_ROCK", -1, 3}, {"GRIP_CLAW", -1, 5},
    };
    added += generate_tier(run, &r, best, ultra, 5, n_ultra);
    added += generate_tier(run, &r, best, rogue, 8, n_rogue);
    /* removeModifier never runs updateModifiers; every addModifier(.., false, false, false, true) that adds something does (it prunes, e.g., an evolution
     * tracker whose holder cannot use it), so a shuffle that adds nothing leaves the list as the removals left it */
    if (added) {
        gc_me_prune_items(run);
        gc_me_update_modifiers(run);
    } else {
        gc_items_to_battle(run);
    }

    const pr_msg_arg blacephalon = pr_arg_key("blacephalonName", "pokemon:blacephalon");
    gc_ev_msg(run, "mysteryEncounters/clowningAround:option.2.selected2", 2, blacephalon, holder);
    gc_ev_msg(run, "mysteryEncounters/clowningAround:option.2.selected3", 0);
}

static bool has_type(const int8_t *types, int n, int t)
{
    for (int i = 0; i < n; i++) if (types[i] == t) return true;
    return false;
}

/* [...new Set(types)].sort(): Array.prototype.sort compares numbers as strings */
static bool string_before(int a, int b)
{
    char x[4], y[4];
    snprintf(x, sizeof x, "%d", a);
    snprintf(y, sizeof y, "%d", b);
    return strcmp(x, y) < 0;
}

/* option 3 */
static void shuffle_types(gc_run *run)
{
    pr_rng r = pr_run_seed_offset_rng(&run->run, run->wave * 1000 + 512, NULL);
    for (int k = 0; k < run->n_party; k++) {
        gc_mon *m = &run->party[run->party_order[k]];
        int8_t original[PR_MAX_TYPES];
        const int n_original = pr_get_types(&m->b, original, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);

        /* the types of its attacks that are not its own (a Set, in order of first appearance) */
        int priority[4], n_priority = 0;
        for (int j = 0; j < m->b.n_moves && j < 4; j++) {
            const int mv = m->b.moves[j];
            if (mv <= 0 || mv >= PR_MOVE_COUNT) continue;
            const int t = pr_moves[mv].type;
            if (pr_moves[mv].category == MOVE_CATEGORY_STATUS || has_type(original, n_original, t)) continue;
            bool seen = false;
            for (int p = 0; p < n_priority; p++) seen = seen || priority[p] == t;
            if (!seen) priority[n_priority++] = t;
        }
        if (n_priority > 0) {
            for (int a = 1; a < n_priority; a++)
                for (int b = a; b > 0 && string_before(priority[b], priority[b - 1]); b--) {
                    const int t = priority[b]; priority[b] = priority[b - 1]; priority[b - 1] = t;
                }
            for (int i = n_priority - 1; i > 0; i--) {   /* randSeedShuffle: integerInRange(0, i) */
                const int j = pr_rng_seed_int(&r, i + 1, 0);
                const int t = priority[i]; priority[i] = priority[j]; priority[j] = t;
            }
        }
        int second = -1;
        while (second < 0 || has_type(original, n_original, second)) {
            if (n_priority > 0) second = priority[--n_priority];
            else second = pr_rng_seed_int(&r, 18, 0);   /* getRandomRegularPokemonType: randSeedIntRange(NORMAL, FAIRY) */
        }
        m->b.custom_types[0] = 0;
        m->b.custom_types[1] = (uint8_t)(second + 1);
        if (m->b.fusion_species) {
            m->b.fusion_custom_types[0] = 0;
            m->b.fusion_custom_types[1] = (uint8_t)(second + 1);
        }
    }
    gc_items_to_battle(run);
    const pr_msg_arg blacephalon = pr_arg_key("blacephalonName", "pokemon:blacephalon");
    gc_ev_msg(run, "mysteryEncounters/clowningAround:option.3.selected2", 1, blacephalon);
    gc_ev_msg(run, "mysteryEncounters/clowningAround:option.3.selected3", 0);
}

static void set_moves(gc_mon *m, const int *mv)
{
    memset(m->b.moves, 0, sizeof m->b.moves);
    memset(m->b.pp_used, 0, sizeof m->b.pp_used);
    memset(m->b.pp_up, 0, sizeof m->b.pp_up);
    m->b.n_moves = 0;
    for (int k = 0; k < 4; k++) { m->b.max_pp_override[k] = -1; gc_mon_set_move(m, k, mv[k]); }
}

/* option 1: initBattleWithEnemyConfig (false: a species or a trainer run-core cannot make) */
static bool build_fight(gc_run *run, int ability, int first, int second)
{
    gc_internal *in = gc_int(run);
    const int t = TRAINER_TYPE_HARLEQUIN;
    const pr_trainer_info *info = gc_trainer_info_of(t);
    if (!info) return false;
    (void)pr_rng_seed_int(&run->run.rng, 2, 0);   /* partyConfig.female == null: !!randSeedInt(2), although a double-only trainer is never female */
    run->me_battle = true;   /* the encounter's battle: its bosses are made as bosses by the constructor */
    run->trainer_type = t;   /* the trainer exists before its Pokemon do (the moveset reads its class) */
    run->battle_type = BATTLE_TYPE_TRAINER;
    run->double_battle = true;   /* trainerConfig.doubleOnly */
    run->trainer_variant = TRAINER_VARIANT_DOUBLE;
    /* new Trainer(..): getRandomLocaleEntry("trainersCommon:harlequin"); a double-only trainer splits "A & B" for its partner, and these names have no " & " */
    run->trainer_name = run->trainer_partner_name = -1;
    in->custom_trainer_name[0] = 0;
    in->custom_team_on = false;
    if (info->names > 0) run->trainer_name = info->names > 1 ? pr_rng_seed_int(&run->run.rng, info->names, 0) : 0;

    /* getPartyLevels: TrainerPartyCompoundTemplate(TrainerPartyTemplate(1, STRONG), TrainerPartyTemplate(1, STRONGER)); levelAdditiveModifier is unset: no additive */
    static const uint8_t strengths[2] = {4, 5};   /* STRONG, STRONGER */
    int levels[PR_MAX_PARTY_TEMPLATE];
    if (pr_run_get_party_levels(&run->run, run->wave, 2, strengths, 4, true, levels) != 2) return false;

    static gc_mon made[2];
    pr_rng r = run->run.rng;
    for (int e = 0; e < 2; e++) {
        const int species = e ? SPECIES_ID_BLACEPHALON : SPECIES_ID_MR_MIME;
        if (!gc_me_new_enemy_slot(run, &r, species, levels[e], true, true, TRAINER_SLOT_TRAINER, &made[e])) return false;
        for (int k = 0; k < 4; k++) in->ctor_moves[e][k] = k < made[e].b.n_moves ? made[e].b.moves[k] : 0;
    }
    run->run.rng = r;

    /* the configs, after each Pokemon is made: full HP, no status, no passive, the boss bars, the custom data, the moves */
    static const int mime[4] = {MOVE_ID_TEETER_DANCE, MOVE_ID_ALLY_SWITCH, MOVE_ID_DAZZLING_GLEAM, MOVE_ID_PSYCHIC};
    static const int blacephalon[4] = {MOVE_ID_TRICK, MOVE_ID_HYPNOSIS, MOVE_ID_SHADOW_BALL, MOVE_ID_MIND_BLOWN};
    for (int e = 0; e < 2; e++) {
        gc_mon *m = &made[e];
        m->b.hp = m->b.stats[STAT_HP];
        m->b.status = STATUS_EFFECT_NONE;
        m->b.passive_flag = false;
        if (e) {
            m->b.custom_ability = (int16_t)ability;
            m->b.ability = m->b.base_ability = (uint16_t)ability;
            m->b.custom_types[0] = (uint8_t)(first + 1);
            m->b.custom_types[1] = (uint8_t)(second + 1);
        }
        set_moves(m, e ? blacephalon : mime);
    }
    run->n_enemy = 2;
    run->enemy[0] = made[0];
    run->enemy[1] = made[1];
    return true;
}

static void fight(gc_run *run)
{
    gc_internal *in = gc_int(run);
    int ability, first, second;
    gc_clown_roll(run, &ability, &first, &second);
    in->me_post.pending = true;   /* the outro and the ability swap, after the shop */
    in->me_post.a = GC_ME_CLOWNING_AROUND;
    in->me_post.b = 0;
    in->me_post.c = ability;
    /* setEncounterRewards({fillRemaining: true}) and the start-of-battle effects come before the await */
    gc_me_add_start_effect(run, 2, 3, MOVE_ID_ROLE_PLAY);   /* ENEMY -> ENEMY_2 */
    gc_me_add_start_effect(run, 3, 0, MOVE_ID_TAUNT);       /* ENEMY_2 -> PLAYER */
    gc_me_add_start_effect(run, 3, 1, MOVE_ID_TAUNT);       /* ENEMY_2 -> PLAYER_2 */
    if (!build_fight(run, ability, first, second)) {   /* nobody fights: the encounter ends as a refusal would */
        gc_stub(run, GC_STUB_UNPORTED_ENEMY);
        run->me_battle = false;
        run->battle_type = BATTLE_TYPE_MYSTERY_ENCOUNTER;
        run->double_battle = false;
        run->n_enemy = 0;
        run->trainer_type = run->trainer_variant = -1;
        in->n_me_start = 0;
        gc_me_leave_with_heal_shop(run);
        return;
    }
    run->me_battle = true;
    gc_enemy_finish_party_me(run);   /* no modifierConfigs: the ordinary item rolls (the wild pool: the encounter's battle keeps BattleType.MYSTERY_ENCOUNTER) */
    run->me_catch_allowed = false;
    in->me_reward.active = false;
    in->me_no_stats = false;
    run->mystery_type = run->mystery_tier = -1;
    run->in_battle = true;
    in->clown.active = true;
    gc_me_say_trainer_fight(run, in);
}

/* handleMysteryEncounterVictory (trainer battle): BattleEndPhase, TrainerVictoryPhase, then setEncounterRewards({fillRemaining: true})'s shop */
void gc_clown_victory(gc_run *run)
{
    gc_internal *in = gc_int(run);
    in->clown.active = false;
    gc_rq_push(run, gc_rp(RP_BATTLE_END, 1, 0, 0));
    gc_rq_push(run, gc_rp(RP_TRAINER_VICTORY, 0, 0, 0));
    pr_reward_custom *c = &in->reward.me_custom;
    memset(c, 0, sizeof *c);
    c->fill_remaining = true;
    c->reroll_multiplier = NAN;
    c->allow_luck_upgrades = true;
    gc_me_end_with_shop(run);
}

/* options 2 and 3 leave with the heal shop; option 1 is the fight */
void gc_clown_execute(gc_run *run, int option)
{
    gc_internal *in = gc_int(run);
    if (option == 0) { fight(run); return; }
    in->me_post.pending = true;   /* the outro */
    in->me_post.a = GC_ME_CLOWNING_AROUND;
    in->me_post.b = option;
    in->me_post.c = 0;
    if (option == 1) shuffle_items(run);
    else shuffle_types(run);
    gc_me_leave_with_heal_shop(run);   /* leaveEncounterWithoutBattle(true) */
}

/* PostMysteryEncounterPhase: option 1's onPostOptionPhase first offers the ability (handleSwapAbility); true when it is waiting for the answer */
bool gc_clown_post_start(gc_run *run, const gc_rphase *p)
{
    if (p->b != 0) { gc_clown_outro(run); return true; }
    gc_ev_msg(run, "mysteryEncounters/clowningAround:option.1.applyAbilityDialogue", 0);
    gc_ev_msg(run, "mysteryEncounters/clowningAround:option.1.applyAbilityMessage", 1, pr_arg_ability("ability", p->c));
    gc_ev_msg(run, "mysteryEncounters/clowningAround:option.1.abilityPrompt", 1, pr_arg_ability("ability", p->c));
    gc_screen *s = &run->screen;
    memset(s, 0, sizeof *s);
    s->kind = GC_SCREEN_MYSTERY;
    s->mystery_type = GC_ME_CLOWNING_AROUND;
    s->mystery_tier = 0;
    s->me_stage = GC_ME_STAGE_ABILITY;
    s->n_options = 2;
    s->option_ok[0] = s->option_ok[1] = true;
    s->option_pick[0] = GC_PICK_MEMBER;   /* Yes: the party screen (any Pokemon, fainted or not; backing out asks again) */
    s->me_args[s->n_me_args++] = pr_arg_ability("ability", p->c);
    s->me_args[s->n_me_args++] = pr_arg_key("blacephalonName", "pokemon:blacephalon");
    return false;
}

/* a = 0 (Yes) with b = the party member, or 1 (No) */
bool gc_clown_post_choose(gc_run *run, const gc_rphase *p, const gc_choice *c, const char **why, bool *ok)
{
    if (c->kind != GC_CH_MYSTERY) { *why = "choose yes or no"; return false; }
    if (c->a != 0 && c->a != 1) { *why = "no such option"; return false; }
    if (c->a == 0 && (c->b < 0 || c->b >= run->n_party)) { *why = "no such Pokemon"; return false; }
    *ok = true;
    memset(&run->screen, 0, sizeof run->screen);
    if (c->a == 0) {
        /* applyAbilityOverrideToPokemon: a fusion's ability is its fusion data's */
        gc_mon *m = &run->party[c->b];
        if (m->b.fusion_species > 0) m->b.fusion_custom_ability = (int16_t)p->c;
        else m->b.custom_ability = (int16_t)p->c;
        pr_refresh_ability(&m->b);
        gc_ev_msg(run, "mysteryEncounters/clowningAround:option.1.abilityGained", 2, pr_arg_pokemon_name(&run->bt, "chosenPokemon", &m->b), pr_arg_ability("ability", p->c));
    }
    gc_clown_outro(run);
    return true;
}

void gc_clown_outro(gc_run *run)
{
    gc_ev_msg(run, "mysteryEncounters/clowningAround:outro", 0);
}
