/* The reward screen: the adapter between game-core's run and reward-core. It makes a pr_reward_ctx from the
 * party, the items, the unlocks, the balls, the learnsets and the arena as they are now, and rolls
 * SelectModifierPhase's options through reward-core's pr_reward_select_start for each way the screen comes
 * up (src/phases/select-modifier-phase.ts at 8555c08): the first roll, a paid reroll (with the Lock Capsule's
 * tiers), the screen again after a TM or the Memory Mushroom (copy()), and the no-victory screen. reward-core
 * rolls on run->run.rng; this file rolls nothing itself. What the player does with an option (applying it) is
 * gc_stubs.c until Task 60. */
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
    r->n_used_tms = 0;               /* game-core doesn't apply TMs yet (Task 60): usedTMs stays empty */
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
    r->nature = p->nature;
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

pr_reward_ctx *gc_reward_ctx(gc_run *run)
{
    static pr_reward_ctx c;
    memset(&c, 0, sizeof c);
    c.run = &run->run;
    c.wave = run->wave;
    c.luck = gc_party_luck(run);
    c.n_party = run->n_party < PR_REWARD_MAX_PARTY ? run->n_party : PR_REWARD_MAX_PARTY;
    for (int k = 0; k < c.n_party; k++) fill_mon(run, &run->party[run->party_order[k]], &c.party[k]);
    for (int i = 0; i < run->n_items && c.n_items < PR_REWARD_MAX_OWNED; i++) {
        const gc_item *it = &run->items[i];
        const int item = gc_reward_item_index(it->key);
        if (item < 0) { gc_stub(run, GC_STUB_REWARD_INPUT); continue; }
        pr_owned_item *o = &c.items[c.n_items++];
        o->item = (uint16_t)item;
        o->arg = it->arg;
        o->stack = it->stack;
        o->owner = -1;
        if (it->pokemon_id)
            for (int k = 0; k < c.n_party; k++)
                if (c.party[k].id == it->pokemon_id) { o->owner = (int8_t)k; break; }
        o->active = true;   /* the modifier's own default; lures' and the Silver Poke Ball's battle counts aren't kept yet */
    }
    for (int i = 0; i < 5; i++) c.pokeball_counts[i] = run->pokeballs[i];
    c.biome = run->biome;
    c.time_of_day = pr_run_get_time_of_day(run->biome, run->wave, run->wave_cycle_offset);
    c.weather = run->bt.weather;
    c.unlocked_eviolite = run->sys && run->sys->unlocks.eviolite;
    c.unlocked_mini_black_hole = run->sys && run->sys->unlocks.mini_black_hole;
    c.caught = caught;
    c.caught_user = run;
    /* getLearnableLevelMoves().length > 0 (the Memory Mushroom's weight): reward-core's own list, once the
     * rows are in */
    for (int k = 0; k < c.n_party; k++) fill_learnsets(run, &run->party[run->party_order[k]], &c.party[k]);
    static pr_reward_memory_move moves[PR_REWARD_MAX_LEVEL_MOVES + 4 + PR_REWARD_MAX_USED_TMS];
    for (int k = 0; k < c.n_party; k++)
        c.party[k].has_learnable_level_moves =
            pr_reward_memory_moves(&c, k, moves, (int)(sizeof moves / sizeof moves[0])) > 0;
    return &c;
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
