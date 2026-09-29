/* The PLAYER pool's weight functions (src/modifier/init-modifier-pools.ts, initCommonModifierPool to
 * initMasterModifierPool). The dump lists every entry whose weight is a function (gen/dump-rewards.test.ts,
 * PORTED_WEIGHT_FUNCS) and fails when upstream adds one, so each case below is one of upstream's closures. */
#include "pr/reward.h"
#include "reward_internal.h"
#include <math.h>

bool pr_reward_is_classic(const pr_run *run) { return run->mode == GAME_MODES_CLASSIC || run->mode == GAME_MODES_CHALLENGE; }
static bool is_endless(const pr_run *run) { return run->mode == GAME_MODES_ENDLESS || run->mode == GAME_MODES_SPLICED_ENDLESS; }
static bool has_challenge(const pr_reward_ctx *ctx, int c) { return (ctx->challenges >> c) & 1; }

double pr_reward_hp_ratio(const pr_reward_mon *m) { return floor((double)m->hp / m->max_hp * 100 + 0.5) / 100; }
static int inverse_hp(const pr_reward_mon *m) { return m->max_hp - m->hp; }
static bool fainted(const pr_reward_mon *m) { return m->hp <= 0; }
static int min3(int n) { return n < 3 ? n : 3; }

/* hasMaximumBalls(ballType): classic and pokeballCounts[ballType] >= MAX_PER_TYPE_POKEBALLS (99) */
static bool has_maximum_balls(const pr_reward_ctx *ctx, int ball)
{
    return pr_reward_is_classic(ctx->run) && ctx->pokeball_counts[ball] >= 99;
}

/* skipInClassicAfterWave(wave, defaultWeight) (skipInLastClassicWaveOrDefault: wave 199) */
static int skip_in_classic_after(const pr_reward_ctx *ctx, int wave, int weight)
{
    return pr_reward_is_classic(ctx->run) && ctx->wave >= wave ? 0 : weight;
}

/* lureWeightFunc(maxBattles, weight): not classic wave 199, and no lure of that duration with at least 60% of it
 * left (getBattleCount() >= maxBattles * 0.6) */
static int lure_weight(const pr_reward_ctx *ctx, int max_battles, int weight)
{
    if (pr_reward_is_classic(ctx->run) && ctx->wave == 199) return 0;
    for (int i = 0; i < ctx->n_items; i++) {
        const pr_owned_item *o = &ctx->items[i];
        const int mb = o->item == PR_ITEM_LURE ? 10 : o->item == PR_ITEM_SUPER_LURE ? 15 : o->item == PR_ITEM_MAX_LURE ? 30 : 0;
        if (mb == max_battles && o->battle_count >= max_battles * 0.6) return 0;
    }
    return weight;
}

/* p.hp && no Leppa Berry held && a move with ppUsed, getMovePp() - ppUsed <= 5, ppUsed > floor(getMovePp() / 2) */
static int pp_members(const pr_reward_ctx *ctx)
{
    int n = 0;
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        if (!m->hp || pr_reward_held(ctx, p, PR_ITEM_BERRY, BERRY_TYPE_LEPPA)) continue;
        bool low = false;
        for (int i = 0; i < m->n_moves; i++) {
            const int used = m->moves[i].pp_used, max = m->moves[i].max_pp;
            low |= used && max - used <= 5 && used > max / 2;
        }
        n += low;
    }
    return min3(n);
}

/* p.hp && p.status && no TurnStatusEffectModifier (Toxic or Flame Orb) held whose effect is p's status */
static int status_members(const pr_reward_ctx *ctx)
{
    int n = 0;
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        if (!m->hp || !m->status) continue;
        if (m->status == STATUS_EFFECT_TOXIC && pr_reward_held(ctx, p, PR_ITEM_TOXIC_ORB, -1)) continue;
        if (m->status == STATUS_EFFECT_BURN && pr_reward_held(ctx, p, PR_ITEM_FLAME_ORB, -1)) continue;
        n++;
    }
    return min3(n);
}

/* getInverseHp() >= inv && getHpRatio() <= ratio && !isFainted() */
static int hp_members(const pr_reward_ctx *ctx, int inv, double ratio)
{
    int n = 0;
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        n += inverse_hp(m) >= inv && pr_reward_hp_ratio(m) <= ratio && !fainted(m);
    }
    return min3(n);
}

static int fainted_members(const pr_reward_ctx *ctx)
{
    int n = 0;
    for (int p = 0; p < ctx->n_party; p++) n += fainted(&ctx->party[p]);
    return n;
}

static int unfused_members(const pr_reward_ctx *ctx)
{
    int n = 0;
    for (int p = 0; p < ctx->n_party; p++) n += ctx->party[p].fusion_species < 0;
    return n;
}

/* p.hasAbility(a, false, true): the ability, or the passive when p has one */
static bool has_ability(const pr_reward_mon *m, int a) { return m->ability == a || (m->passive && m->passive == a); }
static bool has_any_ability(const pr_reward_mon *m, const int *v, int n)
{
    for (int i = 0; i < n; i++)
        if (has_ability(m, v[i])) return true;
    return false;
}
static bool knows(const pr_reward_mon *m, int move)   /* getMoveset(true) */
{
    for (int i = 0; i < m->n_base_moves; i++)
        if (m->base_moves[i] == move) return true;
    return false;
}

/* The Toxic and Flame Orbs: a member holding neither orb that can get the status (canSetStatus: caller) and has
 * the orb's ability, a general one without the opposite one, Facade or Psycho Shift */
static int orb_weight(const pr_reward_ctx *ctx, bool toxic)
{
    static const int general[] = {ABILITY_ID_QUICK_FEET, ABILITY_ID_GUTS, ABILITY_ID_MARVEL_SCALE, ABILITY_ID_MAGIC_GUARD};
    static const int poison[] = {ABILITY_ID_TOXIC_BOOST, ABILITY_ID_POISON_HEAL};
    static const int burn[] = {ABILITY_ID_FLARE_BOOST};
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        if (pr_reward_held(ctx, p, PR_ITEM_FLAME_ORB, -1) || pr_reward_held(ctx, p, PR_ITEM_TOXIC_ORB, -1)) continue;
        const bool status_moves = knows(m, MOVE_ID_FACADE) || knows(m, MOVE_ID_PSYCHO_SHIFT);
        if (!(toxic ? m->can_toxic : m->can_burn)) continue;   /* hasItemMoves: always false (no item moves yet) */
        const bool general_ab = has_any_ability(m, general, 4);
        const bool specific = toxic ? has_any_ability(m, poison, 2) : has_any_ability(m, burn, 1);
        const bool opposite = toxic ? has_any_ability(m, burn, 1) : has_any_ability(m, poison, 2);
        if (specific || (general_ab && !opposite) || status_moves) return 10;
    }
    return 0;
}

/* Mystical Rock: a member not holding a full stack (getStackCount() === getMaxStackCount()) with a weather or terrain
 * ability or move */
static int mystical_rock_weight(const pr_reward_ctx *ctx)
{
    static const int abilities[] = {
        ABILITY_ID_DROUGHT, ABILITY_ID_ORICHALCUM_PULSE, ABILITY_ID_DRIZZLE, ABILITY_ID_SAND_STREAM,
        ABILITY_ID_SAND_SPIT, ABILITY_ID_SNOW_WARNING, ABILITY_ID_ELECTRIC_SURGE, ABILITY_ID_HADRON_ENGINE,
        ABILITY_ID_PSYCHIC_SURGE, ABILITY_ID_GRASSY_SURGE, ABILITY_ID_SEED_SOWER, ABILITY_ID_MISTY_SURGE,
    };
    static const int moves[] = {
        MOVE_ID_SUNNY_DAY, MOVE_ID_RAIN_DANCE, MOVE_ID_SANDSTORM, MOVE_ID_SNOWSCAPE, MOVE_ID_HAIL,
        MOVE_ID_CHILLY_RECEPTION, MOVE_ID_ELECTRIC_TERRAIN, MOVE_ID_PSYCHIC_TERRAIN, MOVE_ID_GRASSY_TERRAIN,
        MOVE_ID_MISTY_TERRAIN,
    };
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        const pr_owned_item *rock = pr_reward_held(ctx, p, PR_ITEM_MYSTICAL_ROCK, -1);
        if (rock && rock->stack == pr_items[PR_ITEM_MYSTICAL_ROCK].max_held) continue;
        if (has_any_ability(m, abilities, 12)) return 10;
        for (int i = 0; i < 10; i++)
            if (knows(m, moves[i])) return 10;
    }
    return 0;
}

/* isMax(): the form key (or the fusion's) is a G-Max or Eternamax one */
static bool form_is_max(int species, int form)
{
    const pr_reward_species *s = pr_reward_species_find(species);
    if (!s || !s->n_forms || form < 0 || form >= s->n_forms) return false;
    return pr_reward_forms[s->form_start + form].is_max;
}
static bool has_evolutions(int species)
{
    const pr_reward_species *s = pr_reward_species_find(species);
    return s && (s->flags & 1);
}

/* Eviolite: daily, or unlocked outside Fresh Start; a member that isn't G-Max'd whose species (or fusion) evolves and
 * doesn't hold one */
static int eviolite_weight(const pr_reward_ctx *ctx)
{
    if (!(ctx->run->mode == GAME_MODES_DAILY || (!has_challenge(ctx, CHALLENGES_FRESH_START) && ctx->unlocked_eviolite)))
        return 0;
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        const bool fusion = m->fusion_species >= 0;
        if (form_is_max(m->species, m->form) || (fusion && form_is_max(m->fusion_species, m->fusion_form))) continue;
        if (!has_evolutions(m->species) && !(fusion && has_evolutions(m->fusion_species))) continue;
        if (!pr_reward_held(ctx, p, PR_ITEM_EVIOLITE, -1)) return 10;
    }
    return 0;
}

/* Leek: a Farfetch'd, Galarian Farfetch'd or Sirfetch'd (or fusion) not holding a SpeciesCritBoosterModifier */
static bool leek_species(int s)
{
    return s == SPECIES_ID_FARFETCHD || s == SPECIES_ID_GALAR_FARFETCHD || s == SPECIES_ID_SIRFETCHD;
}
static int leek_weight(const pr_reward_ctx *ctx)
{
    for (int p = 0; p < ctx->n_party; p++) {
        const pr_reward_mon *m = &ctx->party[p];
        if (pr_reward_held(ctx, p, PR_ITEM_LEEK, -1)) continue;
        if (leek_species(m->species) || (m->fusion_species >= 0 && leek_species(m->fusion_species))) return 12;
    }
    return 0;
}

static int ceil_div(int a, int b) { return (a + b - 1) / b; }   /* Math.ceil(a / b), a >= 0 */
static int imin(int a, int b) { return a < b ? a : b; }
static int imax(int a, int b) { return a > b ? a : b; }

int pr_reward_weight(pr_reward_ctx *ctx, int item, int tier, int reroll_count)
{
    const pr_run *run = ctx->run;
    const int wave = ctx->wave;
    switch (item) {
    /* balls */
    case PR_ITEM_POKEBALL: return has_maximum_balls(ctx, POKEBALL_TYPE_POKEBALL) ? 0 : 6;
    case PR_ITEM_GREAT_BALL: return has_maximum_balls(ctx, POKEBALL_TYPE_GREAT_BALL) ? 0 : 6;
    case PR_ITEM_ULTRA_BALL: return has_maximum_balls(ctx, POKEBALL_TYPE_ULTRA_BALL) ? 0 : 15;
    case PR_ITEM_ROGUE_BALL: return has_maximum_balls(ctx, POKEBALL_TYPE_ROGUE_BALL) ? 0 : 16;
    case PR_ITEM_MASTER_BALL: return has_maximum_balls(ctx, POKEBALL_TYPE_MASTER_BALL) ? 0 : 24;
    /* healing */
    case PR_ITEM_POTION: return hp_members(ctx, 10, 0.875) * 3;
    case PR_ITEM_SUPER_POTION: return hp_members(ctx, 25, 0.75);
    case PR_ITEM_HYPER_POTION: return hp_members(ctx, 100, 0.625) * 3;
    case PR_ITEM_MAX_POTION: return hp_members(ctx, 100, 0.5);
    case PR_ITEM_FULL_RESTORE: return (hp_members(ctx, 100, 0.5) + status_members(ctx)) / 2;
    case PR_ITEM_FULL_HEAL: return status_members(ctx) * 6;
    case PR_ITEM_REVIVE: return min3(fainted_members(ctx)) * 9;
    case PR_ITEM_MAX_REVIVE: return min3(fainted_members(ctx)) * 3;
    case PR_ITEM_SACRED_ASH: return fainted_members(ctx) >= ceil_div(ctx->n_party, 2) ? 1 : 0;
    case PR_ITEM_ETHER: case PR_ITEM_ELIXIR: return pp_members(ctx) * 3;
    case PR_ITEM_MAX_ETHER: case PR_ITEM_MAX_ELIXIR: return pp_members(ctx);
    /* lures */
    case PR_ITEM_LURE: return lure_weight(ctx, 10, 2);
    case PR_ITEM_SUPER_LURE: return lure_weight(ctx, 15, 4);
    case PR_ITEM_MAX_LURE: return lure_weight(ctx, 30, 4);
    /* skipInLastClassicWaveOrDefault */
    case PR_ITEM_NUGGET: return skip_in_classic_after(ctx, 199, 5);
    case PR_ITEM_BIG_NUGGET: return skip_in_classic_after(ctx, 199, 12);
    case PR_ITEM_RELIC_GOLD: return skip_in_classic_after(ctx, 199, 2);
    case PR_ITEM_AMULET_COIN: return skip_in_classic_after(ctx, 199, 3);
    case PR_ITEM_CANDY_JAR: return skip_in_classic_after(ctx, 199, 5);
    case PR_ITEM_GOLDEN_PUNCH: return skip_in_classic_after(ctx, 199, 2);
    case PR_ITEM_IV_SCANNER: return skip_in_classic_after(ctx, 199, 4);
    case PR_ITEM_EXP_CHARM: return skip_in_classic_after(ctx, 199, 8);
    case PR_ITEM_EXP_SHARE: return skip_in_classic_after(ctx, 199, 10);
    case PR_ITEM_SUPER_EXP_CHARM: return skip_in_classic_after(ctx, 199, 8);
    case PR_ITEM_ABILITY_CHARM: return skip_in_classic_after(ctx, 189, 6);
    /* by wave */
    case PR_ITEM_EVOLUTION_ITEM: return imin(ceil_div(wave, 15), 8);
    case PR_ITEM_RARE_EVOLUTION_ITEM: return imin(ceil_div(wave, 15) * 4, 32);
    case PR_ITEM_FORM_CHANGE_ITEM: case PR_ITEM_RARE_FORM_CHANGE_ITEM: return imin(ceil_div(wave, 50), 4) * 6;
    case PR_ITEM_MEGA_BRACELET: case PR_ITEM_DYNAMAX_BAND: return imin(ceil_div(wave, 50), 4) * 9;
    case PR_ITEM_MAP: return pr_reward_is_classic(run) && wave < 180 ? 2 : 0;
    case PR_ITEM_TERA_ORB: return pr_reward_is_classic(run) ? 0 : imin(imax((wave / 50) * 2, 1), 4);
    case PR_ITEM_CATCHING_CHARM: return pr_reward_is_classic(run) ? 0 : 4;
    case PR_ITEM_LOCK_CAPSULE: return pr_reward_is_classic(run) ? 0 : 3;
    case PR_ITEM_MEMORY_MUSHROOM: {
        bool any = false;
        for (int p = 0; p < ctx->n_party; p++) any |= ctx->party[p].has_learnable_level_moves;
        if (!any) return 0;
        return imin(1 + pr_run_get_wave_for_difficulty(run, wave, true) / 30, 4);
    }
    case PR_ITEM_TERA_SHARD:
        for (int p = 0; p < ctx->n_party; p++)
            if (!pr_reward_tera_excluded_species(&ctx->party[p])) return 1;
        return 0;
    case PR_ITEM_DNA_SPLICERS:
        if (tier == MODIFIER_TIER_GREAT) {
            if (unfused_members(ctx) > 1) {
                if (run->mode == GAME_MODES_SPLICED_ENDLESS) return 4;
                if (pr_reward_is_classic(run) && ctx->fusions_boosted) return 2;
            }
            return 0;
        }
        return !(pr_reward_is_classic(run) && ctx->fusions_boosted) && run->mode != GAME_MODES_SPLICED_ENDLESS &&
                       unfused_members(ctx) > 1
                   ? 24
                   : 0;
    case PR_ITEM_VOUCHER: return run->mode == GAME_MODES_DAILY ? 0 : imax(1 - reroll_count, 0);
    case PR_ITEM_VOUCHER_PLUS: return run->mode == GAME_MODES_DAILY ? 0 : imax(3 - reroll_count, 0);
    case PR_ITEM_VOUCHER_PREMIUM:
        return run->mode != GAME_MODES_DAILY && !is_endless(run) && run->mode != GAME_MODES_SPLICED_ENDLESS
                   ? imax(5 - reroll_count * 2, 0)
                   : 0;
    case PR_ITEM_MINI_BLACK_HOLE:
        return run->mode == GAME_MODES_DAILY || (!has_challenge(ctx, CHALLENGES_FRESH_START) && ctx->unlocked_mini_black_hole)
                   ? 1
                   : 0;
    /* held items */
    case PR_ITEM_EVIOLITE: return eviolite_weight(ctx);
    case PR_ITEM_LEEK: return leek_weight(ctx);
    case PR_ITEM_TOXIC_ORB: return orb_weight(ctx, true);
    case PR_ITEM_FLAME_ORB: return orb_weight(ctx, false);
    case PR_ITEM_MYSTICAL_ROCK: return mystical_rock_weight(ctx);
    }
    /* the dump lists every weight function; one missing here is a port bug */
    ctx->run->unported |= PR_REWARD_UNPORTED_FUNC;
    return 0;
}
