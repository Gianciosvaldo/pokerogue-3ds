/* The reward screen: the adapter between game-core's run and reward-core. It makes a pr_reward_ctx from the
 * party, the items, the unlocks, the balls, the learnsets and the arena as they are now, and rolls
 * SelectModifierPhase's options through reward-core's pr_reward_select_start for each way the screen comes
 * up (src/phases/select-modifier-phase.ts at 8555c08): the first roll, a paid reroll (with the Lock Capsule's
 * tiers), the screen again after a TM or the Memory Mushroom (copy()), and the no-victory screen. reward-core
 * rolls on run->run.rng; this file rolls nothing itself. What the player does with an option: the application
 * of healing, revive, PP and PP Up rewards is at the end of this file (Task 60); the rest is gc_stubs.c. */
#include "internal.h"

/* ---- the items by key ---- */
int gc_reward_item_index(const char *key)
{
    const char c = key[0];
    for (int i = 0; i < PR_ITEM_COUNT_; i++) {
        const char *k = pr_item_keys[i];
        if (k[0] == c && !strcmp(k, key)) return i;
    }
    return -1;
}

static int held_entries(const gc_run *run, const char *key, uint32_t pokemon_id)
{
    int n = 0;
    for (int i = 0; i < run->n_items; i++)
        if (!strcmp(run->items[i].key, key) && (!pokemon_id || run->items[i].pokemon_id == pokemon_id)) n++;
    return n;
}

/* ---- the types an attack move counts as for the type-boosting items ---- */
/* Judgment, Multi-Attack, Techno Blast, Raging Bull and Ivy Cudgel read the user's species or its fusion
 * partner's; battle-core's pr_apply_variable_move_type reads the primary only. For the partner upstream reads
 * `user.fusionSpecies.formIndex`, the species object's own formIndex, which is 0 whatever form the fusion
 * has. A copy whose primary is the partner, in form 0, gives battle-core's version that answer. */
static pr_pokemon with_partner_as_primary(const pr_pokemon *p, int a, int b)
{
    pr_pokemon t = *p;
    if (t.species != a && t.species != b && t.fusion_species && (t.fusion_species == a || t.fusion_species == b)) {
        t.species = t.fusion_species;
        t.form = 0;
    }
    return t;
}

static bool can_species_tera(const gc_run *run, const pr_pokemon *p)
{
    /* canSpeciesTera: the Tera Orb, and not a Mega, a Gigantamax or Ultra Necrozma */
    if (gc_item_stack(run, "TERA_ORB") <= 0 || pr_is_mega(p) || pr_is_max(p)) return false;
    const pr_species *sp = gc_species(p->species);
    return !(p->species == SPECIES_ID_NECROZMA && sp && !strcmp(gc_form(p->species, p->form)->form_key, "ultra"));
}

int gc_types_for_item_spawn(gc_run *run, const gc_mon *m, int move, int8_t out[2])
{
    pr_battle *bt = &run->bt;
    const pr_pokemon *p = &m->b;
    const pr_move *mv = &pr_moves[move];
    /* move.getAttrs("VariableMoveTypeAttr")[0] */
    int var = -1;
    for (unsigned i = 0; i < mv->nattr && var < 0; i++)
        if (pr_attr_is(pr_attrs[mv->attr0 + i].cls, PR_AC_VariableMoveTypeAttr)) var = pr_attrs[mv->attr0 + i].cls;
    if (var >= 0) {
        switch (var) {
        case PR_AC_AuraWheelTypeAttr:
            /* on Morpeko only, both the Black Glasses' and the Magnet's type */
            if (p->species == SPECIES_ID_MORPEKO || p->fusion_species == SPECIES_ID_MORPEKO) {
                out[0] = POKEMON_TYPE_DARK;
                out[1] = POKEMON_TYPE_ELECTRIC;
                return 2;
            }
            break;
        case PR_AC_TeraBlastTypeAttr: {
            const int tera = pr_get_tera_type(p);
            if (can_species_tera(run, p) && tera != mv->type && tera != POKEMON_TYPE_STELLAR && tera != POKEMON_TYPE_UNKNOWN) {
                out[0] = mv->type;
                out[1] = (int8_t)tera;
                return 2;
            }
            break;
        }
        case PR_AC_MatchUserTypeAttr: {
            int8_t ty[PR_MAX_TYPES];
            const int n = pr_get_types(p, ty, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);
            out[0] = n > 0 ? ty[0] : mv->type;
            return 1;
        }
        case PR_AC_FormChangeItemTypeAttr:
        case PR_AC_TechnoBlastTypeAttr:
        case PR_AC_RagingBullTypeAttr:
        case PR_AC_IvyCudgelTypeAttr:
        case PR_AC_HiddenPowerTypeAttr: {
            /* getTypesForItemSpawn applies the attribute to the move's own type */
            int t = mv->type;
            pr_pokemon user = *p;
            if (var == PR_AC_FormChangeItemTypeAttr) user = with_partner_as_primary(p, SPECIES_ID_ARCEUS, SPECIES_ID_SILVALLY);
            else if (var == PR_AC_TechnoBlastTypeAttr) user = with_partner_as_primary(p, SPECIES_ID_GENESECT, SPECIES_ID_GENESECT);
            else if (var == PR_AC_RagingBullTypeAttr) user = with_partner_as_primary(p, SPECIES_ID_PALDEA_TAUROS, SPECIES_ID_PALDEA_TAUROS);
            else if (var == PR_AC_IvyCudgelTypeAttr) user = with_partner_as_primary(p, SPECIES_ID_OGERPON, SPECIES_ID_OGERPON);
            pr_apply_variable_move_type(bt, &user, move, &t);
            out[0] = (int8_t)t;
            return 1;
        }
        default:
            break;
        }
        out[0] = mv->type;
        return 1;
    }
    /* getMoveTypeForItemSpawn: only the abilities that change a move's type, with movesetGenInProgress set,
     * which makes canApplyAbility true whatever the Pokemon's state (fainted, suppressed, on the field) */
    int t = mv->type;
    pr_pokemon tmp = *p;
    if (tmp.hp <= 0) tmp.hp = 1;
    tmp.ability_suppressed = false;
    tmp.on_field = false;
    pr_ab_params pp = {.pokemon = &tmp, .opponent = &tmp, .move = move, .simulated = true, .holder = &t};
    pr_apply_ab_attrs(bt, PR_AC_MoveTypeChangeAbAttr, &pp);
    out[0] = (int8_t)t;
    return 1;
}

/* ---- the party member ---- */
static void fill_learnsets(gc_run *run, const gc_mon *m, pr_reward_mon *r)
{
    static gc_learn_row rows[PR_REWARD_MAX_LEVEL_MOVES];
    /* getLevelMoves({includeEvolutionMoves, includeRelearnerMoves}): what getCompatibleTms leaves out */
    int n = gc_level_moves(m, m->b.level, true, false, true, rows, PR_REWARD_MAX_LEVEL_MOVES);
    if (n < 0) { gc_stub(run, GC_STUB_REWARD_INPUT); n = 0; }
    r->n_level_moves = (uint16_t)n;
    for (int i = 0; i < n; i++) {
        r->level_moves[i] = rows[i].move;
        r->level_move_levels[i] = rows[i].level;
        r->level_move_sources[i] = rows[i].source;
    }
    /* getLearnableLevelMoves' own list: from level 1, with the prevolutions' */
    n = gc_level_moves(m, 1, true, true, true, rows, PR_REWARD_MAX_LEVEL_MOVES);
    if (n < 0) { gc_stub(run, GC_STUB_REWARD_INPUT); n = 0; }
    r->has_explicit_relearn_moves = true;
    r->n_relearn_moves = (uint16_t)n;
    for (int i = 0; i < n; i++) {
        r->relearn_moves[i] = rows[i].move;
        r->relearn_move_levels[i] = rows[i].level;
        r->relearn_move_sources[i] = rows[i].source;
    }
    r->n_egg_moves = (uint8_t)gc_unlocked_egg_moves(run, m, r->egg_moves);
    r->met_as_starter = m->met_wave == -1;
    r->egg_relearn_allowed = true;   /* no challenge hook yet: ChallengeType.EGG_MOVE_RELEARN_AVAILABILITY keeps its default */
    r->n_used_tms = (uint8_t)(m->n_used_tms < PR_REWARD_MAX_USED_TMS ? m->n_used_tms : PR_REWARD_MAX_USED_TMS);
    for (int i = 0; i < r->n_used_tms; i++) r->used_tms[i] = m->used_tms[i];
}

static void fill_mon(gc_run *run, gc_mon *m, pr_reward_mon *r)
{
    pr_pokemon *p = &m->b;
    memset(r, 0, sizeof *r);
    r->id = p->id;
    r->species = (int16_t)p->species;
    r->form = p->form;
    r->fusion_species = p->fusion_species ? (int16_t)p->fusion_species : -1;
    r->fusion_form = p->fusion_form;
    r->gender = p->gender;
    r->fusion_gender = 0;
    r->level = p->level;
    r->exp = m->exp;
    r->hp = p->hp;
    r->max_hp = p->stats[STAT_HP];
    r->status = p->status;
    r->status_toxic_turns = p->status_toxic_turns;
    r->status_sleep_turns = p->status_sleep_turns;
    r->status_freeze_turns = p->status_freeze_turns;
    /* getTag(CONFUSED), getTag(NIGHTMARE): the tags stay in summonData until the next battle's ReturnPhase, so a
     * confusion or Nightmare still active on the last turn is on the Pokemon at the reward screen. A fainted one has
     * none upstream (FaintPhase resets its summonData); its HP decides first in every filter that reads these */
    r->confused = pr_get_battler_tag(p, BATTLER_TAG_TYPE_CONFUSED) != NULL;
    r->nightmare = pr_get_battler_tag(p, BATTLER_TAG_TYPE_NIGHTMARE) != NULL;
    r->nature = p->nature;
    r->has_explicit_nature = true;
    r->base_nature = m->has_custom_nature ? m->base_nature : p->nature;
    r->custom_nature = m->has_custom_nature ? p->nature : -1;
    r->tera_type = p->tera_type;
    r->friendship = p->friendship;
    r->ability = p->base_ability;
    r->cur_ability = p->ability;
    r->passive = p->passive;
    r->can_apply_ability = pr_can_apply_ability(&run->bt, p, p->ability);
    r->can_apply_passive = p->passive && pr_can_apply_ability(&run->bt, p, p->passive);
    /* getTypes({includeTeraType: false, bypassSummonData: true, ignoreThirdType: true}), what PARTY_TYPE reads */
    int8_t ty[PR_MAX_TYPES];
    const int nt = pr_get_types(p, ty, PR_TYPES_NO_TERA | PR_TYPES_BYPASS_SUMMON | PR_TYPES_IGNORE_THIRD);
    for (int i = 0; i < nt && i < 2; i++) r->types[r->n_types++] = ty[i];
    for (int i = 0; i < p->n_moves && i < 4; i++) {
        if (!p->moves[i]) continue;
        r->moves[r->n_moves].move = p->moves[i];
        r->moves[r->n_moves].pp_used = p->pp_used[i];
        r->moves[r->n_moves].pp_up = p->pp_up[i];
        r->moves[r->n_moves].max_pp = (uint8_t)gc_move_max_pp(p, i);
        r->base_moves[r->n_moves] = p->moves[i];
        r->n_moves++;
    }
    r->n_base_moves = r->n_moves;
    r->pause_evolutions = m->pause_evolutions;
    r->allowed_in_challenge = true;   /* no challenge is modelled */
    /* canSetStatus(TOXIC | BURN, quiet, overrideStatus, no source, ignoreField): the Toxic and Flame Orbs' weights */
    r->can_toxic = pr_can_set_status(&run->bt, &run->eng, p, STATUS_EFFECT_TOXIC, true, true, NULL, true);
    r->can_burn = pr_can_set_status(&run->bt, &run->eng, p, STATUS_EFFECT_BURN, true, true, NULL, true);
    /* each attack move's types for item spawns, in moveset order */
    for (int i = 0; i < r->n_moves; i++) {
        if (!pr_move_is_attack(r->moves[i].move)) continue;
        int8_t t[2];
        const int k = gc_types_for_item_spawn(run, m, r->moves[i].move, t);
        for (int j = 0; j < k && r->n_attack_types < PR_REWARD_MAX_ATTACK_TYPES; j++) r->attack_types[r->n_attack_types++] = t[j];
    }
    /* getPersistentTreasureCount(): its Golden Punches, and the run's Amulet Coins and Golden Poke Balls */
    r->persistent_treasure = held_entries(run, "GOLDEN_PUNCH", p->id) + held_entries(run, "AMULET_COIN", 0)
                             + held_entries(run, "GOLDEN_POKEBALL", 0);
    memcpy(r->ivs, p->ivs, 6);
    for (int i = 0; i < 6; i++) r->stats[i] = p->stats[i];
    r->ability_index = (int8_t)m->ability_index;
    r->fusion_ability_index = p->fusion_ability_index;
    r->shiny = p->shiny;
    r->passive_flag = p->passive_flag;
    r->variant = m->variant;
    r->luck = m->luck;
}

static bool caught(void *user, int species)
{
    const gc_run *run = user;
    return run->sys && species > 0 && species <= GC_MAX_SPECIES_ID && run->sys->dex[species].caught_attr != 0;
}

/* the context without the learnsets: everything the application of a reward reads (hp, status, moves, the
 * held items for the Healing Charm), and the first half of what the screens read. `count` = a stand-in
 * input is counted in run->stubs (the screens count it; applying a reward doesn't count it twice) */
static pr_reward_ctx ctx_store;

static pr_reward_ctx *ctx_base(gc_run *run, bool count)
{
    pr_reward_ctx *c = &ctx_store;
    memset(c, 0, sizeof *c);
    c->run = &run->run;
    c->wave = run->wave;
    c->luck = gc_party_luck(run);
    c->n_party = run->n_party < PR_REWARD_MAX_PARTY ? run->n_party : PR_REWARD_MAX_PARTY;
    for (int k = 0; k < c->n_party; k++) fill_mon(run, &run->party[run->party_order[k]], &c->party[k]);
    for (int i = 0; i < run->n_items && c->n_items < PR_REWARD_MAX_OWNED; i++) {
        const gc_item *it = &run->items[i];
        const int item = gc_reward_item_index(it->key);
        if (item < 0) { if (count) gc_stub(run, GC_STUB_REWARD_INPUT); continue; }
        pr_owned_item *o = &c->items[c->n_items++];
        o->item = (uint16_t)item;
        o->arg = it->arg;
        o->stack = it->stack;
        o->battle_count = it->battle_count;
        o->owner = -1;
        if (it->pokemon_id)
            for (int k = 0; k < c->n_party; k++)
                if (c->party[k].id == it->pokemon_id) { o->owner = (int8_t)k; break; }
        o->active = true;   /* the modifier's own default; lures' and the Silver Poke Ball's battle counts aren't kept yet */
    }
    for (int i = 0; i < 5; i++) c->pokeball_counts[i] = run->pokeballs[i];
    c->biome = run->biome;
    c->time_of_day = pr_run_get_time_of_day(run->biome, run->wave, run->wave_cycle_offset);
    c->weather = run->bt.weather;
    c->unlocked_eviolite = run->sys && run->sys->unlocks.eviolite;
    c->unlocked_mini_black_hole = run->sys && run->sys->unlocks.mini_black_hole;
    c->caught = caught;
    c->caught_user = run;
    return c;
}

pr_reward_ctx *gc_reward_ctx(gc_run *run)
{
    pr_reward_ctx *c = ctx_base(run, true);
    /* getLearnableLevelMoves().length > 0 (the Memory Mushroom's weight): reward-core's own list, once the
     * rows are in */
    for (int k = 0; k < c->n_party; k++) fill_learnsets(run, &run->party[run->party_order[k]], &c->party[k]);
    static pr_reward_memory_move moves[PR_REWARD_MAX_LEVEL_MOVES + 4 + PR_REWARD_MAX_USED_TMS];
    for (int k = 0; k < c->n_party; k++)
        c->party[k].has_learnable_level_moves =
            pr_reward_memory_moves(c, k, moves, (int)(sizeof moves / sizeof moves[0])) > 0;
    return c;
}

/* ---- the offers ---- */
static uint8_t target_of(int item)
{
    /* PokemonMoveModifierType: Ether, Max Ether, PP Up, PP Max pick a move; any other PokemonModifierType (held
     * items, healing, evolution and form change items, TMs, the Memory Mushroom...) a party member */
    if (item == PR_ITEM_ETHER || item == PR_ITEM_MAX_ETHER || item == PR_ITEM_PP_UP || item == PR_ITEM_PP_MAX)
        return GC_TARGET_MOVE;
    return pr_items[item].flags & (PR_ITEMF_POKEMON | PR_ITEMF_HELD | PR_ITEMF_FORM_CHANGE) ? GC_TARGET_POKEMON
                                                                                             : GC_TARGET_NONE;
}

static void to_offer(const pr_reward_option *o, gc_offer *g)
{
    g->key = pr_item_keys[o->type.item];
    g->arg = o->type.arg;
    g->tier = o->tier;
    g->target = target_of(o->type.item);
    g->cost = o->cost;
    g->upgrade_count = o->upgrade_count;
    g->is_purchase = false;
}

/* ---- SelectModifierPhase ---- */
double gc_reward_reroll_cost(gc_run *run)
{
    gc_internal *in = gc_int(run);
    const gc_reward_state *rs = &in->reward;
    return pr_reward_reroll_cost(gc_reward_ctx(run), in->reroll_count, in->lock_rarities, rs->options, rs->n_options,
                                 rs->has_custom ? &rs->custom : NULL);
}

void gc_reward_queue_copy(gc_run *run)
{
    gc_internal *in = gc_int(run);
    gc_reward_state *rs = &in->reward;
    rs->copy_pending = true;
    rs->copy_reroll_count = in->reroll_count;
    rs->copy_reroll_multiplier = rs->has_custom ? rs->custom.reroll_multiplier : NAN;
    rs->n_copy_options = rs->n_options;
    memcpy(rs->copy_options, rs->options, sizeof rs->options);
    gc_rq_unshift(run, gc_rp(RP_SELECT_MODIFIER, 0, 1, 0));
}

int gc_port_reward_options(gc_run *run, int kind, gc_offer out[GC_MAX_OPTIONS], double *reroll_cost)
{
    gc_internal *in = gc_int(run);
    gc_reward_state *rs = &in->reward;
    pr_reward_ctx *ctx = gc_reward_ctx(run);
    const unsigned before = run->run.unported;
    pr_reward_option opts[PR_REWARD_MAX_OPTIONS];
    int8_t tiers[PR_REWARD_MAX_OPTIONS];
    pr_reward_select sel;
    memset(&sel, 0, sizeof sel);
    int n;

    switch (kind) {
    case GC_REWARD_REROLL: {
        /* rerollModifiers(): unshiftNew("SelectModifierPhase", rerollCount + 1, the options' tiers). The tiers
         * only count while lockModifierTiers is on; the new phase has no custom settings. */
        in->reroll_count++;
        rs->is_copy = rs->has_custom = false;
        sel.reroll_count = in->reroll_count;
        const int nt = in->lock_rarities ? pr_reward_get_locked_tiers(rs->options, rs->n_options, tiers) : 0;
        if (nt > 0) { sel.locked_tiers = tiers; sel.n_locked_tiers = nt; }
        break;
    }
    case GC_REWARD_COPY:
        /* copy(): the same rerollCount, and the old options as guaranteed options, without luck upgrades. The
         * thresholds stay what the screen it copies made. */
        if (!rs->copy_pending) {   /* the screen again as it is */
            rs->copy_reroll_count = in->reroll_count;
            rs->copy_reroll_multiplier = rs->has_custom ? rs->custom.reroll_multiplier : NAN;
            rs->n_copy_options = rs->n_options;
            memcpy(rs->copy_options, rs->options, sizeof rs->options);
        }
        rs->copy_pending = false;
        in->reroll_count = rs->copy_reroll_count;
        rs->is_copy = rs->has_custom = true;
        memset(&rs->custom, 0, sizeof rs->custom);
        rs->custom.n_options = rs->n_copy_options;
        memcpy(rs->custom.options, rs->copy_options, sizeof rs->custom.options);
        rs->custom.reroll_multiplier = rs->copy_reroll_multiplier;
        rs->custom.allow_luck_upgrades = false;
        sel.reroll_count = in->reroll_count;
        sel.is_copy = true;
        break;
    default:
        /* start(): a first screen. The no-victory one is (0, undefined, {fillRemaining: false, rerollMultiplier: -1}) */
        in->reroll_count = 0;
        rs->is_copy = false;
        rs->has_custom = kind == GC_REWARD_RESTRICTED;
        memset(&rs->custom, 0, sizeof rs->custom);
        rs->custom.reroll_multiplier = kind == GC_REWARD_RESTRICTED ? -1 : NAN;
        rs->custom.allow_luck_upgrades = true;
        break;
    }
    sel.custom = rs->has_custom ? &rs->custom : NULL;
    n = pr_reward_select_start(ctx, &sel, &rs->th, opts);
    if (n < 0 || n > GC_MAX_OPTIONS) {
        /* reward-core flagged the roll (upstream's own answer is undefined here, or the tables are too small):
         * an empty screen without a reroll, counted */
        run->run.unported = before;
        gc_stub(run, GC_STUB_REWARDS);
        n = 0;
        rs->has_custom = true;
        rs->custom.reroll_multiplier = -1;
    }
    rs->n_options = n;
    memset(rs->options, 0, sizeof rs->options);
    memcpy(rs->options, opts, (size_t)n * sizeof opts[0]);
    for (int i = 0; i < n; i++) to_offer(&opts[i], &out[i]);
    *reroll_cost = pr_reward_reroll_cost(ctx, in->reroll_count, in->lock_rarities, rs->options, n,
                                         rs->has_custom ? &rs->custom : NULL);
    return n;
}

/* ---- applying a reward: healing, revives, PP restores and PP Ups (Task 60) ----
 * What `applyModifier` does once the player has picked the target: PokemonHpRestoreModifier (Potions, Full
 * Restore, Revives, Sacred Ash), PokemonPpRestoreModifier / PokemonAllMovePpRestoreModifier (Ether, Elixir)
 * and PokemonPpUpModifier, after the party menu's own filter (the selectFilter / moveSelectFilter of each
 * item type). reward-core's pr_reward_apply does both on a context made from the run; this adds only
 * what lives on game-core's side of the line: which party member and move slot the choice means, and
 * writing the result back into the run. */
uint8_t gc_reward_target(const char *key)
{
    const int item = gc_reward_item_index(key);
    return item < 0 ? GC_TARGET_NONE : target_of(item);
}

/* FRIENDSHIP_GAIN_FROM_RARE_CANDY (#balance/starters): reward-core's candy.c adds the same 6 to the Pokemon itself */
#define GC_FRIENDSHIP_GAIN_FROM_RARE_CANDY 6

/* Upstream 8555c08: AddPokeballModifier and AddPokeballModifierType (src/modifier/modifier.ts & modifier-type.ts):
 * "export class AddPokeballModifier extends ConsumableModifier {
 *    private pokeballType: PokeballType;
 *    private count: number;
 *    override apply(): boolean {
 *      const pokeballCounts = globalScene.pokeballCounts;
 *      pokeballCounts[this.pokeballType] = Math.min(
 *        pokeballCounts[this.pokeballType] + this.count,
 *        MAX_PER_TYPE_POKEBALLS,
 *      );
 *      return true;
 *    }
 *  }"
 * "POKEBALL: () => new AddPokeballModifierType("pb", PokeballType.POKEBALL, 5),
 *  GREAT_BALL: () => new AddPokeballModifierType("gb", PokeballType.GREAT_BALL, 5),
 *  ULTRA_BALL: () => new AddPokeballModifierType("ub", PokeballType.ULTRA_BALL, 5),
 *  ROGUE_BALL: () => new AddPokeballModifierType("rb", PokeballType.ROGUE_BALL, 5),
 *  MASTER_BALL: () => new AddPokeballModifierType("mb", PokeballType.MASTER_BALL, 1),"
 * "get name(): string {
 *    return i18next.t("modifierType:ModifierType.AddPokeballModifierType.name", {
 *      modifierCount: this.count,
 *      pokeballName: getPokeballName(this.pokeballType),
 *    });
 *  }"
 * "export const MAX_PER_TYPE_POKEBALLS: number = 99;" (src/data/pokeball.ts)
 * Taken from the reward screen, SelectModifierPhase.applyModifier -> BattleScene.addModifier runs apply() and shows
 * no text. Given by ModifierRewardPhase.doReward (addModifier, then the text below), the text is
 * "i18next.t("battle:rewardGain", { modifierName: newModifier?.type.name })", i.e. "5× Poké Ball".
 * reward-core has no application for Poke Balls (its pools.c only reads the same 99 for hasMaximumBalls), so the rule
 * is game-core's; tools/applydiff.sh compares it with upstream's own classes (tests/pokeball.producer.test.ts). */
#define GC_MAX_PER_TYPE_POKEBALLS 99

/* AddPokeballModifierType's pokeballType and count for a reward item; -1 = not a Poke Ball reward */
static int pokeball_reward(int item, int *count)
{
    switch (item) {
    case PR_ITEM_POKEBALL: *count = 5; return POKEBALL_TYPE_POKEBALL;
    case PR_ITEM_GREAT_BALL: *count = 5; return POKEBALL_TYPE_GREAT_BALL;
    case PR_ITEM_ULTRA_BALL: *count = 5; return POKEBALL_TYPE_ULTRA_BALL;
    case PR_ITEM_ROGUE_BALL: *count = 5; return POKEBALL_TYPE_ROGUE_BALL;
    case PR_ITEM_MASTER_BALL: *count = 1; return POKEBALL_TYPE_MASTER_BALL;
    default: return -1;
    }
}

/* AddPokeballModifier.apply, for the reward whose key is `key`; false = not a Poke Ball reward, nothing changed */
bool gc_reward_add_pokeballs(gc_run *run, const char *key)
{
    int count = 0;
    const int item = gc_reward_item_index(key);
    const int ball = item < 0 ? -1 : pokeball_reward(item, &count);
    if (ball < 0) return false;
    const int total = run->pokeballs[ball] + count;
    run->pokeballs[ball] = (uint8_t)(total < GC_MAX_PER_TYPE_POKEBALLS ? total : GC_MAX_PER_TYPE_POKEBALLS);
    return true;
}

/* ModifierRewardPhase.doReward: the reward is added as it is, then the text names it. A Poke Ball's name is
 * AddPokeballModifierType.name, "{{modifierCount}}× {{pokeballName}}": its two values ride on the message and the
 * name nests that key ($t() takes the message's own values), so the text is the locale's, not ours. */
void gc_reward_gain(gc_run *run, const char *key)
{
    static const char *const ball_name[] = {"pokeball:pokeBall", "pokeball:greatBall", "pokeball:ultraBall",
                                            "pokeball:rogueBall", "pokeball:masterBall"};
    int count = 0;
    const int item = gc_reward_item_index(key);
    const int ball = item < 0 ? -1 : pokeball_reward(item, &count);
    gc_port_give_reward(run, key);
    if (ball < 0) {
        gc_ev_msg(run, "battle:rewardGain", 1, gc_m_item("modifierName", key));
        return;
    }
    gc_ev_msg(run, "battle:rewardGain", 3,
              (pr_msg_arg){"modifierName", PR_ARG_TEMPLATE, 0, "$t(modifierType:ModifierType.AddPokeballModifierType.name)"},
              pr_arg_int("modifierCount", count), pr_arg_key("pokeballName", ball_name[ball]));
}

static bool core_applies(int item)
{
    switch (item) {
    case PR_ITEM_RARE_CANDY: case PR_ITEM_RARER_CANDY:
    case PR_ITEM_POTION: case PR_ITEM_SUPER_POTION: case PR_ITEM_HYPER_POTION: case PR_ITEM_MAX_POTION:
    case PR_ITEM_FULL_RESTORE: case PR_ITEM_FULL_HEAL: case PR_ITEM_REVIVE: case PR_ITEM_MAX_REVIVE: case PR_ITEM_SACRED_ASH:
    case PR_ITEM_ETHER: case PR_ITEM_MAX_ETHER: case PR_ITEM_ELIXIR: case PR_ITEM_MAX_ELIXIR:
    case PR_ITEM_PP_UP: case PR_ITEM_PP_MAX:
        return true;
    default:
        return false;
    }
}

/* a move's index in reward-core's moveset (fill_mon leaves out an empty slot), -1 = no such move */
static int core_move_index(const pr_pokemon *p, int slot)
{
    if (slot < 0 || slot >= p->n_moves || slot >= 4 || !p->moves[slot]) return -1;
    int k = 0;
    for (int i = 0; i < slot; i++) k += p->moves[i] != 0;
    return k;
}

/* the tag a cure ended, out of summonData.tags (the order of the rest stays). The cure's own text is the caller's:
 * reward-core reports it, so pr_remove_tag's (battle-core's onRemove) is not used */
static void drop_tag(pr_pokemon *b, int type)
{
    for (int i = 0; i < b->n_tags; i++) {
        if (b->tags[i].type != type) continue;
        memmove(&b->tags[i], &b->tags[i + 1], (size_t)(b->n_tags - i - 1) * sizeof b->tags[0]);
        b->n_tags--;
        return;
    }
}

/* Bounded Nature Mint adapter. Unsupported permanent-stat configurations keep
 * their existing counted stand-in path; they are never credited to this API. */
static int apply_mint(gc_run *run, const gc_offer *o, int party, const char **why)
{
    int k;
    if (party < 0 || party >= run->n_party || (k = gc_battle_slot_of(run, party)) < 0) {
        *why = "pick a Pokemon";
        return GC_APPLY_REFUSED;
    }
    gc_mon *m = &run->party[party];
    if (o->arg < 0 || o->arg > 24 || m->b.nature == o->arg) {
        *why = "partyUiHandler:anyEffect";
        return GC_APPLY_REFUSED;
    }
    if (run->run.has_challenges) return GC_APPLY_UNHANDLED;
    static const char *const stat_items[] = {"BASE_STAT_BOOSTER", "SOUL_DEW",
        "MYSTERY_ENCOUNTER_SHUCKLE_JUICE", "MYSTERY_ENCOUNTER_OLD_GATEAU",
        "MYSTERY_ENCOUNTER_MACHO_BRACE"};
    for (int i = 0; i < run->n_items; i++) {
        const gc_item *it = &run->items[i];
        if (it->virtual_stack) return GC_APPLY_UNHANDLED;
        if (it->stack <= 0 || (it->pokemon_id && it->pokemon_id != m->b.id)) continue;
        for (unsigned j = 0; j < sizeof stat_items / sizeof *stat_items; j++)
            if (!strcmp(it->key, stat_items[j])) return GC_APPLY_UNHANDLED;
    }
    pr_reward_ctx *c = ctx_base(run, false);
    if (!pr_reward_apply_mint(c, o->arg, k)) return GC_APPLY_UNHANDLED;
    const pr_reward_mon *r = &c->party[k];
    const int old_hp = m->b.hp;
    m->base_nature = r->base_nature;
    m->has_custom_nature = true;
    m->b.nature = r->nature;
    memcpy(m->b.stats, r->stats, sizeof m->b.stats);
    m->b.hp = r->hp;
    for (int i = 0; i < c->n_nature_unlock_events; i++)
        gc_system_unlock_species_nature(run->sys, c->nature_unlock_events[i].species,
                                       c->nature_unlock_events[i].nature);
    c->n_nature_unlock_events = 0;
    if (m->b.hp != old_hp) {
        gc_event *ev = gc_ev_new(run, GC_EV_HP);
        if (ev) { ev->a = 0; ev->b = party; ev->c = m->b.hp; }
    }
    return GC_APPLY_DONE;
}

/* updateModifiers recalculates the whole party. Refused purchases may still
 * have added their persistent item: pinned addModifier returns false there. */
static int apply_vitamin(gc_run *run, const gc_offer *o, int party, const char **why)
{
    int target;
    if (party < 0 || party >= run->n_party || (target = gc_battle_slot_of(run, party)) < 0) {
        *why = "pick a Pokemon";
        return GC_APPLY_REFUSED;
    }
    if (o->arg < 0 || o->arg >= 6) { *why = "partyUiHandler:anyEffect"; return GC_APPLY_REFUSED; }
    if (run->run.has_challenges || run->n_items < 0 || run->n_items > GC_MAX_ITEMS
        || run->n_items > PR_REWARD_MAX_OWNED || run->n_items > PR_MAX_MODIFIERS) return GC_APPLY_UNHANDLED;
    bool existing = false;
    /* ctx_base omits unknown keys and maps missing holders to -1. Do not let
     * either conversion silently widen this API's bounded inventory contract. */
    for (int i = 0; i < run->n_items; i++) {
        if (gc_reward_item_index(run->items[i].key) < 0
            || run->items[i].virtual_stack)
            return GC_APPLY_UNHANDLED;
        if (!strcmp(run->items[i].key, "BASE_STAT_BOOSTER") && run->items[i].arg == o->arg
            && run->items[i].pokemon_id == run->party[party].b.id) existing = true;
        if (!run->items[i].pokemon_id) continue;
        bool found = false;
        for (int k = 0; k < run->n_party; k++) if (run->party[k].b.id == run->items[i].pokemon_id) found = true;
        if (!found) return GC_APPLY_UNHANDLED;
    }
    if ((run->n_items == GC_MAX_ITEMS || run->n_items == PR_MAX_MODIFIERS) && !existing)
        return GC_APPLY_UNHANDLED;
    pr_reward_ctx *c = ctx_base(run, false);
    const double cost = o->is_purchase || o->cost > 0 ? o->cost : -1;
    pr_reward_vitamin_result result;
    const int status = pr_reward_apply_vitamin(c, o->arg, target, cost, &result);
    if (status == PR_REWARD_VITAMIN_UNSUPPORTED) return GC_APPLY_UNHANDLED;
    if (status != PR_REWARD_VITAMIN_APPLIED) {
        *why = status == PR_REWARD_VITAMIN_INOPERABLE
            ? "modifierType:ModifierType.PokemonHeldItemModifierType.extra.inoperable"
            : "modifierType:ModifierType.PokemonHeldItemModifierType.extra.tooMany";
        return GC_APPLY_REFUSED;
    }
    gc_item_add(run, "BASE_STAT_BOOSTER", o->arg, run->party[party].b.id, 1);
    gc_items_to_battle(run); /* later candy/evolution stat recalculations read this list */
    for (int k = 0; k < c->n_party; k++) {
        const int storage = run->party_order[k];
        pr_pokemon *p = &run->party[storage].b;
        const int old_hp = p->hp;
        memcpy(p->stats, c->party[k].stats, sizeof p->stats);
        p->hp = c->party[k].hp;
        if (p->hp != old_hp) {
            gc_event *ev = gc_ev_new(run, GC_EV_HP);
            if (ev) { ev->a = 0; ev->b = storage; ev->c = p->hp; }
        }
    }
    if (result.purchase_error) {
        *why = ""; /* upstream plays an error with no text; caller owns presentation */
        return GC_APPLY_REFUSED; /* keeps shop open and prevents money deduction */
    }
    return GC_APPLY_DONE;
}

/* Monetary consumables have no persistent inventory. UI animation and money
 * achievement validation remain caller-owned; Gimmighoul's EvoTracker is guarded. */
static int apply_money_item(gc_run *run, int item)
{
    if (run->run.has_challenges || run->n_items < 0 || run->n_items > PR_REWARD_MAX_OWNED
        || run->n_items > GC_MAX_ITEMS || run->n_party < 1 || run->n_party > PR_REWARD_MAX_PARTY)
        return GC_APPLY_UNHANDLED;
    /* ctx_base intentionally skips unknown keys: reject them before transport. */
    for (int i = 0; i < run->n_items; i++)
        if (run->items[i].virtual_stack || gc_reward_item_index(run->items[i].key) < 0
            || (!strcmp(run->items[i].key, "AMULET_COIN") && run->items[i].pokemon_id))
            return GC_APPLY_UNHANDLED;
    pr_reward_ctx *c = ctx_base(run, false);
    pr_reward_money_item_result result;
    if (!pr_reward_apply_money_item(c, item, &run->money, &result)) return GC_APPLY_UNHANDLED;
    return GC_APPLY_DONE;
}

int gc_reward_apply(gc_run *run, const gc_offer *o, int party, int move_slot, const char **why)
{
    const int item = gc_reward_item_index(o->key);
    const int lifecycle = gc_reward_lifecycle_apply(run, o, party, why);
    if (lifecycle != GC_APPLY_UNHANDLED) return lifecycle;
    if (item < 0) return GC_APPLY_UNHANDLED;
    if (item == PR_ITEM_NUGGET || item == PR_ITEM_BIG_NUGGET || item == PR_ITEM_RELIC_GOLD)
        return apply_money_item(run, item);
    if (item == PR_ITEM_WIDE_LENS) return gc_wide_scene_apply(run,o,party,why,true);
    if (item == PR_ITEM_TEMP_STAT_STAGE_BOOSTER || item == PR_ITEM_DIRE_HIT)
        return gc_temp_apply(run, o, why);
    if (item == PR_ITEM_MINT) return apply_mint(run, o, party, why);
    if (item == PR_ITEM_BASE_STAT_BOOSTER) return apply_vitamin(run, o, party, why);

    if (gc_reward_add_pokeballs(run, o->key)) return GC_APPLY_DONE;   /* no text: addModifier shows none */

    if (item == PR_ITEM_TM_COMMON || item == PR_ITEM_TM_GREAT || item == PR_ITEM_TM_ULTRA) {
        int k;
        if (party < 0 || party >= run->n_party || (k = gc_battle_slot_of(run, party)) < 0) {
            *why = "pick a Pokemon";
            return GC_APPLY_REFUSED;
        }
        gc_mon *m = &run->party[party];
        bool dup = false;
        for (int i = 0; i < m->n_used_tms; i++) if (m->used_tms[i] == (uint16_t)o->arg) dup = true;
        if (!dup && m->n_used_tms < 64) m->used_tms[m->n_used_tms++] = (uint16_t)o->arg;
        gc_reward_queue_copy(run);
        gc_rq_unshift(run, gc_rp(RP_LEARN_MOVE, party, o->arg, 0));
        return GC_APPLY_DONE;
    }

    if (item == PR_ITEM_MEMORY_MUSHROOM) {
        int k;
        if (party < 0 || party >= run->n_party || (k = gc_battle_slot_of(run, party)) < 0) {
            *why = "pick a Pokemon";
            return GC_APPLY_REFUSED;
        }
        pr_reward_ctx *ctx = gc_reward_ctx(run);
        static pr_reward_memory_move moves[PR_REWARD_MAX_LEVEL_MOVES + 4 + PR_REWARD_MAX_USED_TMS];
        const int n = pr_reward_memory_moves(ctx, k, moves, (int)(sizeof moves / sizeof moves[0]));
        if (n <= 0) {
            *why = "partyUiHandler:anyEffect";
            return GC_APPLY_REFUSED;
        }
        if (move_slot < 0 || move_slot >= n) {
            *why = "pick a move";
            return GC_APPLY_REFUSED;
        }
        const uint16_t move_id = moves[move_slot].move;
        gc_reward_queue_copy(run);
        gc_rq_unshift(run, gc_rp(RP_LEARN_MOVE, party, move_id, 0));
        return GC_APPLY_DONE;
    }

    if (!core_applies(item)) return GC_APPLY_UNHANDLED;
    /* a free reward is applied with cost -1, a purchase with its price (a shop item: cost > 0) */
    const bool purchase = o->cost > 0;
    int k = -1, move = -1;
    /* Sacred Ash and Rarer Candy have no party menu: Sacred Ash touches the whole party, and Rarer Candy
     * (AllPokemonLevelIncrementModifierType, pokemonId -1) every member. Neither candy has a filter or a level
     * cap check: PokemonLevelIncrementModifierType's selectFilter always returns null, and getMaxExpLevel() only
     * decides whether the exp is brought up to the new level (it always is: getMaxExpLevel(true) has no limit) */
    if (item != PR_ITEM_SACRED_ASH && item != PR_ITEM_RARER_CANDY) {
        if (party < 0 || party >= run->n_party || (k = gc_battle_slot_of(run, party)) < 0) {
            *why = "pick a Pokemon";
            return GC_APPLY_REFUSED;
        }
        if (target_of(item) == GC_TARGET_MOVE) {
            const pr_pokemon *b = &run->party[party].b;
            move = core_move_index(b, move_slot);
            if (move < 0) { *why = "pick a move"; return GC_APPLY_REFUSED; }
            /* PokemonPpUpModifierType's moveSelectFilter: `pokemonMove.maxPpOverride` (reward-core's moves
             * don't carry it) */
            if ((item == PR_ITEM_PP_UP || item == PR_ITEM_PP_MAX) && b->max_pp_override[move_slot] > 0) {
                *why = "partyUiHandler:anyEffect";
                return GC_APPLY_REFUSED;
            }
        }
    }
    pr_reward_ctx *ctx = ctx_base(run, false);
    const char *msg = NULL;
    if (!pr_reward_apply(ctx, (uint16_t)item, o->arg, k, move, &msg)) {
        /* SelectModifierPhase.applyModifier ends the phase for a free reward whatever addModifier returned
         * (only Sacred Ash, which needs no target, can come back false here); a purchase keeps the screen
         * and its money */
        if (item == PR_ITEM_SACRED_ASH && !purchase) return GC_APPLY_DONE;
        *why = "partyUiHandler:anyEffect";
        return GC_APPLY_REFUSED;
    }
    /* what reward-core changed, back into the run: hp, status (and the counters its Status object carried), the
     * confusion and Nightmare tags a cure ended, PP used and PP Ups. Sacred Ash can touch the whole party, anything
     * else the one member */
    for (int i = 0; i < ctx->n_party; i++) {
        const int storage = run->party_order[i];
        gc_mon *m = &run->party[storage];
        pr_pokemon *b = &m->b;
        const pr_reward_mon *r = &ctx->party[i];
        const bool hp_changed = b->hp != r->hp;
        b->status = (uint8_t)r->status;
        b->status_toxic_turns = r->status_toxic_turns;
        b->status_sleep_turns = r->status_sleep_turns;
        b->status_freeze_turns = r->status_freeze_turns;
        if (!r->confused) drop_tag(b, BATTLER_TAG_TYPE_CONFUSED);
        if (!r->nightmare) drop_tag(b, BATTLER_TAG_TYPE_NIGHTMARE);
        b->hp = r->hp;
        for (int s = 0, c = 0; s < b->n_moves && s < 4; s++) {
            if (!b->moves[s]) continue;
            b->pp_used[s] = r->moves[c].pp_used;
            b->pp_up[s] = r->moves[c].pp_up;
            c++;
        }
        if (b->level != r->level) {
            /* a candy: PokemonLevelIncrementModifier. Its addFriendship(FRIENDSHIP_GAIN_FROM_RARE_CANDY, true)
             * adds the candy's 6 to the starter's candy progress whether or not the Pokemon's own friendship rose
             * (at 199 it rises by 1, at 200 or more not at all), so the amount is the candy's, not the change in
             * friendship. The Pokemon's own friendship is reward-core's (the 200 cap keeps what is above it) */
            const int old_level = b->level;
            const bool too_high = r->level > 255;   /* battle-core's level is a byte: stop at 255, not wrap to a low level */
            b->level = too_high ? 255 : (uint8_t)r->level;
            const pr_species *sp = gc_species(b->species);
            m->exp = too_high && sp ? pr_reward_level_total_exp(255, sp->growth_rate) : r->exp;
            gc_system_add_friendship(run->sys, run, m, GC_FRIENDSHIP_GAIN_FROM_RARE_CANDY, true);
            b->friendship = r->friendship;
            gc_rq_unshift(run, gc_rp(RP_LEVEL_UP, storage, old_level, b->level));
        }
        if (hp_changed || (i == k && item != PR_ITEM_SACRED_ASH && item != PR_ITEM_RARE_CANDY && item != PR_ITEM_RARER_CANDY)) {
            gc_event *ev = gc_ev_new(run, GC_EV_HP);
            if (ev) { ev->a = 0; ev->b = storage; ev->c = b->hp; }
        }
    }
    /* the texts the application queued (queueMessage, after its effect): a cure that ends a confusion says so. Taking
     * the reward shows nothing else (SelectModifierPhase.applyModifier only ends the phase) */
    for (int i = 0; i < ctx->n_apply_texts; i++) {
        const pr_reward_text *t = &ctx->apply_texts[i];
        gc_ev_msg(run, t->key, 1, gc_m_mon(run, "pokemonNameWithAffix", 0, run->party_order[t->party_idx]));
    }
    return GC_APPLY_DONE;
}
