/* Attribute dispatch. Upstream calls applyAbAttrs("<Class>")/applyMoveAttrs("<Class>")
 * all over the battle code; every such call site in the C port asks here first.
 * Classes with a C handler are applied by the caller; anything else is logged as
 * unported so a differential test never silently passes on missing logic. */
#include "pr/battle.h"

void pr_note_unported(pr_battle *bt, uint16_t cls)
{
    for (int i = 0; i < bt->n_unported; i++)
        if (bt->unported[i] == cls) return;
    if (bt->n_unported < PR_MAX_UNPORTED) bt->unported[bt->n_unported++] = cls;
}

static bool ability_has(uint16_t ability, uint16_t cls)
{
    if (ability >= PR_ABILITY_COUNT) return false;
    const pr_ability *a = &pr_abilities[ability];
    for (unsigned i = 0; i < a->nattr; i++)
        if (pr_attr_is(pr_attrs[a->attr0 + i].cls, cls)) return true;
    return false;
}

bool pr_hook_ab(pr_battle *bt, const pr_pokemon *p, uint16_t cls)
{
    if (!p) return false;
    bool hit = ability_has(p->ability, cls) || (p->passive && ability_has(p->passive, cls));
    if (hit) pr_note_unported(bt, cls);
    return hit;
}

bool pr_move_has_attr(uint16_t move, uint16_t cls)
{
    if (move >= PR_MOVE_COUNT) return false;
    const pr_move *m = &pr_moves[move];
    for (unsigned i = 0; i < m->nattr; i++)
        if (pr_attr_is(pr_attrs[m->attr0 + i].cls, cls)) return true;
    return false;
}

bool pr_hook_move(pr_battle *bt, uint16_t move, uint16_t cls)
{
    bool hit = pr_move_has_attr(move, cls);
    if (hit) pr_note_unported(bt, cls);
    return hit;
}

int32_t pr_battle_rand_seed_int(pr_battle *bt, int32_t range, int32_t min)
{
    /* battle.ts randSeedInt: runs on its own saved RNG state, seeded lazily from
     * shiftCharCodes(battleSeed, turn << 6). The global RND is left untouched. */
    if (range <= 1) return min;
    if (!bt->battle_rng_live) {
        pr_seedstr s;
        pr_seed_shift(&s, &bt->battle_seed, bt->turn << 6);
        pr_rng_sow(&bt->battle_rng, &s);
        bt->battle_rng_live = true;
    }
    return pr_rng_seed_int(&bt->battle_rng, range, min);
}

/* ---- state the port does not model yet ----
 * Each query below answers for state that doesn't exist yet in the C port (battler and
 * arena tags, wave data). The caller passes counts of what upstream holds;
 * when there is nothing, "absent" is the exact answer. When there is something, the
 * query can't be answered, and the unported note keeps the result from being trusted.
 * These are the only places to change when that state is ported. */
bool pr_has_battler_tag(pr_battle *bt, const pr_pokemon *p, int tag_type)
{
    (void)tag_type;
    if (p->n_tags) pr_note_unported(bt, PR_FEAT_BATTLER_TAGS);
    return false;
}

bool pr_has_battler_tag_class(pr_battle *bt, const pr_pokemon *p, int tag_class)
{
    (void)tag_class;
    if (p->n_tags) pr_note_unported(bt, PR_FEAT_BATTLER_TAGS);
    return false;
}

bool pr_arena_has_tag(pr_battle *bt, int tag_type, int side)
{
    (void)tag_type; (void)side;
    if (bt->n_arena_tags) pr_note_unported(bt, PR_FEAT_ARENA_TAGS);
    return false;
}

bool pr_wave_ability_applied(const pr_pokemon *p, int ability)
{
    (void)p; (void)ability;
    return false;
}

int pr_battler_index(const pr_battle *bt, const pr_pokemon *p)
{
    for (int i = 0; i < 4; i++)
        if (bt->field[i] == p) return i;
    return -1;
}

const char *pr_unported_name(uint16_t id)
{
    static const char *const feat[PR_FEAT_COUNT_ - PR_AC_COUNT_] = {
        "(doubles)", "(modifier subclass not ported)", "(battler tags)", "(arena tags)", "(move history)",
        "(turn order)", "(faint history)", "(wave data)", "(party)", "(earlier hit of the same move)"};
    if (id < PR_AC_COUNT_) return pr_attr_class_name[id];
    if (id < PR_FEAT_COUNT_) return feat[id - PR_AC_COUNT_];
    return "?";
}
