/* Arena tags being added, lapsed and removed: arena.addTag, removeTagOnSide, removeTagsOnSide and
 * lapseTags (src/field/arena.ts) and, per tag type, what getArenaTag() builds (src/data/arena-tag.ts):
 * its constructor's fields, onAdd, onOverlap, lapse and onRemove; and ConditionalProtectTag.apply
 * (Quick Guard, Wide Guard, Mat Block, Crafty Shield), which MoveEffectPhase.protectedCheck runs.
 *
 * Imprison's and Neutralizing Gas's own onAdd, onOverlap, lapse and onRemove, and the pending heals'
 * lapse, aren't ported: reaching one reports PR_FEAT_ARENA_TAG_TURN. */
#include "pr/engine.h"
#include <math.h>
#include <string.h>

static void note(pr_battle *bt) { pr_note_unported(bt, PR_FEAT_ARENA_TAG_TURN); }

int pr_arena_max_layers(int type)
{
    switch (type) {
    case ARENA_TAG_TYPE_SPIKES: return 3;
    case ARENA_TAG_TYPE_TOXIC_SPIKES: return 2;
    default: return 1;                   /* Stealth Rock, Sticky Web, Imprison */
    }
}

/* getArenaTag(tagType, turnCount, sourceMove, sourceId, side): each constructor's own turn count,
 * move and side where it ignores the ones given; false for NONE (null) */
static bool construct(int type, int turn_count, int source_move, int64_t source_id, int side, pr_arena_tag *t)
{
    memset(t, 0, sizeof *t);
    t->type = (uint8_t)type;
    t->source_id = source_id;
    t->side = (uint8_t)side;
    t->layers = 1;
    int move = source_move;
    switch (type) {
    case ARENA_TAG_TYPE_MIST: move = MOVE_ID_MIST; t->source_id = -1; break;
    case ARENA_TAG_TYPE_QUICK_GUARD: turn_count = 1; move = MOVE_ID_QUICK_GUARD; break;
    case ARENA_TAG_TYPE_WIDE_GUARD: turn_count = 1; move = MOVE_ID_WIDE_GUARD; break;
    case ARENA_TAG_TYPE_MAT_BLOCK: turn_count = 1; move = MOVE_ID_MAT_BLOCK; break;
    case ARENA_TAG_TYPE_CRAFTY_SHIELD: turn_count = 1; move = MOVE_ID_CRAFTY_SHIELD; break;
    case ARENA_TAG_TYPE_NO_CRIT: break;
    case ARENA_TAG_TYPE_MUD_SPORT: move = MOVE_ID_MUD_SPORT; t->side = ARENA_TAG_SIDE_BOTH; break;
    case ARENA_TAG_TYPE_WATER_SPORT: move = MOVE_ID_WATER_SPORT; t->side = ARENA_TAG_SIDE_BOTH; break;
    case ARENA_TAG_TYPE_ION_DELUGE: turn_count = 1; t->source_id = -1; t->side = ARENA_TAG_SIDE_BOTH; break;
    case ARENA_TAG_TYPE_SPIKES: turn_count = 0; move = MOVE_ID_SPIKES; break;
    case ARENA_TAG_TYPE_TOXIC_SPIKES: turn_count = 0; move = MOVE_ID_TOXIC_SPIKES; break;
    case ARENA_TAG_TYPE_STEALTH_ROCK: turn_count = 0; move = MOVE_ID_STEALTH_ROCK; break;
    case ARENA_TAG_TYPE_STICKY_WEB: turn_count = 0; move = MOVE_ID_STICKY_WEB; break;
    case ARENA_TAG_TYPE_IMPRISON: turn_count = 0; move = MOVE_ID_IMPRISON; break;
    case ARENA_TAG_TYPE_TRICK_ROOM: move = MOVE_ID_TRICK_ROOM; t->side = ARENA_TAG_SIDE_BOTH; break;
    case ARENA_TAG_TYPE_GRAVITY: move = MOVE_ID_GRAVITY; t->side = ARENA_TAG_SIDE_BOTH; break;
    case ARENA_TAG_TYPE_REFLECT: move = MOVE_ID_REFLECT; break;
    case ARENA_TAG_TYPE_LIGHT_SCREEN: move = MOVE_ID_LIGHT_SCREEN; break;
    case ARENA_TAG_TYPE_AURORA_VEIL: move = MOVE_ID_AURORA_VEIL; break;
    case ARENA_TAG_TYPE_TAILWIND: move = MOVE_ID_TAILWIND; break;
    case ARENA_TAG_TYPE_HAPPY_HOUR: move = MOVE_ID_HAPPY_HOUR; break;
    case ARENA_TAG_TYPE_SAFEGUARD: move = MOVE_ID_SAFEGUARD; break;
    case ARENA_TAG_TYPE_FIRE_GRASS_PLEDGE: turn_count = 4; move = MOVE_ID_FIRE_PLEDGE; break;
    case ARENA_TAG_TYPE_WATER_FIRE_PLEDGE: turn_count = 4; move = MOVE_ID_WATER_PLEDGE; break;
    case ARENA_TAG_TYPE_GRASS_WATER_PLEDGE: turn_count = 4; move = MOVE_ID_GRASS_PLEDGE; break;
    case ARENA_TAG_TYPE_FAIRY_LOCK: move = MOVE_ID_FAIRY_LOCK; t->side = ARENA_TAG_SIDE_BOTH; break;
    case ARENA_TAG_TYPE_NEUTRALIZING_GAS: turn_count = 0; move = 0; t->side = ARENA_TAG_SIDE_BOTH; break;
    case ARENA_TAG_TYPE_PENDING_HEAL: turn_count = 0; move = 0; t->source_id = -1; t->side = ARENA_TAG_SIDE_BOTH; break;
    default: return false;
    }
    t->turn_count = t->max_duration = (int16_t)turn_count;
    t->source_move = (uint16_t)(move > 0 ? move : 0);   /* `if (sourceMove) this.sourceMove = sourceMove` */
    return true;
}

/* onAddMessageKey / onRemoveMessageKey: the ones that are "" */
static bool has_add_message(int type)
{
    return type != ARENA_TAG_TYPE_NEUTRALIZING_GAS && type != ARENA_TAG_TYPE_PENDING_HEAL && type != ARENA_TAG_TYPE_NONE;
}
static bool has_remove_message(int type)
{
    switch (type) {
    case ARENA_TAG_TYPE_QUICK_GUARD: case ARENA_TAG_TYPE_WIDE_GUARD: case ARENA_TAG_TYPE_MAT_BLOCK:
    case ARENA_TAG_TYPE_CRAFTY_SHIELD: case ARENA_TAG_TYPE_ION_DELUGE: case ARENA_TAG_TYPE_FAIRY_LOCK:
    case ARENA_TAG_TYPE_IMPRISON: case ARENA_TAG_TYPE_PENDING_HEAL: case ARENA_TAG_TYPE_NONE:
        return false;
    }
    return true;
}

/* GravityTag.onAdd after the message: for each active Pokemon in speed order, the ones in the air
 * are grounded (Magnet Rise, Telekinesis and Fly end; Fly's user is interrupted) */
static void gravity_on_add(pr_battle *bt, pr_engine *e)
{
    pr_speed_iter it;
    pr_speed_iter_init(bt, &it, ARENA_TAG_SIDE_BOTH);
    for (pr_pokemon *p; (p = pr_speed_iter_next(bt, &it));) {
        bool was_airborne = !pr_is_grounded(bt, p, true);
        pr_remove_tag(bt, e, p, BATTLER_TAG_TYPE_FLOATING);
        pr_remove_tag(bt, e, p, BATTLER_TAG_TYPE_TELEKINESIS);
        if (pr_get_battler_tag(p, BATTLER_TAG_TYPE_FLYING)) {
            pr_remove_tag(bt, e, p, BATTLER_TAG_TYPE_FLYING);
            pr_add_tag(bt, e, p, BATTLER_TAG_TYPE_INTERRUPTED, 0, 0, -1);
        }
        if (was_airborne) pr_queue_message(bt, e);       /* gravityGroundsPokemon */
    }
}

/* TailwindTag.onAdd after the message: the source's side, in speed order: Wind Power charges a
 * Pokemon that isn't charged yet, Wind Rider raises Attack */
static void tailwind_on_add(pr_battle *bt, pr_engine *e, const pr_arena_tag *t)
{
    pr_pokemon *source = pr_field_pokemon_by_id(bt, t->source_id);
    if (!source) {
        if (t->source_id >= 0) pr_note_unported(bt, PR_FEAT_PARTY);   /* getPokemonById: off the field */
        return;
    }
    pr_speed_iter it;
    pr_speed_iter_init(bt, &it, source->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY);
    for (pr_pokemon *p; (p = pr_speed_iter_next(bt, &it));) {
        if (pr_has_ability_active(bt, p, ABILITY_ID_WIND_POWER) && !pr_get_battler_tag(p, BATTLER_TAG_TYPE_CHARGED)) {
            pr_add_tag(bt, e, p, BATTLER_TAG_TYPE_CHARGED, 0, 0, -1);
            pr_queue_message(bt, e);                      /* windPowerCharged */
        }
        if (pr_has_ability_active(bt, p, ABILITY_ID_WIND_RIDER)) {
            pr_queue_ability_display(bt, e, p, false, true);
            static const int32_t atk = STAT_ATK;
            if (pr_unshift_stat_stage_change(bt, e, pr_battler_index(bt, p), &atk, 1, 1, p) < 0) e->overflow = true;
            pr_queue_ability_display(bt, e, p, false, false);
        }
    }
}

/* ArenaTag.onAdd(quiet) and the subclasses' own */
static void on_add(pr_battle *bt, pr_engine *e, const pr_arena_tag *t, bool quiet)
{
    switch (t->type) {
    case ARENA_TAG_TYPE_NEUTRALIZING_GAS:                /* its message and the abilities it ends */
    case ARENA_TAG_TYPE_IMPRISON:                        /* the IMPRISON battler tags */
        note(bt);
        return;
    }
    if (!quiet && has_add_message(t->type)) pr_queue_message(bt, e);
    if (t->type == ARENA_TAG_TYPE_GRAVITY) gravity_on_add(bt, e);
    else if (t->type == ARENA_TAG_TYPE_TAILWIND) tailwind_on_add(bt, e, t);
}

/* ArenaTag.onRemove(quiet) */
static void on_remove(pr_battle *bt, pr_engine *e, const pr_arena_tag *t, bool quiet)
{
    if (t->type == ARENA_TAG_TYPE_NEUTRALIZING_GAS || t->type == ARENA_TAG_TYPE_IMPRISON) {
        note(bt);                                        /* abilities regained, IMPRISON tags removed */
        return;
    }
    if (!quiet && has_remove_message(t->type)) pr_queue_message(bt, e);
}

static void splice(pr_battle *bt, int i)
{
    memmove(&bt->arena_tags[i], &bt->arena_tags[i + 1], sizeof(pr_arena_tag) * (size_t)(bt->n_arena_tags - 1 - i));
    bt->n_arena_tags--;
}

static int find_on_side(const pr_battle *bt, int type, int side)
{
    for (int i = 0; i < bt->n_arena_tags; i++)
        if (bt->arena_tags[i].type == type && pr_arena_tag_on_side(&bt->arena_tags[i], side)) return i;
    return -1;
}

bool pr_arena_remove_tag_on_side(pr_battle *bt, pr_engine *e, int type, int side, bool quiet)
{
    int i = find_on_side(bt, type, side);
    if (i < 0) return false;
    pr_arena_tag t = bt->arena_tags[i];
    on_remove(bt, e, &t, quiet);
    splice(bt, i);
    return true;
}

/* onOverlap(source): entry hazards add a layer up to their maximum and run onAdd again; Trick
 * Room ends; Neutralizing Gas counts another source (not ported); the others do nothing */
static void on_overlap(pr_battle *bt, pr_engine *e, int i)
{
    pr_arena_tag *t = &bt->arena_tags[i];
    if (pr_arena_tag_is(t->type, PR_ARENACLASS_ENTRY_HAZARD)) {
        if (t->layers >= pr_arena_max_layers(t->type)) return;
        t->layers++;
        pr_arena_tag copy = *t;
        on_add(bt, e, &copy, false);
    } else if (t->type == ARENA_TAG_TYPE_TRICK_ROOM) {
        pr_arena_remove_tag_on_side(bt, e, t->type, t->side, false);
    } else if (t->type == ARENA_TAG_TYPE_NEUTRALIZING_GAS) {
        note(bt);
    }
}

bool pr_arena_add_tag(pr_battle *bt, pr_engine *e, int type, int turn_count, int source_move, int64_t source_id,
                      int side, bool quiet)
{
    int i = find_on_side(bt, type, side);
    if (i >= 0) {
        on_overlap(bt, e, i);
        return false;
    }
    pr_arena_tag t;
    if (!construct(type, turn_count, source_move, source_id, side, &t)) return false;
    on_add(bt, e, &t, quiet);                            /* before the push: Gravity's onAdd can't see itself */
    if (bt->n_arena_tags >= PR_MAX_ARENA_TAGS) {
        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
        e->overflow = true;
        return false;
    }
    bt->arena_tags[bt->n_arena_tags++] = t;
    return true;
}

void pr_arena_remove_tags_on_side(pr_battle *bt, pr_engine *e, const int32_t *types, int n_types, int side, bool quiet)
{
    /* every match in list order gets onRemove, then the list keeps the rest */
    pr_arena_tag keep[PR_MAX_ARENA_TAGS];
    int k = 0;
    for (int i = 0; i < bt->n_arena_tags; i++) {
        const pr_arena_tag *t = &bt->arena_tags[i];
        bool match = false;
        for (int j = 0; j < n_types; j++)
            if (types[j] == t->type) match = true;
        if (!match || !pr_arena_tag_on_side(t, side)) {
            keep[k++] = *t;
            continue;
        }
        on_remove(bt, e, t, quiet);
    }
    memcpy(bt->arena_tags, keep, sizeof(pr_arena_tag) * (size_t)k);
    bt->n_arena_tags = (uint8_t)k;
}

/* FireGrassPledgeTag.lapse before ArenaTag.lapse: the sea of fire hurts every active Pokemon on its
 * side, in speed order, that isn't Fire type or switching out */
static void sea_of_fire(pr_battle *bt, pr_engine *e, int side)
{
    pr_speed_iter it;
    pr_speed_iter_init(bt, &it, side);
    for (pr_pokemon *p; (p = pr_speed_iter_next(bt, &it));) {
        if (pr_is_of_type(p, POKEMON_TYPE_FIRE) || p->switch_out_status) continue;
        pr_queue_message(bt, e);                          /* fireGrassPledgeLapse */
        pr_unshift_new(bt, e, PH_COMMON_ANIM, pr_battler_index(bt, p));   /* CommonAnim.MAGMA_STORM */
        double damage = floor(pr_get_stat(p, STAT_HP, false) / 8.0);
        pr_damage_and_update(bt, e, p, damage < 1 ? 1 : damage, HIT_RESULT_INDIRECT, false, NULL);
    }
}

void pr_arena_lapse_tags(pr_battle *bt, pr_engine *e)
{
    /* arena.lapseTags(): every tag's lapse() first; then, in list order, each ended tag's onRemove
     * and its splice */
    bool ended[PR_MAX_ARENA_TAGS];
    for (int i = 0; i < bt->n_arena_tags; i++) {
        pr_arena_tag *t = &bt->arena_tags[i];
        if (t->type == ARENA_TAG_TYPE_IMPRISON || t->type == ARENA_TAG_TYPE_PENDING_HEAL) note(bt);
        if (t->type == ARENA_TAG_TYPE_FIRE_GRASS_PLEDGE) sea_of_fire(bt, e, t->side);
        ended[i] = !(t->turn_count < 1 || --t->turn_count > 0);
    }
    for (int i = 0; i < bt->n_arena_tags;) {
        if (!ended[i]) { i++; continue; }
        pr_arena_tag t = bt->arena_tags[i];
        on_remove(bt, e, &t, false);
        splice(bt, i);
        memmove(&ended[i], &ended[i + 1], sizeof(bool) * (size_t)(bt->n_arena_tags - i));
    }
}

/* ConditionalProtectTag's condition functions */
static bool conditional_protects(pr_battle *bt, pr_engine *e, int type, int move)
{
    const pr_move *mv = &pr_moves[move];
    switch (type) {
    case ARENA_TAG_TYPE_QUICK_GUARD: {                   /* priority above 0, the attacker's changes included */
        pr_pokemon *attacker = pr_move_effect_user(bt, e, NULL);
        return attacker ? pr_move_get_priority(bt, move, attacker) > 0 : mv->priority > 0;
    }
    case ARENA_TAG_TYPE_WIDE_GUARD:                      /* isSpreadMove */
        return mv->target == MOVE_TARGET_ALL_ENEMIES || mv->target == MOVE_TARGET_ALL_NEAR_ENEMIES
               || mv->target == MOVE_TARGET_ALL_OTHERS || mv->target == MOVE_TARGET_ALL_NEAR_OTHERS;
    case ARENA_TAG_TYPE_MAT_BLOCK: return mv->category != MOVE_CATEGORY_STATUS;
    case ARENA_TAG_TYPE_CRAFTY_SHIELD:
        return mv->category == MOVE_CATEGORY_STATUS && mv->target != MOVE_TARGET_ENEMY_SIDE
               && mv->target != MOVE_TARGET_BOTH_SIDES && mv->target != MOVE_TARGET_ALL;
    }
    return false;
}

/* canAffect(pokemon): getAffectedPokemon().includes(pokemon); one side's field (fainted included),
 * or every active Pokemon for BOTH */
static bool can_affect(const pr_battle *bt, const pr_arena_tag *t, const pr_pokemon *p)
{
    int b = pr_battler_index(bt, p);
    if (b < 0) return false;
    switch (t->side) {
    case ARENA_TAG_SIDE_PLAYER: return b < 2;
    case ARENA_TAG_SIDE_ENEMY: return b >= 2;
    default: return p->hp > 0;
    }
}

void pr_arena_conditional_protect(pr_battle *bt, pr_engine *e, int side, const pr_pokemon *defender, int move,
                                  bool *is_protected, bool *bypass)
{
    for (int i = 0; i < bt->n_arena_tags; i++) {
        const pr_arena_tag *t = &bt->arena_tags[i];
        if (!pr_arena_tag_is(t->type, PR_ARENACLASS_CONDITIONAL_PROTECT) || !pr_arena_tag_on_side(t, side)) continue;
        if (!can_affect(bt, t, defender) || !conditional_protects(bt, e, t->type, move) || *is_protected) continue;
        *is_protected = true;
        pr_queue_message(bt, e);                          /* conditionalProtectApply / matBlockApply */
        *bypass = *bypass || t->type == ARENA_TAG_TYPE_CRAFTY_SHIELD;   /* ignoresBypass */
    }
}

/* ---------------------------------------------------------------- positional tags */

bool pr_pos_can_add(const pr_battle *bt, int type, int target_index)
{
    for (int i = 0; i < bt->n_pos_tags; i++)
        if (bt->pos_tags[i].type == type && bt->pos_tags[i].target_index == target_index) return false;
    return true;
}

void pr_pos_add(pr_battle *bt, pr_engine *e, const pr_positional_tag *t)
{
    if (bt->n_pos_tags >= PR_MAX_POSITIONAL_TAGS) {
        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
        if (e) e->overflow = true;
        return;
    }
    bt->pos_tags[bt->n_pos_tags++] = *t;
}
