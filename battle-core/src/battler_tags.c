/* Battler tags being added, lapsed and removed: Pokemon.addTag, lapseTag, lapseTags, removeTag
 * (src/field/pokemon.ts) and, per tag type, what getBattlerTag() builds (src/data/battler-tags.ts):
 * its constructor's fields, canAdd, onAdd, onOverlap, lapse and onRemove.
 *
 * Only some tag types have those ported (pr_tag_type_ported). The others still take part where
 * upstream would reach them: a lapse or removal that would run one of their methods reports
 * the tag type (PR_FEAT_TAG_0 + type) and leaves the tag as it is. Which lapses reach a tag
 * comes from the lapse types upstream builds each tag with (pr_battler_tags_info, dumped). */
#include "pr/engine.h"
#include <string.h>

/* The tag types whose constructor, canAdd, onAdd, onOverlap, lapse and onRemove are ported. */
bool pr_tag_type_ported(int type)
{
    switch (type) {
    case BATTLER_TAG_TYPE_CHARGING:     /* SerializableBattlerTag(CHARGING, CUSTOM, 1, sourceMove, sourceId) */
    case BATTLER_TAG_TYPE_FLYING:       /* SemiInvulnerableTag: onAdd / onRemove only hide and show the sprite */
    case BATTLER_TAG_TYPE_UNDERGROUND:
    case BATTLER_TAG_TYPE_UNDERWATER:
    case BATTLER_TAG_TYPE_HIDDEN:
        return true;
    }
    return false;
}

static bool lapses_on(int type, int lapse_type)
{
    return (pr_battler_tags_info[type].lapse_mask >> lapse_type) & 1u;
}

static void note_tag(pr_battle *bt, int type)
{
    pr_note_unported(bt, (uint16_t)(PR_FEAT_TAG_0 + type));
}

/* tag.lapse(pokemon, lapseType) for a ported type: BattlerTag's `--this.turnCount > 0` */
static bool tag_lapse(pr_battler_tag *t, int lapse_type)
{
    (void)lapse_type;
    return --t->turn_count > 0;
}

/* tag.onRemove(pokemon) for a ported type: nothing the battle state sees */
static void tag_on_remove(pr_battle *bt, pr_engine *e, pr_pokemon *p, const pr_battler_tag *t)
{
    (void)bt; (void)e; (void)p; (void)t;
}

static void splice_tag(pr_pokemon *p, int i)
{
    memmove(&p->tags[i], &p->tags[i + 1], (size_t)(p->n_tags - i - 1) * sizeof p->tags[0]);
    p->n_tags--;
}

static int find_tag(const pr_pokemon *p, int type)
{
    for (int i = 0; i < p->n_tags; i++)
        if (p->tags[i].type == type) return i;
    return -1;
}

bool pr_add_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type, int turn_count, int source_move,
                int64_t source_id)
{
    if (!pr_tag_type_ported(type)) {
        note_tag(bt, type);
        return false;
    }
    /* an existing tag of the type: existingTag.onOverlap(this) (BattlerTag's does nothing) */
    if (find_tag(p, type) >= 0) return false;

    /* getBattlerTag(tagType, turnCount, sourceMove, sourceId) */
    pr_battler_tag t;
    memset(&t, 0, sizeof t);
    t.type = (uint8_t)type;
    t.source_move = (uint16_t)source_move;        /* `if (sourceMove)`: 0 stays none */
    t.source_id = -1;
    if (type == BATTLER_TAG_TYPE_CHARGING) {
        t.turn_count = 1;
        t.source_id = source_id;
    } else {                                      /* SemiInvulnerableTag(tagType, turnCount, sourceMove) */
        t.turn_count = (int16_t)turn_count;
    }

    /* BattlerTagImmunityAbAttr on the Pokemon, then UserFieldBattlerTagImmunityAbAttr on its
     * side in speed order (the Pokemon itself included) */
    bool cancelled = false;
    pr_ab_params ip = {.pokemon = p, .move = -1, .flag = &cancelled, .tag_type = type};
    pr_apply_ab_attrs(bt, PR_AC_BattlerTagImmunityAbAttr, &ip);
    if (cancelled) return false;
    pr_speed_iter it;
    pr_pokemon *ally;
    pr_speed_iter_init(bt, &it, p->is_player ? ARENA_TAG_SIDE_PLAYER : ARENA_TAG_SIDE_ENEMY);
    while ((ally = pr_speed_iter_next(bt, &it))) {
        pr_ab_params ap = {.pokemon = ally, .target = p, .move = -1, .flag = &cancelled, .tag_type = type};
        pr_apply_ab_attrs(bt, PR_AC_UserFieldBattlerTagImmunityAbAttr, &ap);
        if (cancelled) return false;
    }

    /* newTag.canAdd(this) is true for the ported types; push, then onAdd (nothing to model) */
    if (p->n_tags >= PR_MAX_BATTLER_TAGS) {
        pr_note_unported(bt, PR_FEAT_ENGINE_LIMIT);
        if (e) e->overflow = true;
        return false;
    }
    p->tags[p->n_tags++] = t;
    return true;
}

void pr_lapse_tags(pr_battle *bt, pr_engine *e, pr_pokemon *p, int lapse_type)
{
    /* tags.filter(t => lapseType === FAINT || (t.lapseTypes has lapseType && !t.lapse(this, lapseType)))
     *     .forEach(t => { t.onRemove(this); tags.splice(tags.indexOf(t), 1); })
     * Every lapse runs before the first removal. */
    bool gone[PR_MAX_BATTLER_TAGS] = {0};
    int n = p->n_tags;
    for (int i = 0; i < n; i++) {
        int type = p->tags[i].type;
        if (lapse_type != BATTLER_TAG_LAPSE_TYPE_FAINT && !lapses_on(type, lapse_type)) continue;
        if (!pr_tag_type_ported(type)) {
            note_tag(bt, type);
            continue;
        }
        gone[i] = lapse_type == BATTLER_TAG_LAPSE_TYPE_FAINT || !tag_lapse(&p->tags[i], lapse_type);
    }
    /* in list order: onRemove, then the splice (removed counts the earlier splices) */
    for (int i = 0, removed = 0; i < n; i++) {
        if (!gone[i]) continue;
        pr_battler_tag t = p->tags[i - removed];
        tag_on_remove(bt, e, p, &t);
        splice_tag(p, i - removed);
        removed++;
    }
}

bool pr_lapse_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type)
{
    int i = find_tag(p, type);
    if (i < 0) return false;
    if (!pr_tag_type_ported(type)) {
        note_tag(bt, type);
        return true;
    }
    if (!tag_lapse(&p->tags[i], BATTLER_TAG_LAPSE_TYPE_CUSTOM)) {
        tag_on_remove(bt, e, p, &p->tags[i]);
        splice_tag(p, i);
    }
    return true;
}

void pr_remove_tag(pr_battle *bt, pr_engine *e, pr_pokemon *p, int type)
{
    int i = find_tag(p, type);
    if (i < 0) return;
    if (!pr_tag_type_ported(type)) {
        note_tag(bt, type);
        return;
    }
    tag_on_remove(bt, e, p, &p->tags[i]);
    splice_tag(p, i);
}
