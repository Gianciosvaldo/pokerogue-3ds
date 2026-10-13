/* Moves that call other moves (data/moves/move.ts): CallMoveAttr and its subclasses (Nature
 * Power, Mirror Move and Copycat, Metronome, Sleep Talk and Assist), Instruct's RepeatMoveAttr,
 * and getMoveTargets (data/moves/move-utils.ts), which they use to aim the called move.
 * Queueing the called move is in phases.c (it needs the phase engine). */
#include "pr/engine.h"
#include <stdlib.h>
#include <string.h>

/* ---------------------------------------------------------------- banlists */

/* CallMoveAttrWithBanlist.invalidMoves: the dump holds the set as {"$set": [ids...]}. Parsed
 * once per distinct set into a bitset. */
typedef struct {
    const char *src;
    uint8_t bits[(PR_MOVE_COUNT + 7) / 8];
} banlist;
static banlist bans[8];
static int n_bans;

static const uint8_t *ban_bits(const char *json)
{
    for (int i = 0; i < n_bans; i++)
        if (bans[i].src == json) return bans[i].bits;
    if (n_bans == (int)(sizeof bans / sizeof *bans)) abort();          /* 5 sets upstream */
    banlist *b = &bans[n_bans++];
    memset(b, 0, sizeof *b);
    b->src = json;
    const char *s = json ? strchr(json, '[') : NULL;
    while (s && *s && *s != ']') {
        char *end;
        long v = strtol(s + 1, &end, 10);
        if (end == s + 1) break;
        if (v >= 0 && v < PR_MOVE_COUNT) b->bits[v >> 3] |= (uint8_t)(1u << (v & 7));
        s = end;
        while (*s == ' ') s++;
    }
    return b->bits;
}

/* CallMoveAttrWithBanlist.isMoveAllowed: not NONE, not banned, implemented (challenges that
 * bar moves aren't modelled: classic mode has none) */
static bool move_allowed(const char *invalid, int move)
{
    if (move <= MOVE_ID_NONE || move >= PR_MOVE_COUNT) return false;
    if (ban_bits(invalid)[move >> 3] & (1u << (move & 7))) return false;
    return !pr_moves[move].unimplemented;
}

/* ---------------------------------------------------------------- RandomMovesetMoveAttr.selectedMove */

/* Upstream keeps the move Sleep Talk's or Assist's condition picked on the attribute object, so
 * it is shared by every user of the move and outlives the move use (a pick the move never
 * used is handed to the next caller). One per move; tests read and write it with the state. */
static uint16_t selected[PR_MOVE_COUNT];

void pr_called_selected_reset(void)
{
    memset(selected, 0, sizeof selected);
    pr_instruct_keep(NULL, -1);
}
void pr_called_selected_set(int move, int sel) { if (move > 0 && move < PR_MOVE_COUNT) selected[move] = (uint16_t)sel; }
int pr_called_selected_list(uint16_t *move, uint16_t *sel, int max)
{
    int n = 0;
    for (int m = 0; m < PR_MOVE_COUNT && n < max; m++)
        if (selected[m]) { move[n] = (uint16_t)m; sel[n] = selected[m]; n++; }
    return n;
}

/* ---------------------------------------------------------------- getMove */

/* NaturePowerAttr.getMoveId(terrain, biome) */
static int nature_power_move(int terrain, int biome)
{
    switch (terrain) {
    case TERRAIN_ELECTRIC: return MOVE_ID_THUNDERBOLT;
    case TERRAIN_GRASSY: return MOVE_ID_ENERGY_BALL;
    case TERRAIN_PSYCHIC: return MOVE_ID_PSYCHIC;
    case TERRAIN_MISTY: return MOVE_ID_MOONBLAST;
    }
    switch (biome) {
    case BIOME_ID_TOWN: return MOVE_ID_ROUND;
    case BIOME_ID_METROPOLIS: return MOVE_ID_TRI_ATTACK;
    case BIOME_ID_SLUM: return MOVE_ID_SLUDGE_BOMB;
    case BIOME_ID_PLAINS: return MOVE_ID_SILVER_WIND;
    case BIOME_ID_GRASS: return MOVE_ID_GRASS_KNOT;
    case BIOME_ID_TALL_GRASS: return MOVE_ID_POLLEN_PUFF;
    case BIOME_ID_MEADOW: return MOVE_ID_GIGA_DRAIN;
    case BIOME_ID_FOREST: return MOVE_ID_BUG_BUZZ;
    case BIOME_ID_JUNGLE: return MOVE_ID_LEAF_STORM;
    case BIOME_ID_SEA: return MOVE_ID_HYDRO_PUMP;
    case BIOME_ID_SWAMP: return MOVE_ID_MUD_BOMB;
    case BIOME_ID_BEACH: return MOVE_ID_SCALD;
    case BIOME_ID_LAKE: return MOVE_ID_BUBBLE_BEAM;
    case BIOME_ID_SEABED: return MOVE_ID_BRINE;
    case BIOME_ID_ISLAND: return MOVE_ID_LEAF_TORNADO;
    case BIOME_ID_MOUNTAIN: return MOVE_ID_AIR_SLASH;
    case BIOME_ID_BADLANDS: return MOVE_ID_EARTH_POWER;
    case BIOME_ID_DESERT: return MOVE_ID_SCORCHING_SANDS;
    case BIOME_ID_WASTELAND: return MOVE_ID_DRAGON_PULSE;
    case BIOME_ID_CONSTRUCTION_SITE: return MOVE_ID_STEEL_BEAM;
    case BIOME_ID_CAVE: return MOVE_ID_POWER_GEM;
    case BIOME_ID_ICE_CAVE: return MOVE_ID_ICE_BEAM;
    case BIOME_ID_SNOWY_FOREST: return MOVE_ID_FROST_BREATH;
    case BIOME_ID_VOLCANO: return MOVE_ID_LAVA_PLUME;
    case BIOME_ID_GRAVEYARD: return MOVE_ID_SHADOW_BALL;
    case BIOME_ID_RUINS: return MOVE_ID_ANCIENT_POWER;
    case BIOME_ID_TEMPLE: return MOVE_ID_EXTRASENSORY;
    case BIOME_ID_DOJO: return MOVE_ID_FOCUS_BLAST;
    case BIOME_ID_FAIRY_CAVE: return MOVE_ID_ALLURING_VOICE;
    case BIOME_ID_ABYSS: return MOVE_ID_OMINOUS_WIND;
    case BIOME_ID_SPACE: return MOVE_ID_DRACO_METEOR;
    case BIOME_ID_FACTORY: return MOVE_ID_FLASH_CANNON;
    case BIOME_ID_LABORATORY: return MOVE_ID_ZAP_CANNON;
    case BIOME_ID_POWER_PLANT: return MOVE_ID_CHARGE_BEAM;
    case BIOME_ID_END: return MOVE_ID_ETERNABEAM;
    }
    return MOVE_ID_NONE;                                   /* `biome satisfies never` */
}

/* RandomMovesetMoveAttr.getEligibleMoves: the user's own moveset (Sleep Talk), or every other
 * party member's (Assist), duplicates kept, filtered by the banlist */
static int eligible_moves(pr_battle *bt, const char *invalid, bool include_party, const pr_pokemon *user, uint16_t *out)
{
    int n = 0;
    if (!include_party) {
        for (int k = 0; k < PR_N_MOVES(user); k++)
            if (move_allowed(invalid, PR_MOVES(user)[k])) out[n++] = PR_MOVES(user)[k];
        return n;
    }
    int side = user->is_player ? 0 : 1;
    if (!bt->n_party[side]) {
        pr_note_unported(bt, PR_FEAT_PARTY);
        return 0;
    }
    for (int i = 0; i < bt->n_party[side]; i++) {
        const pr_pokemon *m = bt->party[side][i];
        if (m->id == user->id) continue;
        for (int k = 0; k < PR_N_MOVES(m); k++)
            if (move_allowed(invalid, PR_MOVES(m)[k])) out[n++] = PR_MOVES(m)[k];
    }
    return n;
}

int pr_call_move_get(pr_battle *bt, pr_engine *e, int move, const pr_attr *a, const pr_pokemon *user,
                     const pr_pokemon *target)
{
    switch (a->cls) {
    case PR_AC_NaturePowerAttr: {
        if (!bt->has_biome) pr_note_unported(bt, PR_FEAT_WAVE);
        int id = nature_power_move(bt->terrain, bt->biome);
        if (e)
            pr_queue_msg(bt, e, pr_msg2("moveTriggers:naturePowerUse", pr_arg_pokemon(bt, "pokemonName", user),
                                        pr_arg_move("moveName", id)));
        return id;
    }
    case PR_AC_CopyMoveAttr: {
        const pr_attr_CopyMoveAttr *x = a->p;
        int called = bt->last_move;                        /* Copycat: currentBattle.lastMove */
        if (x->preserveTargeting) {                        /* Mirror Move: target.getLastNonVirtualMove(false, false) */
            bool unknown;
            const pr_turn_move *tm = target ? pr_last_non_virtual_move(bt, target, false, false, &unknown) : NULL;
            if (target && unknown) pr_note_unported(bt, PR_FEAT_MOVE_HISTORY);
            called = tm ? tm->move : MOVE_ID_NONE;
        }
        return move_allowed(x->invalidMoves, called) ? called : MOVE_ID_NONE;
    }
    case PR_AC_RandomMoveAttr: {                           /* Metronome: a fresh pick every call */
        const pr_attr_RandomMoveAttr *x = a->p;
        static uint16_t ids[PR_MOVE_COUNT];
        int n = 0;
        for (int m = 0; m < PR_MOVE_COUNT; m++)
            if (move_allowed(x->invalidMoves, m)) ids[n++] = (uint16_t)m;
        int k = pr_battle_rand_seed_int(bt, n, 0);
        return n ? ids[k] : MOVE_ID_NONE;
    }
    case PR_AC_RandomMovesetMoveAttr: {
        const pr_attr_RandomMovesetMoveAttr *x = a->p;
        if (selected[move]) {                              /* the condition's pick, used once */
            int t = selected[move];
            selected[move] = MOVE_ID_NONE;
            return t;
        }
        uint16_t el[PR_MAX_PARTY * 4];
        int n = eligible_moves(bt, x->invalidMoves, x->includeParty, user, el);
        int k = pr_battle_rand_seed_int(bt, n, 0);
        selected[move] = n ? el[k] : MOVE_ID_NONE;
        return selected[move];
    }
    }
    pr_note_unported(bt, a->cls);
    return MOVE_ID_NONE;
}

/* ---------------------------------------------------------------- getMoveTargets */

static bool active(const pr_pokemon *p) { return p && p->hp > 0; }
static bool active_on_field(const pr_pokemon *p) { return active(p) && p->on_field && !p->switch_out_status; }

int pr_get_move_targets(pr_battle *bt, const pr_pokemon *user, int move, int replace_target, int8_t out[4],
                        bool *multiple)
{
    const pr_move *mv = &pr_moves[move];
    int b = pr_battler_index(bt, user);
    int own = b < 2 ? 0 : 2, opp = b < 2 ? 2 : 0;
    /* getOpponents(false): the other side's field, not fainted, in field order */
    pr_pokemon *opps[2];
    int no = 0;
    for (int i = opp; i < opp + 2; i++)
        if (active(bt->field[i])) opps[no++] = bt->field[i];

    /* applyMoveAttrs("VariableTargetAttr", user, p, move, variableTarget) for each opponent */
    int target = replace_target >= 0 ? replace_target : mv->target;
    for (int k = 0; k < no; k++)
        for (unsigned i = 0; i < mv->nattr; i++) {
            const pr_attr *a = &pr_attrs[mv->attr0 + i];
            if (!pr_attr_is(a->cls, PR_AC_VariableTargetAttr)) continue;
            const pr_attr_VariableTargetAttr *x = a->p;
            target = (int)pr_closure_call(bt, x->targetChangeFunc, PR_AC_VariableTargetAttr, target, user, opps[k], move);
        }

    /* getAlly(): the other slot of the user's own side (field index 0 <-> 1) */
    pr_pokemon *ally = bt->field[own + ((b - own) ? 0 : 1)];
    pr_pokemon *set[5];
    int n = 0;
    *multiple = false;
    switch (target) {
    case MOVE_TARGET_CURSE: {
        /* a non-Ghost user targets itself (original types when Stellar) */
        int8_t ty[PR_MAX_TYPES];
        int nt = pr_get_types(user, ty, PR_TYPES_ORIGINAL_IF_STELLAR);
        bool ghost = false;
        for (int i = 0; i < nt; i++) ghost |= ty[i] == POKEMON_TYPE_GHOST;
        if (!ghost) { set[n++] = (pr_pokemon *)user; break; }
    }
    /* fall through */
    case MOVE_TARGET_NEAR_OTHER: case MOVE_TARGET_OTHER: case MOVE_TARGET_ALL_NEAR_OTHERS: case MOVE_TARGET_ALL_OTHERS:
        for (int k = 0; k < no; k++) set[n++] = opps[k];
        if (ally) set[n++] = ally;
        *multiple = target == MOVE_TARGET_ALL_NEAR_OTHERS || target == MOVE_TARGET_ALL_OTHERS;
        break;
    case MOVE_TARGET_USER: case MOVE_TARGET_PARTY:
        set[n++] = (pr_pokemon *)user;
        break;
    case MOVE_TARGET_NEAR_ENEMY: case MOVE_TARGET_ALL_NEAR_ENEMIES: case MOVE_TARGET_ALL_ENEMIES: case MOVE_TARGET_ENEMY_SIDE:
        for (int k = 0; k < no; k++) set[n++] = opps[k];
        *multiple = target != MOVE_TARGET_NEAR_ENEMY;
        break;
    case MOVE_TARGET_RANDOM_NEAR_ENEMY: {
        int k = pr_battle_rand_seed_int(bt, no, 0);
        if (k < no) set[n++] = opps[k];                    /* [undefined] filters out */
        break;
    }
    case MOVE_TARGET_ATTACKER:
        out[0] = BATTLER_INDEX_ATTACKER;
        return 1;
    case MOVE_TARGET_NEAR_ALLY: case MOVE_TARGET_ALLY:
        if (ally) set[n++] = ally;
        break;
    case MOVE_TARGET_USER_OR_NEAR_ALLY: case MOVE_TARGET_USER_AND_ALLIES: case MOVE_TARGET_USER_SIDE:
        set[n++] = (pr_pokemon *)user;
        if (ally) set[n++] = ally;
        *multiple = target != MOVE_TARGET_USER_OR_NEAR_ALLY;
        break;
    case MOVE_TARGET_ALL: case MOVE_TARGET_BOTH_SIDES:
        set[n++] = (pr_pokemon *)user;
        if (ally) set[n++] = ally;
        for (int k = 0; k < no; k++) set[n++] = opps[k];
        *multiple = true;
        break;
    }
    /* .filter(p => p?.isActive(true)).map(p => p.getBattlerIndex()) */
    int m = 0;
    for (int k = 0; k < n && m < 4; k++)
        if (active_on_field(set[k])) out[m++] = (int8_t)pr_battler_index(bt, set[k]);
    return m;
}

bool pr_move_is_multi_target(int move)
{
    switch (pr_moves[move].target) {
    case MOVE_TARGET_ALL_OTHERS: case MOVE_TARGET_ALL_NEAR_OTHERS: case MOVE_TARGET_ALL_NEAR_ENEMIES:
    case MOVE_TARGET_ALL_ENEMIES: case MOVE_TARGET_USER_AND_ALLIES: case MOVE_TARGET_ALL: case MOVE_TARGET_USER_SIDE:
    case MOVE_TARGET_ENEMY_SIDE: case MOVE_TARGET_BOTH_SIDES:
        return true;
    }
    return false;
}

/* ---------------------------------------------------------------- Instruct */

/* RepeatMoveAttr.movesetMove: the condition keeps the target's moveset move for apply (on the
 * attribute object upstream; apply always follows its own condition within the MovePhase) */
static const pr_pokemon *instruct_holder;
static int instruct_move_slot = -1;

void pr_instruct_keep(const pr_pokemon *p, int slot) { instruct_holder = p; instruct_move_slot = slot; }
int pr_instruct_slot(const pr_pokemon *p) { return p && p == instruct_holder ? instruct_move_slot : -1; }

/* invalidInstructMoves (data/moves/invalid-moves.ts) */
static const uint16_t invalid_instruct[] = {
    MOVE_ID_OUTRAGE, MOVE_ID_RAGING_FURY, MOVE_ID_ROLLOUT, MOVE_ID_PETAL_DANCE, MOVE_ID_THRASH, MOVE_ID_ICE_BALL,
    MOVE_ID_UPROAR, MOVE_ID_BIDE, MOVE_ID_SHELL_TRAP, MOVE_ID_BEAK_BLAST, MOVE_ID_FOCUS_PUNCH, MOVE_ID_FAKE_OUT,
    MOVE_ID_FIRST_IMPRESSION, MOVE_ID_MAT_BLOCK, MOVE_ID_HYPER_BEAM, MOVE_ID_ETERNABEAM, MOVE_ID_FRENZY_PLANT,
    MOVE_ID_BLAST_BURN, MOVE_ID_HYDRO_CANNON, MOVE_ID_GIGA_IMPACT, MOVE_ID_PRISMATIC_LASER, MOVE_ID_ROAR_OF_TIME,
    MOVE_ID_ROCK_WRECKER, MOVE_ID_METEOR_ASSAULT, MOVE_ID_DIG, MOVE_ID_FLY, MOVE_ID_BOUNCE, MOVE_ID_SHADOW_FORCE,
    MOVE_ID_PHANTOM_FORCE, MOVE_ID_DIVE, MOVE_ID_ELECTRO_SHOT, MOVE_ID_ICE_BURN, MOVE_ID_GEOMANCY,
    MOVE_ID_FREEZE_SHOCK, MOVE_ID_SKY_DROP, MOVE_ID_SKY_ATTACK, MOVE_ID_SKULL_BASH, MOVE_ID_SOLAR_BEAM,
    MOVE_ID_SOLAR_BLADE, MOVE_ID_METEOR_BEAM,
    /* moveCallingMoves */
    MOVE_ID_ASSIST, MOVE_ID_COPYCAT, MOVE_ID_ME_FIRST, MOVE_ID_METRONOME, MOVE_ID_MIRROR_MOVE, MOVE_ID_NATURE_POWER,
    MOVE_ID_SLEEP_TALK, MOVE_ID_SNATCH,
    MOVE_ID_INSTRUCT, MOVE_ID_SKETCH, MOVE_ID_TRANSFORM, MOVE_ID_MIMIC, MOVE_ID_STRUGGLE,
};

bool pr_instruct_invalid(int move)
{
    for (unsigned i = 0; i < sizeof invalid_instruct / sizeof *invalid_instruct; i++)
        if (invalid_instruct[i] == move) return true;
    return false;
}
