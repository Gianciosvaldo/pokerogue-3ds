/* Battler and arena tags: the queries upstream's battle logic makes (Pokemon.getTag,
 * findTag, arena.getTagOnSide, applyTagsForSide) and the per-type facts that
 * getBattlerTag() / getArenaTag() (src/data/battler-tags.ts, arena-tag.ts) build into
 * each tag. The tag lists themselves live in pr_pokemon.tags and pr_battle.arena_tags. */
#include "pr/battle.h"

const char *const pr_tagclass_name[PR_TAGCLASS_COUNT_] = {
    [PR_TAGCLASS_PROTECTED] = "ProtectedTag",
    [PR_TAGCLASS_SEMI_INVULNERABLE] = "SemiInvulnerableTag",
    [PR_TAGCLASS_TYPE_BOOST] = "TypeBoostTag",
    [PR_TAGCLASS_HELPING_HAND] = "HelpingHandTag",
    [PR_TAGCLASS_SUPREME_OVERLORD] = "SupremeOverlordTag",
    [PR_TAGCLASS_CRIT_BOOST] = "CritBoostTag",
    [PR_TAGCLASS_TAR_SHOT] = "TarShotTag",
    [PR_TAGCLASS_HIGHEST_STAT_BOOST] = "HighestStatBoostTag",
    [PR_TAGCLASS_EXPOSED] = "ExposedTag",
    [PR_TAGCLASS_STOCKPILING] = "StockpilingTag",
    [PR_TAGCLASS_AUTOTOMIZED] = "AutotomizedTag",
    [PR_TAGCLASS_SUBSTITUTE] = "SubstituteTag",
    [PR_TAGCLASS_TRAPPED] = "TrappedTag",
};

/* getBattlerTag(): which tag types are built as (a subclass of) each class */
bool pr_battler_tag_is(int t, int cls)
{
    switch (cls) {
    case PR_TAGCLASS_PROTECTED: /* ProtectedTag and its Contact*ProtectedTag subclasses */
        return t == BATTLER_TAG_TYPE_PROTECTED || t == BATTLER_TAG_TYPE_SPIKY_SHIELD || t == BATTLER_TAG_TYPE_KINGS_SHIELD
               || t == BATTLER_TAG_TYPE_OBSTRUCT || t == BATTLER_TAG_TYPE_SILK_TRAP
               || t == BATTLER_TAG_TYPE_BANEFUL_BUNKER || t == BATTLER_TAG_TYPE_BURNING_BULWARK;
    case PR_TAGCLASS_SEMI_INVULNERABLE:
        return t == BATTLER_TAG_TYPE_FLYING || t == BATTLER_TAG_TYPE_UNDERGROUND || t == BATTLER_TAG_TYPE_UNDERWATER
               || t == BATTLER_TAG_TYPE_HIDDEN;
    case PR_TAGCLASS_TYPE_BOOST: return t == BATTLER_TAG_TYPE_FIRE_BOOST || t == BATTLER_TAG_TYPE_CHARGED;
    case PR_TAGCLASS_HELPING_HAND: return t == BATTLER_TAG_TYPE_HELPING_HAND;
    case PR_TAGCLASS_SUPREME_OVERLORD: return t == BATTLER_TAG_TYPE_SUPREME_OVERLORD;
    case PR_TAGCLASS_CRIT_BOOST: return t == BATTLER_TAG_TYPE_CRIT_BOOST || t == BATTLER_TAG_TYPE_DRAGON_CHEER;
    case PR_TAGCLASS_TAR_SHOT: return t == BATTLER_TAG_TYPE_TAR_SHOT;
    case PR_TAGCLASS_HIGHEST_STAT_BOOST: /* Weather- and TerrainHighestStatBoostTag */
        return t == BATTLER_TAG_TYPE_PROTOSYNTHESIS || t == BATTLER_TAG_TYPE_QUARK_DRIVE;
    case PR_TAGCLASS_EXPOSED: return t == BATTLER_TAG_TYPE_IGNORE_GHOST || t == BATTLER_TAG_TYPE_IGNORE_DARK;
    case PR_TAGCLASS_STOCKPILING: return t == BATTLER_TAG_TYPE_STOCKPILING;
    case PR_TAGCLASS_AUTOTOMIZED: return t == BATTLER_TAG_TYPE_AUTOTOMIZED;
    case PR_TAGCLASS_SUBSTITUTE: return t == BATTLER_TAG_TYPE_SUBSTITUTE;
    case PR_TAGCLASS_TRAPPED: /* TrappedTag and its subclasses: NoRetreat, Ingrain, Octolock, DamagingTrapTag */
        switch (t) {
        case BATTLER_TAG_TYPE_TRAPPED: case BATTLER_TAG_TYPE_NO_RETREAT: case BATTLER_TAG_TYPE_INGRAIN:
        case BATTLER_TAG_TYPE_OCTOLOCK: case BATTLER_TAG_TYPE_BIND: case BATTLER_TAG_TYPE_WRAP:
        case BATTLER_TAG_TYPE_FIRE_SPIN: case BATTLER_TAG_TYPE_WHIRLPOOL: case BATTLER_TAG_TYPE_CLAMP:
        case BATTLER_TAG_TYPE_SAND_TOMB: case BATTLER_TAG_TYPE_MAGMA_STORM: case BATTLER_TAG_TYPE_SNAP_TRAP:
        case BATTLER_TAG_TYPE_THUNDER_CAGE: case BATTLER_TAG_TYPE_INFESTATION:
            return true;
        }
        return false;
    }
    return false;
}

const char *const pr_arenaclass_name[PR_ARENACLASS_COUNT_] = {
    [PR_ARENACLASS_WEAKEN_MOVE_SCREEN] = "WeakenMoveScreenTag",
    [PR_ARENACLASS_WEAKEN_MOVE_TYPE] = "WeakenMoveTypeTag",
    [PR_ARENACLASS_NO_CRIT] = "NoCritTag",
    [PR_ARENACLASS_SUPPRESS_ABILITIES] = "SuppressAbilitiesTag",
    [PR_ARENACLASS_CONDITIONAL_PROTECT] = "ConditionalProtectTag",
    [PR_ARENACLASS_ENTRY_HAZARD] = "EntryHazardTag",
};

bool pr_arena_tag_is(int t, int cls)
{
    switch (cls) {
    case PR_ARENACLASS_WEAKEN_MOVE_SCREEN:
        return t == ARENA_TAG_TYPE_REFLECT || t == ARENA_TAG_TYPE_LIGHT_SCREEN || t == ARENA_TAG_TYPE_AURORA_VEIL;
    case PR_ARENACLASS_WEAKEN_MOVE_TYPE: return t == ARENA_TAG_TYPE_MUD_SPORT || t == ARENA_TAG_TYPE_WATER_SPORT;
    case PR_ARENACLASS_NO_CRIT: return t == ARENA_TAG_TYPE_NO_CRIT;
    case PR_ARENACLASS_SUPPRESS_ABILITIES: return t == ARENA_TAG_TYPE_NEUTRALIZING_GAS;
    case PR_ARENACLASS_CONDITIONAL_PROTECT:
        return t == ARENA_TAG_TYPE_QUICK_GUARD || t == ARENA_TAG_TYPE_WIDE_GUARD || t == ARENA_TAG_TYPE_MAT_BLOCK
               || t == ARENA_TAG_TYPE_CRAFTY_SHIELD;
    case PR_ARENACLASS_ENTRY_HAZARD:                     /* Imprison is built as one too */
        return t == ARENA_TAG_TYPE_SPIKES || t == ARENA_TAG_TYPE_TOXIC_SPIKES || t == ARENA_TAG_TYPE_STEALTH_ROCK
               || t == ARENA_TAG_TYPE_STICKY_WEB || t == ARENA_TAG_TYPE_IMPRISON;
    }
    return false;
}

/* getTag(type): summonData.tags.find(t => t.tagType === type) */
const pr_battler_tag *pr_get_battler_tag(const pr_pokemon *p, int tag_type)
{
    for (int i = 0; i < p->n_tags; i++)
        if (p->tags[i].type == tag_type) return &p->tags[i];
    return NULL;
}

/* getTag(Class) / findTag(t => t instanceof Class) */
const pr_battler_tag *pr_find_battler_tag_class(const pr_pokemon *p, int tag_class)
{
    for (int i = 0; i < p->n_tags; i++)
        if (pr_battler_tag_is(p->tags[i].type, tag_class)) return &p->tags[i];
    return NULL;
}

bool pr_has_battler_tag(pr_battle *bt, const pr_pokemon *p, int tag_type)
{
    (void)bt;
    return pr_get_battler_tag(p, tag_type) != NULL;
}

bool pr_has_battler_tag_class(pr_battle *bt, const pr_pokemon *p, int tag_class)
{
    (void)bt;
    return pr_find_battler_tag_class(p, tag_class) != NULL;
}

bool pr_arena_tag_on_side(const pr_arena_tag *t, int side)
{
    return side == ARENA_TAG_SIDE_BOTH || t->side == ARENA_TAG_SIDE_BOTH || t->side == side;
}

const pr_arena_tag *pr_arena_get_tag_on_side(const pr_battle *bt, int tag_type, int side)
{
    for (int i = 0; i < bt->n_arena_tags; i++)
        if (bt->arena_tags[i].type == tag_type && pr_arena_tag_on_side(&bt->arena_tags[i], side))
            return &bt->arena_tags[i];
    return NULL;
}

bool pr_arena_has_tag(pr_battle *bt, int tag_type, int side)
{
    return pr_arena_get_tag_on_side(bt, tag_type, side) != NULL;
}

bool pr_arena_has_tag_class(pr_battle *bt, int arena_class, int side)
{
    for (int i = 0; i < bt->n_arena_tags; i++)
        if (pr_arena_tag_is(bt->arena_tags[i].type, arena_class) && pr_arena_tag_on_side(&bt->arena_tags[i], side))
            return true;
    return false;
}

/* WeakenMoveScreenTag.weakenedCategories: Reflect physical, Light Screen special, Aurora Veil both */
bool pr_screen_tag_weakens(int tag_type, int category)
{
    switch (tag_type) {
    case ARENA_TAG_TYPE_REFLECT: return category == MOVE_CATEGORY_PHYSICAL;
    case ARENA_TAG_TYPE_LIGHT_SCREEN: return category == MOVE_CATEGORY_SPECIAL;
    case ARENA_TAG_TYPE_AURORA_VEIL: return category == MOVE_CATEGORY_PHYSICAL || category == MOVE_CATEGORY_SPECIAL;
    }
    return false;
}

/* WeakenMoveTypeTag.weakenedType: Mud Sport Electric, Water Sport Fire; -1 for other tags */
int pr_weaken_move_type_tag_type(int tag_type)
{
    switch (tag_type) {
    case ARENA_TAG_TYPE_MUD_SPORT: return POKEMON_TYPE_ELECTRIC;
    case ARENA_TAG_TYPE_WATER_SPORT: return POKEMON_TYPE_FIRE;
    }
    return -1;
}

/* FIRE_BOOST: new TypeBoostTag(tagType, sourceMove, FIRE, 1.5, false)
 * CHARGED: new TypeBoostTag(tagType, sourceMove, ELECTRIC, 2, true) */
bool pr_type_boost_tag_params(int tag_type, int *boosted_type, double *boost_value)
{
    switch (tag_type) {
    case BATTLER_TAG_TYPE_FIRE_BOOST: *boosted_type = POKEMON_TYPE_FIRE; *boost_value = 1.5; return true;
    case BATTLER_TAG_TYPE_CHARGED: *boosted_type = POKEMON_TYPE_ELECTRIC; *boost_value = 2; return true;
    }
    return false;
}

/* ExposedTag.ignoreImmunity(type, moveType): type === defenderType && allowedTypes.includes(moveType)
 * IGNORE_GHOST: (GHOST, [NORMAL, FIGHTING]); IGNORE_DARK: (DARK, [PSYCHIC]) */
bool pr_exposed_tag_ignores_immunity(int tag_type, int defender_type, int move_type)
{
    switch (tag_type) {
    case BATTLER_TAG_TYPE_IGNORE_GHOST:
        return defender_type == POKEMON_TYPE_GHOST && (move_type == POKEMON_TYPE_NORMAL || move_type == POKEMON_TYPE_FIGHTING);
    case BATTLER_TAG_TYPE_IGNORE_DARK:
        return defender_type == POKEMON_TYPE_DARK && move_type == POKEMON_TYPE_PSYCHIC;
    }
    return false;
}
