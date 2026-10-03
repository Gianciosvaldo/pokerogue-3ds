/* The enemy's turn choice: EnemyCommandPhase.start (src/phases/enemy-command-phase.ts), the trainer's
 * switch-in scores (Trainer.getPartyMemberMatchupScores, getSortedPartyMemberMatchupScores,
 * getNextSummonIndex, shouldTera in src/field/trainer.ts), Pokemon.getMatchupScore / isTrapped /
 * isMoveSelectable, EnemyPokemon.getNextMove / getNextTargets (src/field/pokemon.ts) and
 * PokemonMove.isUsable (src/data/moves/pokemon-move.ts). */
#include "ai_internal.h"
#include <math.h>
#include <string.h>

const char *pr_ai_unported_name(uint16_t id)
{
    static const char *const names[] = {"AI: Illusion", "AI: benefit score", "AI: move restriction",
                                        "AI: ai_data tables"};
    if (id >= PR_FEAT_COUNT_ && id < PR_AI_FEAT_COUNT_) return names[id - PR_FEAT_COUNT_];
    return pr_unported_name(id);
}

/* ---- the per-Pokemon table ---- */

static const pr_ai_mon *mon_find(const pr_ai *ai, const pr_pokemon *p)
{
    for (int i = 0; ai && i < ai->n_mons; i++)
        if (ai->mons[i].id == p->id) return &ai->mons[i];
    return NULL;
}

int pr_ai_default_type(bool boss, bool has_trainer)
{
    return boss || has_trainer ? AI_TYPE_SMART : AI_TYPE_SMART_RANDOM;
}

pr_ai_mon *pr_ai_mon_of(pr_ai *ai, const pr_pokemon *p)
{
    pr_ai_mon *m = (pr_ai_mon *)mon_find(ai, p);
    if (m || ai->n_mons >= 2 * PR_MAX_PARTY) return m;
    m = &ai->mons[ai->n_mons++];
    *m = (pr_ai_mon){.id = p->id, .ai_type = (uint8_t)pr_ai_default_type(p->is_boss, p->has_trainer),
                     .initial_team_index = -1, .trainer_slot = TRAINER_SLOT_NONE};
    return m;
}

bool ai_ability_revealed(const pr_ai *ai, const pr_pokemon *p)
{
    const pr_ai_mon *m = p ? mon_find(ai, p) : NULL;
    return m && m->ability_revealed;
}

int ai_type_of(const pr_ai *ai, const pr_pokemon *p)
{
    const pr_ai_mon *m = mon_find(ai, p);
    return m ? m->ai_type : pr_ai_default_type(p->is_boss, p->has_trainer);
}

static int trainer_slot_of(const pr_ai *ai, const pr_pokemon *p)
{
    const pr_ai_mon *m = mon_find(ai, p);
    return m ? m->trainer_slot : TRAINER_SLOT_NONE;
}

/* ---- small Pokemon helpers ---- */

double ai_max_hp(const pr_pokemon *p) { return pr_get_stat(p, STAT_HP, true); }

bool ai_is_on_field(const pr_pokemon *p) { return p->on_field && !p->switch_out_status; }

/* isActive(onField): not fainted, allowed by the challenges (they only ever bar the player's
 * Pokemon, which can't be sent out then), and on the field when asked */
bool ai_is_active(const pr_pokemon *p, bool on_field)
{
    return p->hp > 0 && (!on_field || ai_is_on_field(p));
}

static double to_dmg_value(double v)
{
    v = floor(v);
    return v > 1 ? v : 1;
}

/* maxPpOverride || move.pp + ppUp * toDmgValue(move.pp / 5) */
double ai_move_pp(const pr_pokemon *p, int slot)
{
    if (p->max_pp_override[slot] > 0) return p->max_pp_override[slot];
    double pp = pr_moves[p->moves[slot]].pp;
    return pp + p->pp_up[slot] * to_dmg_value(pp / 5);
}

double ai_pp_ratio(const pr_pokemon *p, int slot) { return 1 - p->pp_used[slot] / ai_move_pp(p, slot); }

static bool out_of_pp(const pr_pokemon *p, int slot)
{
    double max = ai_move_pp(p, slot);
    return max != -1 && p->pp_used[slot] >= max;
}

void ai_note_illusion(pr_battle *bt, const pr_pokemon *p)
{
    if (p->ability == ABILITY_ID_ILLUSION || p->passive == ABILITY_ID_ILLUSION) pr_note_unported(bt, PR_AI_FEAT_ILLUSION);
}

static double js_max(double a, double b) { return a != a || b != b ? NAN : a > b ? a : b; }
static double js_min(double a, double b) { return a != a || b != b ? NAN : a < b ? a : b; }

/* battle.ts randSeedInt(range, 0) = integerInRange(0, range - 1) on the battle RNG, kept in double
 * arithmetic for a range that isn't a whole number */
double ai_battle_rand_real(pr_battle *bt, double range)
{
    if (range <= 1) return 0;
    if (!bt->battle_rng_live) {
        pr_seedstr s;
        pr_seed_shift(&s, &bt->battle_seed, bt->turn << 6);
        pr_rng_sow(&bt->battle_rng, &s);
        bt->battle_rng_live = true;
    }
    double max = range - 1 + 0;
    return floor(pr_rng_real_in_range(&bt->battle_rng, 0, max - 0 + 1) + 0);
}

/* The sort upstream's arrays get (V8's TimSort; under 64 elements one run, then binary insertion)
 * with the comparator `a < b ? 1 : a > b ? -1 : 0`: highest first, ties in their order */
typedef struct { double key; int idx; } ai_scored;
static int cmp_desc(const ai_scored *a, const ai_scored *b) { return a->key < b->key ? 1 : a->key > b->key ? -1 : 0; }

static void v8_sort(ai_scored *v, int n)
{
    if (n < 2) return;
    int run = 2;
    bool desc = cmp_desc(&v[1], &v[0]) < 0;
    for (int i = 2; i < n; i++) {
        int o = cmp_desc(&v[i], &v[i - 1]);
        if (desc ? o >= 0 : o < 0) break;
        run++;
    }
    if (desc)
        for (int a = 0, b = run - 1; a < b; a++, b--) { ai_scored t = v[a]; v[a] = v[b]; v[b] = t; }
    for (int start = run; start < n; start++) {
        int left = 0, right = start;
        ai_scored pivot = v[start];
        while (left < right) {
            int mid = left + ((right - left) >> 1);
            if (cmp_desc(&pivot, &v[mid]) < 0) right = mid;
            else left = mid + 1;
        }
        for (int p = start; p > left; p--) v[p] = v[p - 1];
        v[left] = pivot;
    }
}

/* ---- arena checks ---- */

static bool is_field_targeted(int move)
{
    int t = pr_moves[move].target;
    return t == MOVE_TARGET_BOTH_SIDES || t == MOVE_TARGET_USER_SIDE || t == MOVE_TARGET_ENEMY_SIDE;
}

bool ai_move_weather_cancelled(pr_battle *bt, const pr_pokemon *user, int move)
{
    if (bt->weather == WEATHER_TYPE_NONE || pr_weather_effect_suppressed(bt)) return false;
    int type = pr_get_move_type(bt, user, move);
    switch (bt->weather) {
    case WEATHER_TYPE_HARSH_SUN: return pr_move_is_attack(move) && type == POKEMON_TYPE_WATER;
    case WEATHER_TYPE_HEAVY_RAIN: return pr_move_is_attack(move) && type == POKEMON_TYPE_FIRE;
    }
    return false;
}

/* Psychic Terrain: a priority move at a grounded opponent on the field */
bool ai_move_terrain_cancelled(pr_battle *bt, const pr_pokemon *user, const int8_t *targets, int n, int move)
{
    if (bt->terrain != TERRAIN_PSYCHIC) return false;
    if (is_field_targeted(move) || pr_move_get_priority(bt, move, user) <= 0) return false;
    for (int i = user->is_player ? 2 : 0, end = i + 2; i < end; i++) {
        const pr_pokemon *o = bt->field[i];
        if (!o || !ai_is_active(o, true)) continue;
        bool listed = false;
        for (int k = 0; k < n; k++) listed |= targets[k] == i;
        if (listed && pr_is_grounded(bt, o, false)) return true;
    }
    return false;
}

double ai_move_chance(pr_battle *bt, int attr_index, const pr_pokemon *user, const pr_pokemon *target, int move,
                      bool self_effect, bool show_ability)
{
    const pr_effect_info *ef = &pr_attr_effects[attr_index];
    double chance = ef->chance_override >= 0 ? ef->chance_override : pr_moves[move].chance;
    pr_ab_params up = {.pokemon = user, .move = move, .simulated = !show_ability, .value = &chance};
    pr_apply_ab_attrs(bt, PR_AC_MoveEffectChanceMultiplierAbAttr, &up);
    if ((!pr_move_has_attr((uint16_t)move, PR_AC_FlinchAttr) || chance <= pr_moves[move].chance)
        && !pr_move_has_attr((uint16_t)move, PR_AC_SecretPowerAttr)) {
        int side = user->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
        for (int i = 0; i < bt->n_arena_tags; i++)
            if (bt->arena_tags[i].type == ARENA_TAG_TYPE_WATER_FIRE_PLEDGE && pr_arena_tag_on_side(&bt->arena_tags[i], side))
                chance *= 2;
    }
    if (!self_effect) {
        pr_ab_params tp = {.pokemon = target, .move = move, .simulated = !show_ability, .value = &chance};
        pr_apply_ab_attrs(bt, PR_AC_IgnoreMoveEffectsAbAttr, &tp);
    }
    return chance;
}

/* move.applyConditions(user, target, -1): sequences 2, 3 and 4 */
static bool conditions_all(pr_battle *bt, int move, const pr_pokemon *user, const pr_pokemon *target)
{
    return pr_move_apply_conditions(bt, move, user, target, 2) && pr_move_apply_conditions(bt, move, user, target, 3)
           && pr_move_apply_conditions(bt, move, user, target, 4);
}

/* ---- move selection rules ---- */

static bool in_moveset(const pr_pokemon *p, int move)
{
    for (int i = 0; i < p->n_moves; i++)
        if (p->moves[i] == move) return true;
    return false;
}

/* MoveRestrictionBattlerTag.isMoveRestricted(move, p) for p's tag t */
static bool tag_restricts(pr_battle *bt, const pr_pokemon *p, const pr_battler_tag *t, int move)
{
    switch (t->type) {
    case BATTLER_TAG_TYPE_THROAT_CHOPPED: return (pr_moves[move].flags & MOVE_FLAGS_SOUND_BASED) != 0;
    case BATTLER_TAG_TYPE_DISABLED: return move == t->move_id;
    case BATTLER_TAG_TYPE_GORILLA_TACTICS:
    case BATTLER_TAG_TYPE_ENCORE: return move != t->move_id;
    case BATTLER_TAG_TYPE_TAUNT: return pr_moves[move].category == MOVE_CATEGORY_STATUS;
    case BATTLER_TAG_TYPE_HEAL_BLOCK:
        for (unsigned i = 0; i < pr_set_heal_blocked_moves_n; i++)
            if (pr_set_heal_blocked_moves[i] == move) return true;
        return false;
    case BATTLER_TAG_TYPE_TORMENT: {
        const pr_turn_move *last = pr_last_move(bt, p, 0);
        if (!last) return false;
        bool unaffected = pr_move_has_attr(last->move, PR_AC_ConsecutiveUseDoublePowerAttr)
                          || pr_get_battler_tag(p, BATTLER_TAG_TYPE_FRENZY);
        bool valid = last->result == MOVE_RESULT_SUCCESS || last->result == MOVE_RESULT_MISS;
        return last->move == move && valid && last->move != MOVE_ID_STRUGGLE && !unaffected;
    }
    case BATTLER_TAG_TYPE_IMPRISON: {
        const pr_pokemon *src = pr_pokemon_by_id(bt, t->source_id);
        return src && in_moveset(src, move) && ai_is_active(src, true);
    }
    }
    return false;
}

/* Move.checkRestrictions(user): restricted? */
static bool move_restricted(pr_battle *bt, const pr_pokemon *p, int move)
{
    unsigned r = ai_moves[move].restrictions;
    bool restricted = false;
    if (r & AI_RESTR_GRAVITY) restricted |= pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_GRAVITY, ARENA_TAG_SIDE_BOTH) != NULL;
    if (r & AI_RESTR_CONSECUTIVE) {
        const pr_turn_move *last = pr_last_move(bt, p, 0);
        restricted |= last && last->move == move;
    }
    if (r & AI_RESTR_BELCH) restricted |= !p->has_eaten_berry;
    if (r & AI_RESTR_NO_BERRY) {
        /* Upstream checks every BerryModifier on the user's side, without filtering its owner
         * or stack count. An unknown held-item subclass may still be a berry. */
        int side = p->is_player ? 0 : 1;
        bool berry = false, maybe_berry = false;
        for (int i = 0; i < bt->n_mods[side]; i++) {
            berry |= pr_mod_is(&bt->mods[side][i], PR_MOD_BERRY);
            maybe_berry |= pr_mod_is(&bt->mods[side][i], PR_MOD_HELD_ITEM) && bt->mods[side][i].unknown_subclass;
        }
        if (!berry) {
            if (maybe_berry) pr_note_unported(bt, PR_AI_FEAT_RESTRICTION);
            else restricted = true;
        }
    }
    if (r & AI_RESTR_UNKNOWN) pr_note_unported(bt, PR_AI_FEAT_RESTRICTION);
    return restricted;
}

bool pr_ai_move_selectable(pr_battle *bt, const pr_pokemon *p, int move)
{
    for (int i = 0; i < p->n_tags; i++)
        if (tag_restricts(bt, p, &p->tags[i], move)) return false;
    return !move_restricted(bt, p, move);
}

bool pr_ai_move_usable(pr_battle *bt, const pr_pokemon *p, int slot, bool ignore_pp, bool for_selection)
{
    int move = p->moves[slot];
    if (move == MOVE_ID_NONE || pr_moves[move].unimplemented) return false;
    if (!ignore_pp && out_of_pp(p, slot)) return false;
    if (for_selection && !pr_ai_move_selectable(bt, p, move)) return false;
    return true;   /* the player's challenge rules (POKEMON_MOVE) aren't modelled */
}

/* ---- trapping ---- */

/* ArenaTrapAbAttr's arenaTrapCondition closures (Shadow Tag, Magnet Pull, Arena Trap), checked
 * against the upstream source they were written for; any other goes through battle-core */
static bool arena_trap_condition(pr_battle *bt, uint16_t fn, const pr_attr *a, const pr_pokemon *user,
                                 const pr_pokemon *target)
{
    if (pr_closure_matches(fn, "ability:23", "2f8bc1c25e68"))       /* !target.hasAbility(SHADOW_TAG) */
        return !pr_has_ability_active(bt, target, ABILITY_ID_SHADOW_TAG);
    if (pr_closure_matches(fn, "ability:42", "4ef22f394ce6")) {     /* target.isOfType(STEEL, {returnOriginalTypesIfStellar}) */
        int8_t t[PR_MAX_TYPES];
        int n = pr_get_types(target, t, PR_TYPES_ORIGINAL_IF_STELLAR);
        for (int i = 0; i < n; i++)
            if (t[i] == POKEMON_TYPE_STEEL) return true;
        return false;
    }
    if (pr_closure_matches(fn, "ability:71", "8e2c012e822a"))       /* target.isGrounded() */
        return pr_is_grounded(bt, target, false);
    return pr_closure_call(bt, fn, a->cls, 0, user, target, -1) != 0;
}

/* applyAbAttrs("CheckTrappedAbAttr", {pokemon: opp, trapped, opponent: p, simulated: true}) */
static void check_trapped(pr_battle *bt, const pr_pokemon *opp, const pr_pokemon *p, bool *trapped)
{
    for (int passive = 0; passive < 2; passive++) {
        uint16_t ability = passive ? opp->passive : opp->ability;
        if (passive && (!opp->passive || opp->passive == opp->ability)) continue;
        const pr_ability *ab = &pr_abilities[ability];
        bool checked = false, can = false;
        for (unsigned i = 0; i < ab->nattr; i++) {
            const pr_attr *a = &pr_attrs[ab->attr0 + i];
            if (!pr_attr_is(a->cls, PR_AC_CheckTrappedAbAttr)) continue;
            if (!checked) { can = pr_can_apply_ability(bt, opp, ability); checked = true; }
            if (!can) break;
            if (a->cond != PR_NO_FN && !pr_closure_call(bt, a->cond, a->cls, 1, opp, NULL, -1)) continue;
            if (a->cls != PR_AC_ArenaTrapAbAttr) continue;   /* CheckTrappedAbAttr itself does nothing */
            uint16_t fn = ((const pr_attr_ArenaTrapAbAttr *)a->p)->arenaTrapCondition;
            int8_t t[PR_MAX_TYPES];
            int n = pr_get_types(p, t, PR_TYPES_ORIGINAL_IF_STELLAR);
            bool ghost = false;
            for (int k = 0; k < n; k++) ghost |= t[k] == POKEMON_TYPE_GHOST;
            if (arena_trap_condition(bt, fn, a, opp, p) && !ghost && !pr_has_ability_active(bt, p, ABILITY_ID_RUN_AWAY))
                *trapped = true;
        }
    }
}

bool pr_ai_is_trapped(pr_battle *bt, pr_ai *ai, pr_pokemon *p)
{
    (void)ai;
    const pr_battler_tag *cmd = pr_get_battler_tag(p, BATTLER_TAG_TYPE_COMMANDED);
    if (cmd) {
        const pr_pokemon *src = pr_pokemon_by_id(bt, cmd->source_id);
        if (src && ai_is_active(src, true)) return true;
    }
    if (pr_is_of_type(p, POKEMON_TYPE_GHOST) || pr_has_ability_active(bt, p, ABILITY_ID_RUN_AWAY)) return false;
    bool trapped = false;
    pr_speed_iter it;
    pr_pokemon *opp;
    pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_ENEMY : ARENA_TAG_SIDE_PLAYER);
    while ((opp = pr_speed_iter_next(bt, &it)))
        if (!opp->switch_out_status) check_trapped(bt, opp, p, &trapped);
    int side = p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY;
    return trapped || pr_find_battler_tag_class(p, PR_TAGCLASS_TRAPPED)
           || pr_arena_get_tag_on_side(bt, ARENA_TAG_TYPE_FAIRY_LOCK, side);
}

/* ---- matchup scores ---- */

double pr_ai_matchup_score(pr_battle *bt, pr_ai *ai, pr_pokemon *p, pr_pokemon *opponent)
{
    (void)ai;
    ai_note_illusion(bt, opponent);                          /* opponent.getTypes({useIllusion: true}) */
    int8_t et[PR_MAX_TYPES];
    int ne = pr_get_types(opponent, et, 0);
    pr_stat_params mine = {.opponent = opponent, .move = -1, .simulated = true};
    pr_stat_params theirs = {.opponent = p, .move = -1, .simulated = true};
    double my_spd = ai_is_active(p, true) ? pr_get_effective_stat_p(bt, p, STAT_SPD, &mine) : pr_get_stat(p, STAT_SPD, false);
    bool outspeed = my_spd >= pr_get_effective_stat_p(bt, opponent, STAT_SPD, &theirs);

    double def = 1 / js_max(pr_get_attack_type_effectiveness(bt, p, et[0], opponent, -1), 0.25);
    if (ne > 1) {
        ai_note_illusion(bt, p);
        def /= js_max(pr_get_attack_type_effectiveness_ex(bt, p, et[1], opponent, -1, false), 0.25);
    }

    double atk = 0;
    int n_atk = 0;
    for (int s = 0; s < p->n_moves; s++) {
        const pr_move *m = &pr_moves[p->moves[s]];
        if (m->category == MOVE_CATEGORY_STATUS || ai_pp_ratio(p, s) <= 0) continue;
        double score = pr_get_attack_type_effectiveness(bt, opponent, m->type, p, -1);
        if (pr_is_of_type(p, m->type) && !pr_move_has_attr(p->moves[s], PR_AC_VariableMoveTypeAttr)) score *= 1.5;
        atk += score;
        n_atk++;
    }
    atk /= n_atk ? n_atk : 1;

    double hp = pr_get_hp_ratio(p, false), opp_hp = pr_get_hp_ratio(opponent, false);
    bool dying = hp <= 0.2;
    double diff = hp + (1 - opp_hp);
    if (dying && ai_is_active(p, true)) {
        bool bad = atk < 1.5 && def < 1.5;
        if (!outspeed && bad) diff *= 0.85;
        else diff = 1 - hp + (outspeed ? 0.2 : 0.1);
    } else if (outspeed) {
        diff *= 1.25;
    } else if (hp > 0.2 && hp <= 0.4) {
        diff *= 0.5;
    }
    return (atk + def) * js_min(diff, 1);
}

static double linear(double p0, double p1, double t) { return (p1 - p0) * t + p0; }

/* EntryHazardTag.getMatchupScoreMultiplier (upstream leaves a grounded Pokemon at 1) */
static double hazard_base(pr_battle *bt, const pr_arena_tag *t, const pr_pokemon *p)
{
    if (pr_is_grounded(bt, p, false)) return 1;
    return linear(0, 1 / js_pow(2, t->layers), fmin(pr_get_hp_ratio(p, false), 0.5) * 2);
}

static double hazard_multiplier(pr_battle *bt, const pr_arena_tag *t, pr_pokemon *p)
{
    switch (t->type) {
    case ARENA_TAG_TYPE_STEALTH_ROCK: {
        double r = 0.125 * pr_get_attack_type_effectiveness_opts(bt, p, POKEMON_TYPE_ROCK, NULL, -1, true, true);
        return linear(hazard_base(bt, t, p), 1, 1 - js_pow(r, r));
    }
    case ARENA_TAG_TYPE_TOXIC_SPIKES:
        if (pr_is_grounded(bt, p, false) || !pr_can_set_status(bt, NULL, p, STATUS_EFFECT_POISON, true, false, NULL, false))
            return 1;
        if (pr_is_of_type(p, POKEMON_TYPE_POISON)) return 1.25;
        return hazard_base(bt, t, p);
    }
    return hazard_base(bt, t, p);   /* Spikes, Sticky Web, Imprison */
}

/* getPlayerField().filter(p => p.isAllowedInBattle()) */
static int player_field(pr_battle *bt, pr_pokemon *out[2])
{
    int count = bt->double_battle ? 2 : 1, n = 0;
    for (int i = 0; i < count; i++) {
        pr_pokemon *q = bt->n_party[0] ? (i < bt->n_party[0] ? bt->party[0][i] : NULL) : bt->field[i];
        if (q && ai_is_active(q, false)) out[n++] = q;
    }
    return n;
}

/* Trainer.getPartyMemberMatchupScores(trainerSlot, forSwitch): [party index, score] */
static int party_member_scores(pr_battle *bt, pr_ai *ai, int trainer_slot, bool for_switch, ai_scored out[PR_MAX_PARTY])
{
    if (trainer_slot && !ai->trainer->is_double) trainer_slot = TRAINER_SLOT_NONE;
    if (!bt->n_party[1]) { pr_note_unported(bt, PR_FEAT_PARTY); return 0; }
    int n = 0;
    for (int k = bt->double_battle ? 2 : 1; k < bt->n_party[1]; k++) {
        pr_pokemon *q = bt->party[1][k];
        if (!ai_is_active(q, false)) continue;
        if (trainer_slot && trainer_slot_of(ai, q) != trainer_slot) continue;
        pr_pokemon *pf[2];
        int npf = player_field(bt, pf);
        double score = 0;
        if (npf > 0) {
            for (int i = 0; i < npf; i++) {
                score += pr_ai_matchup_score(bt, ai, q, pf[i]);
                if (pr_pokemon_species(pf[i])->rarity & 2) score /= 2;   /* species.legendary */
            }
            score /= npf;
            if (for_switch && !ai_is_on_field(q))
                for (int i = 0; i < bt->n_arena_tags; i++) {
                    const pr_arena_tag *t = &bt->arena_tags[i];
                    if (pr_arena_tag_is(t->type, PR_ARENACLASS_ENTRY_HAZARD) && pr_arena_tag_on_side(t, ARENA_TAG_SIDE_ENEMY))
                        score *= hazard_multiplier(bt, t, q);
                }
        }
        out[n++] = (ai_scored){score, k};
    }
    return n;
}

/* Trainer.getNextSummonIndex: the best score, a tie settled by a roll on the session seed */
static int next_summon_index(pr_battle *bt, pr_ai *ai, const ai_scored *scores, int n)
{
    ai_scored sorted[PR_MAX_PARTY];
    memcpy(sorted, scores, sizeof(ai_scored) * (size_t)n);
    v8_sort(sorted, n);
    int best[PR_MAX_PARTY], nb = 0;
    for (int i = 0; i < n; i++)
        if (scores[i].key == sorted[0].key) best[nb++] = scores[i].idx;
    if (nb > 1) {
        if (!ai->seed.len) { pr_note_unported(bt, PR_FEAT_WAVE); return best[0]; }
        pr_rng r;
        pr_seedstr s;
        pr_seed_shift(&s, &ai->seed, bt->turn << 2);        /* executeWithSeedOffset(.., turn << 2) */
        pr_rng_sow(&r, &s);
        return best[pr_rng_seed_int(&r, nb, 0)];
    }
    return nb ? best[0] : -1;
}

int pr_ai_next_summon_index(pr_battle *bt, pr_ai *ai, int trainer_slot)
{
    if (!ai->trainer) return -1;
    ai_scored scores[PR_MAX_PARTY];
    int n = party_member_scores(bt, ai, trainer_slot, false, scores);
    return n ? next_summon_index(bt, ai, scores, n) : -1;
}

int pr_ai_party_matchup_scores(pr_battle *bt, pr_ai *ai, int trainer_slot, bool for_switch,
                               int slots[PR_MAX_PARTY], double scores[PR_MAX_PARTY])
{
    if (!ai->trainer) return 0;
    ai_scored sc[PR_MAX_PARTY];
    int n = party_member_scores(bt, ai, trainer_slot, for_switch, sc);
    for (int i = 0; i < n; i++) slots[i] = sc[i].idx, scores[i] = sc[i].key;
    return n;
}

/* ---- targets and moves ---- */

int pr_ai_get_next_targets(pr_battle *bt, pr_ai *ai, pr_pokemon *p, int move, int8_t out[4])
{
    int8_t t[4];
    bool multiple = false;
    int nt = pr_get_move_targets(bt, p, move, -1, t, &multiple);
    ai_scored cand[4];
    int nc = 0;
    for (int i = 0; i < 4; i++) {
        const pr_pokemon *q = bt->field[i];
        if (!q || !ai_is_active(q, false)) continue;   /* getField(true) */
        for (int k = 0; k < nt; k++)
            if (t[k] == i) { cand[nc++] = (ai_scored){0, i}; break; }
    }
    if (multiple) {
        for (int i = 0; i < nc; i++) out[i] = (int8_t)cand[i].idx;
        return nc;
    }
    for (int i = 0; i < nc; i++) {
        pr_pokemon *q = bt->field[cand[i].idx];
        cand[i].key = pr_ai_target_benefit_score(bt, ai, p, q, move) * (q->is_player == p->is_player ? 1 : -1);
    }
    v8_sort(cand, nc);
    if (nc == 0) {
        if (pr_move_has_attr((uint16_t)move, PR_AC_CounterDamageAttr)) { out[0] = BATTLER_INDEX_ATTACKER; return 1; }
        return 0;
    }
    double w[4];
    for (int i = 0; i < nc; i++) w[i] = cand[i].key;
    double lowest = w[nc - 1];
    if (lowest < 1)
        for (int i = 0; i < nc; i++) w[i] += fabs(lowest - 1);
    int nw = nc;
    for (int i = 0; i < nc; i++)
        if (w[i] < w[0] / 2) { nw = i; break; }
    double th[4], total = 0;
    for (int i = 0; i < nw; i++) { total += w[i]; th[i] = total; }
    double roll = ai_battle_rand_real(bt, total);
    int index = 0;   /* thresholds.every(...): stays 0 when the roll passes every threshold */
    for (int i = 0; i < nw; i++) {
        if (roll >= th[i]) continue;
        index = i;
        break;
    }
    out[0] = (int8_t)cand[index].idx;
    return 1;
}

static pr_turn_move turn_move(int move, const int8_t *targets, int n, int use_mode)
{
    pr_turn_move m = {.move = (uint16_t)move, .use_mode = (uint8_t)use_mode, .result = PR_MOVE_RESULT_UNDEFINED,
                      .n_targets = (uint8_t)n};
    memcpy(m.targets, targets, (size_t)n);
    return m;
}

static bool ignores_failure(int move)
{
    return move == MOVE_ID_SUCKER_PUNCH || move == MOVE_ID_UPPER_HAND || move == MOVE_ID_THUNDERCLAP;
}

/* The KO filter: can this move knock out a Pokemon on the other side this turn? */
static bool move_can_ko(pr_battle *bt, pr_ai *ai, pr_pokemon *p, int move)
{
    const pr_move *m = &pr_moves[move];
    if (m->target == MOVE_TARGET_ATTACKER) return false;
    int8_t t[4];
    bool multiple;
    int nt = pr_get_move_targets(bt, p, move, -1, t, &multiple);
    bool crit = pr_move_has_attr((uint16_t)move, PR_AC_CritOnlyAttr) || pr_get_battler_tag(p, BATTLER_TAG_TYPE_ALWAYS_CRIT);
    if (m->category == MOVE_CATEGORY_STATUS) return false;
    for (int k = 0; k < nt; k++) {
        if (t[k] < 0 || t[k] > 3) continue;
        pr_pokemon *q = bt->field[t[k]];
        if (!q || q->is_player == p->is_player) continue;
        bool ok = !ai_move_weather_cancelled(bt, p, move) && !ai_move_terrain_cancelled(bt, p, &t[k], 1, move)
                  && (conditions_all(bt, move, p, q) || ignores_failure(move));
        if (!ok) continue;
        const pr_pokemon *ally = pr_get_ally(bt, q);
        pr_damage_result d = ai_attack_damage(bt, q, p, move, !ai_ability_revealed(ai, q),
                                              !(ally && ai_ability_revealed(ai, ally)), crit);
        if (d.damage >= q->hp) return true;
    }
    return false;
}

pr_turn_move pr_ai_get_next_move(pr_battle *bt, pr_ai *ai, pr_pokemon *p)
{
    int8_t tg[4];
    ai->n_move_scores = 0;
    /* a queued move: the first one that was called indirectly or is still in the moveset (upstream
     * tests isUsable()'s [usable, text] tuple, which is always truthy) */
    for (int i = 0; i < p->n_move_queue; i++) {
        const pr_turn_move *q = &p->move_queue[i];
        if (q->use_mode >= MOVE_USE_MODE_INDIRECT || in_moveset(p, q->move)) {
            pr_turn_move out = *q;
            memmove(&p->move_queue[0], &p->move_queue[i], sizeof(pr_turn_move) * (size_t)(p->n_move_queue - i));
            p->n_move_queue = (uint8_t)(p->n_move_queue - i);
            return out;
        }
    }
    p->n_move_queue = 0;

    int pool[4], n = 0;
    for (int s = 0; s < p->n_moves; s++)
        if (pr_ai_move_usable(bt, p, s, false, true)) pool[n++] = s;
    if (n > 0) {
        if (n == 1) {
            int move = p->moves[pool[0]];
            return turn_move(move, tg, pr_ai_get_next_targets(bt, ai, p, move, tg), MOVE_USE_MODE_NORMAL);
        }
        const pr_battler_tag *encore = pr_get_battler_tag(p, BATTLER_TAG_TYPE_ENCORE);
        if (encore)
            for (int k = 0; k < n; k++)
                if (p->moves[pool[k]] == encore->move_id) {
                    int move = p->moves[pool[k]];
                    return turn_move(move, tg, pr_ai_get_next_targets(bt, ai, p, move, tg), MOVE_USE_MODE_NORMAL);
                }
        int type = ai_type_of(ai, p);
        if (type == AI_TYPE_RANDOM) {
            int move = p->moves[pool[pr_battle_rand_seed_int(bt, n, 0)]];
            return turn_move(move, tg, pr_ai_get_next_targets(bt, ai, p, move, tg), MOVE_USE_MODE_NORMAL);
        }
        if (type == AI_TYPE_SMART_RANDOM || type == AI_TYPE_SMART) {
            int ko[4], nko = 0;
            for (int k = 0; k < n; k++)
                if (move_can_ko(bt, ai, p, p->moves[pool[k]])) ko[nko++] = pool[k];
            if (nko > 0) {
                memcpy(pool, ko, sizeof(int) * (size_t)nko);
                n = nko;
            }
            /* moveTargets: one getNextTargets per pool entry, in order; a move listed twice keeps the last */
            int8_t targets[4][4];
            int n_targets[4];
            for (int k = 0; k < n; k++)
                n_targets[k] = pr_ai_get_next_targets(bt, ai, p, p->moves[pool[k]], targets[k]);
            int of_move[4];
            for (int k = 0; k < n; k++) {
                of_move[k] = k;
                for (int j = k + 1; j < n; j++)
                    if (p->moves[pool[j]] == p->moves[pool[k]]) of_move[k] = j;
            }
            ai_scored scores[4];
            for (int k = 0; k < n; k++) {
                int move = p->moves[pool[k]];
                const pr_move *m = &pr_moves[move];
                int tl = of_move[k];
                double best = -INFINITY;
                for (int j = 0; j < n_targets[tl]; j++) {
                    int mt = targets[tl][j];
                    if (mt == BATTLER_INDEX_ATTACKER) break;
                    pr_pokemon *target = mt >= 0 && mt < 4 ? bt->field[mt] : NULL;
                    if (!target) continue;
                    double ts = pr_ai_user_benefit_score(bt, ai, p, target, move)
                                + pr_ai_target_benefit_score(bt, ai, p, target, move) * ((mt < 2) == p->is_player ? 1 : -1);
                    if (ts != ts) ts = 0;
                    if ((m->unimplemented || !conditions_all(bt, move, p, target)) && !ignores_failure(move)) {
                        ts = -20;
                    } else if (ai_move_weather_cancelled(bt, p, move)
                               || ai_move_terrain_cancelled(bt, p, &targets[tl][j], 1, move)) {
                        ts = -20;
                    } else if (pr_move_is_attack(move)) {
                        double eff = ai_move_effectiveness(bt, target, p, move, !ai_ability_revealed(ai, target), true, NULL);
                        if (target->is_player != p->is_player) {
                            ts *= eff;
                            if (pr_is_of_type(p, m->type)) ts *= 1.5;
                        } else if (eff) {
                            ts /= eff;
                            if (pr_is_of_type(p, m->type)) ts /= 1.5;
                        }
                        if (!(ts != 0 && ts == ts)) ts = -20;
                    }
                    best = best != best || ts != ts ? NAN : ts > best ? ts : best;   /* Math.max */
                }
                scores[k] = (ai_scored){0 + best, k};
            }
            for (int k = 0; k < n; k++) ai->move_scores[k] = scores[k].key;
            ai->n_move_scores = (uint8_t)n;
            ai_scored sorted[4];
            memcpy(sorted, scores, sizeof(ai_scored) * (size_t)n);
            v8_sort(sorted, n);
            int chosen = 0;
            if (type == AI_TYPE_SMART_RANDOM) {
                while (chosen < n - 1 && pr_battle_rand_seed_int(bt, 8, 0) >= 5) chosen++;
            } else {
                while (chosen < n - 1) {
                    double ratio = sorted[chosen + 1].key / sorted[chosen].key;
                    if (!(ratio >= 0) || !(pr_battle_rand_seed_int(bt, 100, 0) < js_round(ratio * 50))) break;
                    chosen++;
                }
            }
            int k = sorted[chosen].idx, tl = of_move[k];
            return turn_move(p->moves[pool[k]], targets[tl], n_targets[tl], MOVE_USE_MODE_NORMAL);
        }
        /* an AI type upstream's switch has no case for falls through to Struggle */
    }
    return turn_move(MOVE_ID_STRUGGLE, tg, pr_ai_get_next_targets(bt, ai, p, MOVE_ID_STRUGGLE, tg), MOVE_USE_MODE_IGNORE_PP);
}

/* ---- EnemyCommandPhase ---- */

static bool should_tera(pr_battle *bt, pr_ai *ai, const pr_pokemon *p)
{
    const pr_ai_trainer *tr = ai->trainer;
    if (!tr || tr->tera_mode != PR_TERA_AI_INSTANT_TERA || p->terastallized) return false;
    const pr_ai_mon *m = mon_find(ai, p);
    int index = m ? m->initial_team_index : -1;
    bool listed = false;
    for (int i = 0; i < tr->n_instant_teras; i++) listed |= tr->instant_teras[i] == index;
    if (!listed) return false;
    for (int i = 0; i < bt->n_faint_history[1]; i++)
        if (bt->faint_history[1][i].id == p->id) return false;
    return true;
}

static struct pr_turn_command empty_command(int command)
{
    struct pr_turn_command c;
    memset(&c, 0, sizeof c);
    c.command = (int8_t)command;
    c.no_move = true;
    c.cursor = PR_CURSOR_UNDEFINED;
    return c;
}

void pr_ai_enemy_command(pr_battle *bt, pr_ai *ai, int field_index)
{
    int bi = BATTLER_INDEX_ENEMY + field_index;
    pr_pokemon *p = bt->field[bi];
    if (!p) { pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT); return; }
    bool skip = ai->skip_enemy_turns;
    if (bt->double_battle && pr_has_ability_active(bt, p, ABILITY_ID_COMMANDER)) {
        const pr_pokemon *ally = pr_get_ally(bt, p);
        if (ally && pr_get_battler_tag(ally, BATTLER_TAG_TYPE_COMMANDED)) skip = true;
    }

    /* a trainer's Pokemon switches out when a party member's matchup is 3x as good (2x for a boss
     * trainer), less eagerly right after a switch */
    if (ai->trainer && p->n_move_queue == 0 && !pr_ai_is_trapped(bt, ai, p)) {
        ai_scored scores[PR_MAX_PARTY];
        int n = party_member_scores(bt, ai, trainer_slot_of(ai, p), true, scores);
        if (n > 0) {
            double sum = 0;
            int n_opp = 0;
            for (int i = p->is_player ? 2 : 0, end = i + (bt->double_battle ? 2 : 1); i < end; i++) {   /* getOpponents() */
                pr_pokemon *o = bt->field[i];
                if (!o || !ai_is_active(o, true)) continue;
                sum += pr_ai_matchup_score(bt, ai, p, o);
                n_opp++;
            }
            double matchup = sum / n_opp;   /* NaN with no opponent: no switch, as upstream */
            ai_scored sorted[PR_MAX_PARTY];
            memcpy(sorted, scores, sizeof(ai_scored) * (size_t)n);
            v8_sort(sorted, n);
            int counter = ai->enemy_switch_counter;
            double mult = 1 - (counter ? js_pow(0.1, 1.0 / counter) : 0);
            if (sorted[0].key * mult >= matchup * (ai->trainer->is_boss ? 2 : 3)) {
                struct pr_turn_command c = empty_command(COMMAND_POKEMON);
                c.cursor = (int8_t)next_summon_index(bt, ai, scores, n);
                c.skip = skip;
                bt->commands[bi] = c;
                ai->enemy_switch_counter++;
                return;
            }
        }
    }

    pr_turn_move m = pr_ai_get_next_move(bt, ai, p);
    if (should_tera(bt, ai, p)) bt->pre_commands[bi] = empty_command(COMMAND_TERA);
    struct pr_turn_command c = empty_command(COMMAND_FIGHT);
    c.no_move = false;
    c.move = m.move;
    memcpy(c.targets, m.targets, sizeof c.targets);
    c.n_targets = m.n_targets;
    c.use_mode = m.use_mode;
    c.skip = skip;
    bt->commands[bi] = c;
    ai->enemy_switch_counter = ai->enemy_switch_counter - 1 > 0 ? ai->enemy_switch_counter - 1 : 0;
}
