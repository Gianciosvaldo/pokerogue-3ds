/* Saving and loading a run (gc_run_save / gc_run_load).
 *
 * Upstream saves the session at the start of each encounter (gameData.saveAll in EncounterPhase), with
 * the party's summon data, the enemy party and the arena, so a reload comes back to that battle. game-core
 * saves at the same point: gc_run_can_save is true from GC_EV_SAVE_POINT until the battle's first turn
 * runs, while the COMMAND screen of turn 1 waits. A load comes back to that screen with the run exactly
 * as it was, so what follows is what would have followed without the reload.
 *
 * The format is made to outlive changes to battle-core's and game-core's structs: every value is stored
 * under its field's name (a hash) and size, and a load takes each field it knows from a field the save
 * has with the same name and size. A field the save doesn't have, or has with another size (the struct
 * changed between builds), keeps its default and counts in run->load_dropped; a field only the save has
 * is skipped. Little-endian hosts only (the 3DS and PCs); a save moves between them as it is.
 *
 *   "PR3DSRUN" | u16 format | u16 GC_API_VERSION | u32 body length | u32 CRC-32 of the body | body
 *   body:    sections: u32 name hash | u32 length | its blocks
 *   block:   u16 n | n x (u32 field name hash, u16 size) | u16 count | count x the fields' bytes in order */
#include "internal.h"
#include "gc/football.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_FORMAT 2
static const char magic[8] = {'P', 'R', '3', 'D', 'S', 'R', 'U', 'N'};
static const char sys_magic[8] = {'P', 'R', '3', 'D', 'S', 'S', 'Y', 'S'};

/* ---------------------------------------------------------------- fields */
typedef struct { const char *name; uint32_t off; uint16_t size; } sfield;
#define SF(T, m) {#m, (uint32_t)offsetof(T, m), (uint16_t)sizeof(((T *)0)->m)}
#define NF(a) ((int)(sizeof(a) / sizeof((a)[0])))

static uint32_t hash(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (uint8_t)*s++; h *= 16777619u; }
    return h;
}

/* The run's own values, in types that are the same size on every host */
typedef struct {
    int32_t mode;
    bool has_challenges, offset_gym, daily_custom;
    uint16_t seed[64], seed_len, wave_seed[64], wave_seed_len;
    pr_rng rng;
    pr_rng battle_rng;
    bool battle_rng_live;
    int32_t wave, biome, wave_cycle_offset;
    double money;
    int32_t score, trainer_id, secret_id;
    uint8_t pokeballs[8];
    int32_t n_party;
    int8_t party_order[6];
    int32_t n_items, n_enemy_items;
    pr_me_save me;
    uint8_t n_mystery_history;
    int8_t mystery_type, mystery_tier;
    bool in_battle;
    bool me_battle, me_catch_allowed;
    int32_t battle_type, trainer_type, trainer_variant, trainer_name, trainer_partner_name;
    char battle_seed[17];
    int32_t n_enemy;
    /* game-core's own */
    int32_t last_battle_type;
    bool last_final_boss, lock_rarities;
    double last_max_exp_level;
    uint32_t stub_rng;
    /* the arena a same-biome battle keeps (gc_battle_setup_field) */
    uint8_t weather, terrain;
    int16_t weather_turns_left, weather_max_duration, terrain_turns_left, terrain_max_duration;
    int32_t player_faints, n_arena_tags, n_pos_tags;
    double player_teras_used;
    bool frozen_names; /* new saves require both halves of stored names */
} sv_run;
typedef struct { uint8_t double_battle; } sv_encounter;
static const sfield encounter_fields[]={SF(sv_encounter,double_battle)};
static const sfield illusion_fields[]={SF(pr_illusion,species),SF(pr_illusion,fusion_species),
    SF(pr_illusion,form),SF(pr_illusion,fusion_form),SF(pr_illusion,gender),SF(pr_illusion,active)};
typedef struct { uint8_t terminal_result; } sv_terminal;
static const sfield terminal_fields[]={SF(sv_terminal,terminal_result)};
static const sfield run_fields[] = {
    SF(sv_run, mode), SF(sv_run, has_challenges), SF(sv_run, offset_gym), SF(sv_run, daily_custom),
    SF(sv_run, seed), SF(sv_run, seed_len), SF(sv_run, wave_seed), SF(sv_run, wave_seed_len), SF(sv_run, rng),
    SF(sv_run, battle_rng), SF(sv_run, battle_rng_live),
    SF(sv_run, wave), SF(sv_run, biome), SF(sv_run, wave_cycle_offset), SF(sv_run, money), SF(sv_run, score),
    SF(sv_run, trainer_id), SF(sv_run, secret_id), SF(sv_run, pokeballs), SF(sv_run, n_party),
    SF(sv_run, party_order), SF(sv_run, n_items), SF(sv_run, n_enemy_items), SF(sv_run, me),
    SF(sv_run, n_mystery_history), SF(sv_run, mystery_type), SF(sv_run, mystery_tier),
    SF(sv_run, in_battle), SF(sv_run, me_battle), SF(sv_run, me_catch_allowed), SF(sv_run, battle_type), SF(sv_run, trainer_type), SF(sv_run, trainer_variant),
    SF(sv_run, trainer_name), SF(sv_run, trainer_partner_name),
    SF(sv_run, battle_seed), SF(sv_run, n_enemy), SF(sv_run, last_battle_type), SF(sv_run, last_final_boss),
    SF(sv_run, lock_rarities), SF(sv_run, last_max_exp_level), SF(sv_run, stub_rng), SF(sv_run, weather),
    SF(sv_run, terrain), SF(sv_run, weather_turns_left), SF(sv_run, weather_max_duration),
    SF(sv_run, terrain_turns_left), SF(sv_run, terrain_max_duration), SF(sv_run, player_faints),
    SF(sv_run, n_arena_tags), SF(sv_run, n_pos_tags),
    SF(sv_run, frozen_names), SF(sv_run, player_teras_used),
};

static const sfield depth_fields[] = {SF(pr_biome_depths, set), SF(pr_biome_depths, depth), SF(pr_biome_depths, chance)};
static const sfield mystery_history_fields[] = {
    SF(gc_mystery_seen, type), SF(gc_mystery_seen, tier), SF(gc_mystery_seen, wave),
};

/* An item, its key as text (gc_item.key is a pointer) */
typedef struct { char key[40]; int16_t arg; int32_t stack; uint32_t pokemon_id; bool detached; int32_t battle_count; bool non_transferable; bool eviolite_base; bool form_inactive; } sv_item;
static const sfield item_fields[] = {SF(sv_item, key), SF(sv_item, arg), SF(sv_item, stack), SF(sv_item, pokemon_id), SF(sv_item, detached), SF(sv_item, battle_count), SF(sv_item, non_transferable), SF(sv_item, eviolite_base), SF(sv_item, form_inactive)};

/* What the run keeps of a Pokemon beside battle-core's part */
static const sfield mon_fields[] = {
    SF(gc_mon, exp), SF(gc_mon, ability_index), SF(gc_mon, variant), SF(gc_mon, pokerus),
    SF(gc_mon, pause_evolutions), SF(gc_mon, luck), SF(gc_mon, pokeball), SF(gc_mon, met_wave),
    SF(gc_mon, trainer_slot), SF(gc_mon, met_level), SF(gc_mon, met_biome), SF(gc_mon, met_species),
    SF(gc_mon, used_tms), SF(gc_mon, n_used_tms),
    SF(gc_mon, base_nature), SF(gc_mon, has_custom_nature),
};

/* battle-core's Pokemon: everything but its turnData (reset before every turn) and the lists, which
 * have blocks of their own */
static const sfield pokemon_fields[] = {
    SF(pr_pokemon, id), SF(pr_pokemon, species), SF(pr_pokemon, form),
    SF(pr_pokemon, fusion_species), SF(pr_pokemon, fusion_form), SF(pr_pokemon, fusion_ability_index),
    SF(pr_pokemon, level), SF(pr_pokemon, ivs),
    SF(pr_pokemon, nature), SF(pr_pokemon, ability), SF(pr_pokemon, base_ability), SF(pr_pokemon, ability_override),
    SF(pr_pokemon, passive), SF(pr_pokemon, passive_flag), SF(pr_pokemon, custom_passive), SF(pr_pokemon, added_type),
    SF(pr_pokemon, is_player), SF(pr_pokemon, stats), SF(pr_pokemon, summon_stats), SF(pr_pokemon, hp),
    SF(pr_pokemon, stat_stages), SF(pr_pokemon, status), SF(pr_pokemon, status_toxic_turns),
    SF(pr_pokemon, status_sleep_turns), SF(pr_pokemon, status_freeze_turns), SF(pr_pokemon, terastallized),
    SF(pr_pokemon, shiny), SF(pr_pokemon, tera_type), SF(pr_pokemon, summon_types), SF(pr_pokemon, n_summon_types),
    SF(pr_pokemon, moves), SF(pr_pokemon, pp_used), SF(pr_pokemon, pp_up), SF(pr_pokemon, max_pp_override),
    SF(pr_pokemon, n_moves), SF(pr_pokemon, gender), SF(pr_pokemon, friendship), SF(pr_pokemon, has_trainer),
    SF(pr_pokemon, is_boss), SF(pr_pokemon, boss_segments), SF(pr_pokemon, boss_segment_index), SF(pr_pokemon, n_tags), SF(pr_pokemon, stellar_types_boosted), SF(pr_pokemon, on_field),
    SF(pr_pokemon, switch_out_status), SF(pr_pokemon, ability_suppressed), SF(pr_pokemon, summon_hit_count),
    SF(pr_pokemon, n_move_history), SF(pr_pokemon, n_move_queue), SF(pr_pokemon, temp_turn_count),
    SF(pr_pokemon, temp_wave_turn_count), SF(pr_pokemon, ab_applied_summon), SF(pr_pokemon, ab_applied_wave),
    SF(pr_pokemon, n_ab_applied_summon), SF(pr_pokemon, n_ab_applied_wave), SF(pr_pokemon, has_eaten_berry),
    SF(pr_pokemon, transformed), SF(pr_pokemon, transform_species), SF(pr_pokemon, transform_form),
    SF(pr_pokemon, transform_gender), SF(pr_pokemon, transform_moves), SF(pr_pokemon, transform_pp_used),
    SF(pr_pokemon, transform_pp_up), SF(pr_pokemon, transform_max_pp_override), SF(pr_pokemon, transform_n_moves),
    SF(pr_pokemon, summon_moves_active), SF(pr_pokemon, summon_moves_alias_mask),
    SF(pr_pokemon, variant),
    SF(pr_pokemon, ability_lookup_known), SF(pr_pokemon, ability_index),
    SF(pr_pokemon, custom_ability), SF(pr_pokemon, fusion_custom_ability),
    SF(pr_pokemon, lookup_ability_override), SF(pr_pokemon, daily_boss_ability),
    SF(pr_pokemon, temporary_fusion_species), SF(pr_pokemon, temporary_fusion_form),
    SF(pr_pokemon, battle_berries), SF(pr_pokemon, turn_berries), SF(pr_pokemon, last_berries),
    SF(pr_pokemon, n_battle_berries), SF(pr_pokemon, n_turn_berries), SF(pr_pokemon, n_last_berries),
};
static const sfield history_used_fields[]={SF(pr_pokemon,history_used_moves),SF(pr_pokemon,history_used_complete)};
static const sfield history_last_fields[]={SF(pr_pokemon,history_last_known),SF(pr_pokemon,history_last_present),SF(pr_pokemon,history_serial),SF(pr_pokemon,history_last_serial)};
static const sfield history_all_fields[]={SF(pr_pokemon,history_all_known),SF(pr_pokemon,history_all_present),SF(pr_pokemon,history_aim_serial)};
static const sfield protect_history_fields[]={SF(pr_pokemon,history_protect_count),SF(pr_pokemon,history_protect_complete)};
static const sfield tag_fields[] = {
    SF(pr_battler_tag, type), SF(pr_battler_tag, turn_count), SF(pr_battler_tag, source_move),
    SF(pr_battler_tag, source_id), SF(pr_battler_tag, crit_stages), SF(pr_battler_tag, stockpiled_count),
    SF(pr_battler_tag, stock_def), SF(pr_battler_tag, stock_spdef), SF(pr_battler_tag, autotomize_count),
    SF(pr_battler_tag, faint_count), SF(pr_battler_tag, stat), SF(pr_battler_tag, multiplier), SF(pr_battler_tag, hp),
    SF(pr_battler_tag, in_focus), SF(pr_battler_tag, source_index), SF(pr_battler_tag, activated),
    SF(pr_battler_tag, base_pure_flying), SF(pr_battler_tag, move_id), SF(pr_battler_tag, added_this_turn),
    SF(pr_battler_tag, commanded_form_key),
};
static const sfield turn_move_fields[] = {
    SF(pr_turn_move, move), SF(pr_turn_move, use_mode), SF(pr_turn_move, result), SF(pr_turn_move, targets),
    SF(pr_turn_move, n_targets),
};
static const sfield arena_tag_fields[] = {
    SF(pr_arena_tag, type), SF(pr_arena_tag, side), SF(pr_arena_tag, turn_count), SF(pr_arena_tag, max_duration),
    SF(pr_arena_tag, source_move), SF(pr_arena_tag, source_id), SF(pr_arena_tag, source_count),
    SF(pr_arena_tag, layers), SF(pr_arena_tag, being_removed),
    SF(pr_arena_tag, n_pending_heals),
};
static const sfield pos_tag_fields[] = {
    SF(pr_positional_tag, type), SF(pr_positional_tag, turn_count), SF(pr_positional_tag, target_index),
    SF(pr_positional_tag, source_id), SF(pr_positional_tag, source_move), SF(pr_positional_tag, heal_hp),
    SF(pr_positional_tag, pokemon_name), SF(pr_positional_tag, fusion_name),
};
static const sfield pending_heal_fields[] = {
    SF(pr_pending_heal, source_id), SF(pr_pending_heal, pokemon_name),
    SF(pr_pending_heal, move_id), SF(pr_pending_heal, target_index), SF(pr_pending_heal, restore_pp),
    SF(pr_pending_heal, fusion_name),
};
/* the soak runs' counters travel with the run, so a reloaded run reports what the whole run hit */
static const sfield stat_fields[] = {SF(gc_run, unported), SF(gc_run, stubs), SF(gc_run, run_core_unported)};

/* ---------------------------------------------------------------- writing */
typedef struct { uint8_t *p; size_t n, cap; bool full; } writer;

static void put(writer *w, const void *v, size_t n)
{
    if (w->full || n > w->cap - w->n) { w->full = true; return; }
    memcpy(w->p + w->n, v, n);
    w->n += n;
}
static void put16(writer *w, uint16_t v) { put(w, &v, 2); }
static void put32(writer *w, uint32_t v) { put(w, &v, 4); }

static void put_block(writer *w, const sfield *f, int nf, const void *base, size_t stride, int count)
{
    put16(w, (uint16_t)nf);
    for (int i = 0; i < nf; i++) { put32(w, hash(f[i].name)); put16(w, f[i].size); }
    put16(w, (uint16_t)count);
    for (int k = 0; k < count; k++)
        for (int i = 0; i < nf; i++) put(w, (const uint8_t *)base + (size_t)k * stride + f[i].off, f[i].size);
}

static size_t section_start(writer *w, const char *name)
{
    put32(w, hash(name));
    put32(w, 0);
    return w->n;
}
static void section_end(writer *w, size_t start)
{
    if (w->full) return;
    uint32_t len = (uint32_t)(w->n - start);
    memcpy(w->p + start - 4, &len, 4);
}

/* ---------------------------------------------------------------- reading */
typedef struct { const uint8_t *p; size_t n, pos; bool bad; } reader;

static bool take(reader *r, void *v, size_t n)
{
    if (r->bad || n > r->n - r->pos) { r->bad = true; memset(v, 0, n); return false; }
    memcpy(v, r->p + r->pos, n);
    r->pos += n;
    return true;
}
static uint16_t take16(reader *r) { uint16_t v; take(r, &v, 2); return v; }
static uint32_t take32(reader *r) { uint32_t v; take(r, &v, 4); return v; }

/* The section `name`, as a reader of its own; an empty one when the save hasn't got it */
static reader section(const reader *body, const char *name)
{
    const uint32_t h = hash(name);
    reader r = *body;
    r.pos = 0;
    while (r.pos + 8 <= r.n) {
        uint32_t sh = take32(&r), len = take32(&r);
        if (len > r.n - r.pos) break;
        if (sh == h) { reader s = {r.p + r.pos, len, 0, false}; return s; }
        r.pos += len;
    }
    reader none = {NULL, 0, 0, true};
    return none;
}

/* The next block into base[0 .. count-1] (stride apart), at most max records; returns the count. The
 * fields it can't fill keep what base holds and count in *dropped. */
static int get_block(reader *r, const sfield *f, int nf, void *base, size_t stride, int max, int *dropped)
{
    enum { MAX_SAVED = 128 };
    uint32_t sh[MAX_SAVED];
    uint16_t ss[MAX_SAVED];
    const int n = take16(r);
    if (r->bad || n > MAX_SAVED) { r->bad = true; *dropped += nf; return 0; }
    size_t rec = 0;
    for (int i = 0; i < n; i++) { sh[i] = take32(r); ss[i] = take16(r); rec += ss[i]; }
    const int count = take16(r);
    if (r->bad || rec * (size_t)count > r->n - r->pos) { r->bad = true; *dropped += nf; return 0; }
    /* where each known field sits in a saved record, -1 = not there */
    long at[MAX_SAVED];
    bool legacy_level[MAX_SAVED] = {false};
    bool legacy_sub_hp[MAX_SAVED] = {false};
    for (int i = 0; i < nf && i < MAX_SAVED; i++) {
        at[i] = -1;
        const uint32_t h = hash(f[i].name);
        size_t off = 0;
        for (int k = 0; k < n; k++) {
            /* Only the two historical byte-sized Pokemon level fields may
             * widen. All unrelated size mismatches retain normal drop behavior. */
            bool byte_level = ss[k] == 1 && f[i].size == sizeof(int32_t)
                && ((f == pokemon_fields && f[i].off == offsetof(pr_pokemon, level))
                    || (f == mon_fields && f[i].off == offsetof(gc_mon, met_level)));
            bool sub_hp = ss[k] == sizeof(int32_t) && f[i].size == sizeof(double)
                && f == tag_fields && f[i].off == offsetof(pr_battler_tag, hp);
            if (sh[k] == h && (ss[k] == f[i].size || byte_level || sub_hp)) {
                at[i] = (long)off; legacy_level[i] = byte_level; legacy_sub_hp[i] = sub_hp; break;
            }
            off += ss[k];
        }
        if (at[i] < 0 && count > 0) (*dropped)++;
    }
    const int keep = count < max ? count : max;
    for (int k = 0; k < keep; k++) {
        const uint8_t *src = r->p + r->pos + rec * (size_t)k;
        uint8_t *dst = (uint8_t *)base + (size_t)k * stride;
        for (int i = 0; i < nf && i < MAX_SAVED; i++)
            if (at[i] >= 0) {
                if (legacy_level[i]) {
                    int32_t level = src[at[i]];
                    memcpy(dst + f[i].off, &level, sizeof level);
                } else if (legacy_sub_hp[i]) {
                    int32_t old_hp;
                    memcpy(&old_hp, src + at[i], sizeof old_hp);
                    double hp = old_hp;
                    memcpy(dst + f[i].off, &hp, sizeof hp);
                } else memcpy(dst + f[i].off, src + at[i], f[i].size);
            }
    }
    if (count > max) (*dropped)++;
    r->pos += rec * (size_t)count;
    return keep;
}

/* New temporary state is optional for older, untransformed saves, but all of
 * its fields are necessary to restore an actively transformed Pokemon. */
static bool has_transform_schema(reader r)
{
    uint32_t names[128]; uint16_t sizes[128];
    int n = take16(&r);
    if (r.bad || n > 128) return false;
    for (int i = 0; i < n; i++) { names[i] = take32(&r); sizes[i] = take16(&r); }
    if (r.bad) return false;
    for (int i = 0; i < NF(pokemon_fields); i++) {
        const sfield *field = &pokemon_fields[i];
        if (field->off < offsetof(pr_pokemon, transformed)
            || field->off > offsetof(pr_pokemon, transform_max_pp_override)) continue;
        bool found = false;
        for (int k = 0; k < n; k++)
            if (names[k] == hash(field->name) && sizes[k] == field->size) found = true;
        if (!found) return false;
    }
    return true;
}

/* Historical saves have no berry schema. A new save must carry all six fields
 * together, so a partial record cannot silently lose its consumption history. */
static bool berry_schema_ok(reader r)
{
    const char *fields[]={"battle_berries","turn_berries","last_berries","n_battle_berries","n_turn_berries","n_last_berries"};
    const uint16_t sizes[]={PR_MAX_BERRY_HISTORY,PR_MAX_BERRY_HISTORY,PR_MAX_BERRY_HISTORY,2,2,2};
    bool present[6]={false}; int n=take16(&r), found=0;
    if(r.bad || n>128)return false;
    for(int i=0;i<n;i++){
        uint32_t name=take32(&r);uint16_t size=take16(&r);
        for(int k=0;k<6;k++)if(name==hash(fields[k])){
            if(size!=sizes[k] || present[k])return false;
            present[k]=true;found++;
        }
    }
    return !r.bad && (found==0 || found==6);
}

/* Keep historical concrete caches when lookup inputs are absent. New metadata
 * is an indivisible schema: partial records must not create default overrides. */
static bool ability_schema_ok(reader r)
{
    const char *fields[]={"ability_lookup_known","ability_index","custom_ability","fusion_custom_ability",
        "lookup_ability_override","daily_boss_ability","temporary_fusion_species","temporary_fusion_form"};
    const uint16_t sizes[]={1,1,2,2,2,2,2,1};
    bool present[8]={false}; int n=take16(&r), found=0;
    if(r.bad || n>128)return false;
    for(int i=0;i<n;i++){
        uint32_t name=take32(&r);uint16_t size=take16(&r);
        for(int k=0;k<8;k++)if(name==hash(fields[k])){
            if(size!=sizes[k] || present[k])return false;
            present[k]=true;found++;
        }
    }
    return !r.bad && (found==0 || found==8);
}

/* Old Transform-only saves have no alias metadata. New moveset identity is atomic. */
static bool moveset_schema_ok(reader r)
{
    const char *fields[]={"summon_moves_active","summon_moves_alias_mask"};
    bool present[2]={false};int count=0,n=take16(&r);
    if(r.bad || n>128)return false;
    for(int i=0;i<n;i++){
        uint32_t name=take32(&r);uint16_t size=take16(&r);
        for(int k=0;k<2;k++)if(name==hash(fields[k])){
            if(size!=1 || present[k])return false;
            present[k]=true;count++;
        }
    }
    return !r.bad && (count==0 || count==2);
}

/* ---------------------------------------------------------------- CRC-32 (IEEE) */
static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= p[i];
        for (int k = 0; k < 8; k++) c = c >> 1 ^ (0xEDB88320u & (0u - (c & 1)));
    }
    return ~c;
}

static bool little_endian(void)
{
    const uint16_t one = 1;
    return *(const uint8_t *)&one == 1;
}

/* ---------------------------------------------------------------- items' keys */
/* gc_item.key must outlive the run: a loaded key becomes one of these */
static const char *intern(const char *key)
{
    static char pool[256][40];
    static int n;
    for (int i = 0; i < n; i++) if (!strcmp(pool[i], key)) return pool[i];
    if (n >= 256) return NULL;
    size_t l = 0;
    while (key[l] && l < sizeof pool[n] - 1) { pool[n][l] = key[l]; l++; }
    pool[n][l] = 0;
    return pool[n++];
}

/* ---------------------------------------------------------------- the calls */
bool gc_run_can_save(const gc_run *run)
{
    if(!run||run->n_items<0||run->n_enemy_items<0||run->n_items>PR_MAX_MODIFIERS||run->n_enemy_items>PR_MAX_MODIFIERS)return false;
    if (run->terminal_result) return run->finished && run->terminal_result >= 1 && run->terminal_result <= 2
        && !run->in_battle && run->run.mode == GAME_MODES_CLASSIC
        && run->n_party >= 1 && run->n_party <= 6 && run->n_enemy >= 1 && run->n_enemy <= 6
        && run->n_items >= 0 && run->n_items <= GC_MAX_ITEMS
        && run->n_enemy_items >= 0 && run->n_enemy_items <= GC_MAX_ITEMS
        && run->wave >= 1 && (run->terminal_result == 2 ? run->wave >= 200 && !run->run.has_challenges : run->wave <= 200)
        && run->screen.kind == (run->terminal_result == 2 ? GC_SCREEN_CLEAR : GC_SCREEN_GAME_OVER);
    gc_internal *in = gc_int((gc_run *)run);
    if (!in->at_save_point || !run->in_battle || run->screen.kind != GC_SCREEN_COMMAND) return false;
    /* only the turn this COMMAND screen belongs to may be waiting: the load starts it again */
    const gc_rqueue *q = &in->rq;
    if (q->current.type != RP_COMMAND) return false;
    for (int l = 0; l < q->n_levels; l++)
        for (int k = 0; k < q->n[l]; k++) if (q->q[l][k].type != RP_TURN
            && !(run->double_battle&&q->q[l][k].type==RP_COMMAND&&q->q[l][k].a==1)) return false;
    return true;
}

static void put_mons(writer *w, const char *name, const gc_mon *mons, int n)
{
    size_t s = section_start(w, name);
    put_block(w, mon_fields, NF(mon_fields), mons, sizeof(gc_mon), n);
    put_block(w, pokemon_fields, NF(pokemon_fields), &mons[0].b, sizeof(gc_mon), n);
    for (int i = 0; i < n; i++) {
        const pr_pokemon *b = &mons[i].b;
        const int nh = b->n_move_history < PR_MAX_MOVE_HISTORY ? b->n_move_history : PR_MAX_MOVE_HISTORY;
        put_block(w, tag_fields, NF(tag_fields), b->tags, sizeof b->tags[0], b->n_tags);
        put_block(w, turn_move_fields, NF(turn_move_fields), b->move_history, sizeof b->move_history[0], nh);
        put_block(w, turn_move_fields, NF(turn_move_fields), b->move_queue, sizeof b->move_queue[0], b->n_move_queue);
    }
    section_end(w, s);
    char key[48];snprintf(key,sizeof key,"%s_move_uses",name);
    s=section_start(w,key);
    put_block(w,history_used_fields,NF(history_used_fields),&mons[0].b,sizeof(gc_mon),n);
    section_end(w,s);
    snprintf(key,sizeof key,"%s_last_moves",name);
    s=section_start(w,key);
    put_block(w,history_last_fields,NF(history_last_fields),&mons[0].b,sizeof(gc_mon),n);
    for(int i=0;i<n;i++)put_block(w,turn_move_fields,NF(turn_move_fields),mons[i].b.history_last,sizeof(pr_turn_move),4);
    section_end(w,s);
    snprintf(key,sizeof key,"%s_all_moves",name);
    s=section_start(w,key);
    put_block(w,history_all_fields,NF(history_all_fields),&mons[0].b,sizeof(gc_mon),n);
    for(int i=0;i<n;i++)put_block(w,turn_move_fields,NF(turn_move_fields),&mons[i].b.history_aim,sizeof(pr_turn_move),1);
    section_end(w,s);
    snprintf(key,sizeof key,"%s_protect",name);
    s=section_start(w,key);
    put_block(w,protect_history_fields,NF(protect_history_fields),&mons[0].b,sizeof(gc_mon),n);
    section_end(w,s);
}

static void put_items(writer *w, const char *name, const gc_item *items, int n)
{
    static sv_item sv[GC_MAX_ITEMS];
    memset(sv, 0, sizeof sv);
    for (int i = 0; i < n; i++) {
        snprintf(sv[i].key, sizeof sv[i].key, "%s", items[i].key ? items[i].key : "");
        sv[i].arg = items[i].arg;
        sv[i].stack = items[i].stack;
        sv[i].pokemon_id = items[i].pokemon_id;
        sv[i].detached = items[i].detached;
        sv[i].battle_count = items[i].battle_count;
        sv[i].non_transferable = items[i].non_transferable;
        sv[i].eviolite_base = items[i].eviolite_base;
        if (items[i].form_inactive && (!gc_item_key_form_change(items[i].key) || !pr_form_item_known(items[i].arg))) {
            w->full = true;
            return;
        }
        sv[i].form_inactive = items[i].form_inactive;
    }
    size_t s = section_start(w, name);
    put_block(w, item_fields, NF(item_fields), sv, sizeof sv[0], n);
    section_end(w, s);
}

size_t gc_run_save(const gc_run *run, void *buf, size_t cap)
{
    if (!gc_run_can_save(run) || !little_endian() || cap < 24
        || run->n_mystery_history > GC_MAX_MYSTERY_HISTORY) return 0;
    gc_internal *in = gc_int((gc_run *)run);
    const pr_battle *bt = &run->bt;
    sv_run v;
    memset(&v, 0, sizeof v);
    v.mode = run->run.mode;
    v.has_challenges = run->run.has_challenges;
    v.offset_gym = run->run.offset_gym;
    v.daily_custom = run->run.daily_custom;
    memcpy(v.seed, run->run.seed.u, sizeof v.seed);
    v.seed_len = (uint16_t)run->run.seed.len;
    memcpy(v.wave_seed, run->run.wave_seed.u, sizeof v.wave_seed);
    v.wave_seed_len = (uint16_t)run->run.wave_seed.len;
    v.rng = run->run.rng;
    v.battle_rng = bt->battle_rng;
    v.battle_rng_live = bt->battle_rng_live;
    v.wave = run->wave;
    v.biome = run->biome;
    v.wave_cycle_offset = run->wave_cycle_offset;
    v.money = run->money;
    v.score = run->score;
    v.trainer_id = run->trainer_id;
    v.secret_id = run->secret_id;
    memcpy(v.pokeballs, run->pokeballs, sizeof v.pokeballs);
    v.n_party = run->n_party;
    memcpy(v.party_order, run->party_order, sizeof v.party_order);
    v.n_items = run->n_items;
    v.n_enemy_items = run->n_enemy_items;
    v.me = run->me;
    v.n_mystery_history = run->n_mystery_history;
    v.mystery_type = run->mystery_type;
    v.mystery_tier = run->mystery_tier;
    v.in_battle = run->in_battle;
    v.me_battle = run->me_battle;
    v.me_catch_allowed = run->me_catch_allowed;
    v.battle_type = run->battle_type;
    v.trainer_type = run->trainer_type;
    v.trainer_variant = run->trainer_variant;
    v.trainer_name = run->trainer_name;
    v.trainer_partner_name = run->trainer_partner_name;
    memcpy(v.battle_seed, run->battle_seed, sizeof v.battle_seed);
    v.n_enemy = run->n_enemy;
    v.last_battle_type = in->last_battle_type;
    v.last_final_boss = in->last_final_boss;
    v.lock_rarities = in->lock_rarities;
    v.last_max_exp_level = in->last_max_exp_level;
    v.stub_rng = in->stub_rng;
    v.weather = bt->weather;
    v.terrain = bt->terrain;
    v.weather_turns_left = bt->weather_turns_left;
    v.weather_max_duration = bt->weather_max_duration;
    v.terrain_turns_left = bt->terrain_turns_left;
    v.terrain_max_duration = bt->terrain_max_duration;
    v.player_faints = bt->player_faints;
    v.player_teras_used = bt->player_teras_used;
    v.n_arena_tags = bt->n_arena_tags;
    v.n_pos_tags = bt->n_pos_tags;
    v.frozen_names = true;

    writer w = {(uint8_t *)buf, 0, cap, false};
    put(&w, magic, 8);
    put16(&w, SAVE_FORMAT);
    put16(&w, GC_API_VERSION);
    put32(&w, 0);
    put32(&w, 0);
    const size_t body = w.n;
    size_t s = section_start(&w, "run");
    put_block(&w, run_fields, NF(run_fields), &v, sizeof v, 1);
    section_end(&w, s);
    s = section_start(&w,"encounter");
    sv_encounter encounter={run->double_battle};
    put_block(&w,encounter_fields,NF(encounter_fields),&encounter,sizeof encounter,1);
    section_end(&w,s);
    s=section_start(&w,"illusions");
    put_block(&w,illusion_fields,NF(illusion_fields),&run->party[0].b.illusion,sizeof(gc_mon),run->n_party);
    put_block(&w,illusion_fields,NF(illusion_fields),&run->enemy[0].b.illusion,sizeof(gc_mon),run->n_enemy);
    section_end(&w,s);
    s = section_start(&w, "depths");
    put_block(&w, depth_fields, NF(depth_fields), &run->depths, sizeof run->depths, 1);
    section_end(&w, s);
    put_mons(&w, "party", run->party, run->n_party);
    s = section_start(&w, "mystery_history");
    put_block(&w, mystery_history_fields, NF(mystery_history_fields), run->mystery_history,
              sizeof run->mystery_history[0], run->n_mystery_history);
    section_end(&w, s);
    put_mons(&w, "enemy", run->enemy, run->n_enemy);
    put_items(&w, "items", run->items, run->n_items);
    put_items(&w, "enemy_items", run->enemy_items, run->n_enemy_items);
    s = section_start(&w, "arena");
    put_block(&w, arena_tag_fields, NF(arena_tag_fields), bt->arena_tags, sizeof bt->arena_tags[0], bt->n_arena_tags);
    put_block(&w, pos_tag_fields, NF(pos_tag_fields), bt->pos_tags, sizeof bt->pos_tags[0], bt->n_pos_tags);
    section_end(&w, s);
    /* Optional named section keeps effect records separate and avoids writing
     * struct padding or ABI-dependent offsets to a PC/3DS save. */
    s = section_start(&w, "pending-heals");
    put16(&w, bt->n_arena_tags);
    for (int i = 0; i < bt->n_arena_tags; i++) {
        const pr_arena_tag *tag = &bt->arena_tags[i];
        if (tag->n_pending_heals > PR_MAX_PENDING_HEALS
            || (tag->n_pending_heals && tag->type != ARENA_TAG_TYPE_PENDING_HEAL)) { w.full = true; break; }
        put_block(&w, pending_heal_fields, NF(pending_heal_fields), tag->pending_heals,
                  sizeof tag->pending_heals[0], tag->n_pending_heals);
    }
    section_end(&w, s);
    s = section_start(&w, "stats");
    put_block(&w, stat_fields, NF(stat_fields), run, sizeof *run, 1);
    section_end(&w, s);
    if (run->terminal_result) {
        sv_terminal terminal={run->terminal_result};
        s=section_start(&w,"terminal");put_block(&w,terminal_fields,NF(terminal_fields),&terminal,sizeof terminal,1);section_end(&w,s);
    }
    if (w.full) { memset(buf, 0, 8); return 0; }   /* nothing that looks like a save is left behind */
    const uint32_t len = (uint32_t)(w.n - body), crc = crc32(w.p + body, len);
    memcpy(w.p + 12, &len, 4);
    memcpy(w.p + 16, &crc, 4);
    return w.n;
}

/* A Pokemon as a load starts it: battle-core's defaults, so a field the save can't fill reads as unset */
static void mon_defaults(gc_mon *m)
{
    memset(m, 0, sizeof *m);
    pr_pokemon *b = &m->b;
    b->ability_override = -1;
    b->custom_passive = -1;
    b->custom_ability=b->fusion_custom_ability=b->lookup_ability_override=b->daily_boss_ability=-1;
    b->ability_index=-1;
    b->status_toxic_turns = 0;
    b->status_sleep_turns = b->status_freeze_turns = -1;
    b->tera_type = -1;
    b->fusion_ability_index = -1;
    for (int i = 0; i < 4; i++) b->max_pp_override[i] = -1;
    pr_reset_summon_data(b);
    pr_reset_turn_data(b);
    m->met_wave = -1;
    m->met_biome = -1;
}

static bool berry_history_ok(const pr_pokemon *b)
{
    if (b->n_battle_berries > PR_MAX_BERRY_HISTORY || b->n_turn_berries > PR_MAX_BERRY_HISTORY || b->n_last_berries > PR_MAX_BERRY_HISTORY) return false;
    for (int i = 0; i < b->n_battle_berries; i++) if (b->battle_berries[i] > BERRY_TYPE_LEPPA) return false;
    for (int i = 0; i < b->n_turn_berries; i++) if (b->turn_berries[i] > BERRY_TYPE_LEPPA) return false;
    for (int i = 0; i < b->n_last_berries; i++) if (b->last_berries[i] > BERRY_TYPE_LEPPA) return false;
    return true;
}

/* A Pokemon that can't be played with (a species or move this build lacks, counts out of range) */
static bool mon_ok(const gc_mon *m)
{
    const pr_pokemon *b = &m->b;
    const pr_species *sp = gc_species(b->species);
    if (!sp || b->form >= sp->nforms || b->level < 1 || m->met_level < 0) return false;
    if (m->has_custom_nature && (m->base_nature > 24 || b->nature > 24)) return false;
    if (b->fusion_species != 0) {
        const pr_species *fsp = gc_species(b->fusion_species);
        if (!fsp || b->fusion_form >= fsp->nforms) return false;
        if (b->fusion_ability_index < -1 || b->fusion_ability_index > 2) return false;
    }
    if (b->ability_lookup_known && (b->ability_index < 0 || b->ability_index > 2
        || b->custom_ability < -1 || b->custom_ability >= PR_ABILITY_COUNT
        || b->fusion_custom_ability < -1 || b->fusion_custom_ability >= PR_ABILITY_COUNT
        || b->lookup_ability_override < -1 || b->lookup_ability_override >= PR_ABILITY_COUNT
        || b->daily_boss_ability < -1 || b->daily_boss_ability >= PR_ABILITY_COUNT)) return false;
    if (b->temporary_fusion_species) {
        const pr_species *sp=gc_species(b->temporary_fusion_species);
        if (!b->fusion_species || !sp || b->temporary_fusion_form>=sp->nforms) return false;
    } else if (b->temporary_fusion_form) return false;
    if (b->n_moves > 4 || b->n_tags > PR_MAX_BATTLER_TAGS || b->n_move_queue > PR_MAX_MOVE_QUEUE) return false;
    for (int i = 0; i < b->n_moves; i++) if (b->moves[i] >= PR_MOVE_COUNT || b->pp_up[i] > 3 || (b->pp_up[i] < 0 && b->pp_up[i] != -4)) return false;
    if (b->n_summon_types > PR_MAX_SUMMON_TYPES || b->n_ab_applied_summon > 4 || b->n_ab_applied_wave > 4) return false;
    if (!berry_history_ok(b)) return false;
    uint8_t active_byte;memcpy(&active_byte,&b->summon_moves_active,1);
    if(active_byte>1 || b->summon_moves_alias_mask>15) return false;
    if(!PR_HAS_SUMMON_MOVES(b) && b->summon_moves_alias_mask) return false;
    if (b->transformed) {
        const pr_species *target = gc_species(b->transform_species);
        if (!target || b->transform_form >= target->nforms || b->transform_gender < -1
            || b->transform_gender > 1) return false;
    }
    if (PR_HAS_SUMMON_MOVES(b)) {
        if(b->transform_n_moves>4)return false;
        for (int i = 0; i < 4; i++) if (b->transform_moves[i] >= PR_MOVE_COUNT || b->transform_pp_up[i] > 3 || (b->transform_pp_up[i] < 0 && b->transform_pp_up[i] != -4)
            || b->transform_max_pp_override[i] < -1) return false;
    }
    for(int i=0;i<4;i++)if(b->summon_moves_alias_mask & (1u<<i)) {
        if(i>=b->n_moves || i>=b->transform_n_moves || b->moves[i]!=b->transform_moves[i]
            || b->pp_used[i]!=b->transform_pp_used[i] || b->pp_up[i]!=b->transform_pp_up[i]
            || b->max_pp_override[i]!=b->transform_max_pp_override[i])return false;
    }
    return true;
}

static bool illusion_schema_ok(reader r){
    int n=take16(&r);unsigned seen=0;if(r.bad||n!=NF(illusion_fields))return false;
    for(int i=0;i<n;i++){
        uint32_t name=take32(&r);uint16_t size=take16(&r);int found=-1;
        for(int k=0;k<NF(illusion_fields);k++)if(name==hash(illusion_fields[k].name)&&size==illusion_fields[k].size)found=k;
        if(found<0||(seen&(1u<<found)))return false;seen|=1u<<found;
    }
    return !r.bad;
}

static bool illusion_ok(const pr_illusion *v){
    uint8_t active;memcpy(&active,&v->active,1);if(active>1)return false;
    if(!active)return true;
    const pr_species *sp=gc_species(v->species);
    if(!sp||v->form>=sp->nforms||v->gender < -1||v->gender>1)return false;
    const pr_species *fusion=v->fusion_species?gc_species(v->fusion_species):NULL;
    return v->fusion_species?(fusion&&v->fusion_form<fusion->nforms):v->fusion_form==0;
}

/* Optional new section: legacy saves reconstruct known membership from their tail.
 * Missing old entries remain explicitly unknown rather than inventing a complete set. */
static bool get_history_used(const reader *body,const char *name,gc_mon *mons,int n,int *dropped)
{
    char key[48];snprintf(key,sizeof key,"%s_move_uses",name);
    reader scan=*body;int sections=0;
    while(scan.pos+8<=scan.n){uint32_t kind=take32(&scan),len=take32(&scan);if(len>scan.n-scan.pos)return false;if(kind==hash(key))sections++;scan.pos+=len;}
    if(scan.bad||scan.pos!=scan.n||sections>1)return false;
    if(!sections){for(int i=0;i<n;i++)pr_rebuild_history_used_moves(&mons[i].b);return true;}
    reader r=section(body,key),schema=r;int count=take16(&schema);unsigned seen=0;
    if(schema.bad||count!=NF(history_used_fields))return false;
    for(int i=0;i<count;i++){
        uint32_t field=take32(&schema);uint16_t size=take16(&schema);int found=-1;
        for(int j=0;j<NF(history_used_fields);j++)if(field==hash(history_used_fields[j].name)&&size==history_used_fields[j].size)found=j;
        if(found<0||(seen&(1u<<found)))return false;
        seen|=1u<<found;
    }
    if(schema.bad||get_block(&r,history_used_fields,NF(history_used_fields),&mons[0].b,sizeof(gc_mon),n,dropped)!=n||r.bad||r.pos!=r.n)return false;
    for(int i=0;i<n;i++){
        pr_pokemon *p=&mons[i].b;uint8_t complete;memcpy(&complete,&p->history_used_complete,1);
        if(complete>1)return false;
        for(int m=PR_MOVE_COUNT;m<(int)sizeof p->history_used_moves*8;m++)if(p->history_used_moves[m/8]&(1u<<(m%8)))return false;
    }
    return true;
}

/* Optional semantic query state. Old saves retain the exact answers visible in
 * their tail; a missing older prefix remains unknown until a new matching move. */
static bool get_nonvirtual_history(const reader *body,const char *name,gc_mon *mons,int n,int *dropped)
{
    char key[48];snprintf(key,sizeof key,"%s_last_moves",name);
    reader scan=*body;int sections=0;
    while(scan.pos+8<=scan.n){uint32_t kind=take32(&scan),len=take32(&scan);if(len>scan.n-scan.pos)return false;if(kind==hash(key))sections++;scan.pos+=len;}
    if(scan.bad||scan.pos!=scan.n||sections>1)return false;
    if(!sections){for(int i=0;i<n;i++)pr_rebuild_nonvirtual_history(&mons[i].b);return true;}
    reader r=section(body,key),schema=r;int count=take16(&schema);unsigned seen=0;
    if(schema.bad||count!=NF(history_last_fields))return false;
    for(int i=0;i<count;i++){
        uint32_t field=take32(&schema);uint16_t size=take16(&schema);int found=-1;
        for(int j=0;j<NF(history_last_fields);j++)if(field==hash(history_last_fields[j].name)&&size==history_last_fields[j].size)found=j;
        if(found<0||(seen&(1u<<found)))return false;
        seen|=1u<<found;
    }
    if(schema.bad||get_block(&r,history_last_fields,NF(history_last_fields),&mons[0].b,sizeof(gc_mon),n,dropped)!=n)return false;
    for(int i=0;i<n;i++){
        pr_pokemon *p=&mons[i].b;
        if(p->history_last_known>15||(p->history_last_present&~p->history_last_known))return false;
        /* Every cached TurnMove uses the same named schema as normal history. */
        reader entries=r;int fields=take16(&entries);unsigned fields_seen=0;
        if(fields!=NF(turn_move_fields))return false;
        for(int k=0;k<fields;k++){
            uint32_t field=take32(&entries);uint16_t size=take16(&entries);int found=-1;
            for(int j=0;j<NF(turn_move_fields);j++)if(field==hash(turn_move_fields[j].name)&&size==turn_move_fields[j].size)found=j;
            if(found<0||(fields_seen&(1u<<found)))return false;
            fields_seen|=1u<<found;
        }
        if(entries.bad||get_block(&r,turn_move_fields,NF(turn_move_fields),p->history_last,sizeof(pr_turn_move),4,dropped)!=4)return false;
        for(int q=0;q<4;q++)if(p->history_last_present&(1u<<q)){
            const pr_turn_move *m=&p->history_last[q];
            if(!m->move||m->move>=PR_MOVE_COUNT||m->use_mode<1||m->use_mode>6||m->n_targets>4
               ||(m->result>MOVE_RESULT_OTHER&&m->result!=PR_MOVE_RESULT_UNDEFINED)
               ||(q/2&&m->move==MOVE_ID_STRUGGLE)
               ||!(m->use_mode<MOVE_USE_MODE_INDIRECT||(!(q%2)&&m->use_mode==MOVE_USE_MODE_FOLLOW_UP)))return false;
            for(int k=0;k<m->n_targets;k++)if(m->targets[k]<0||m->targets[k]>3)return false;
        }
    }
    return !r.bad&&r.pos==r.n;
}

static bool get_all_history(const reader *body,const char *name,gc_mon *mons,int n,int *dropped)
{
    char key[48];snprintf(key,sizeof key,"%s_all_moves",name);
    reader scan=*body;int sections=0;
    while(scan.pos+8<=scan.n){uint32_t kind=take32(&scan),len=take32(&scan);if(len>scan.n-scan.pos)return false;if(kind==hash(key))sections++;scan.pos+=len;}
    if(scan.bad||scan.pos!=scan.n||sections>1)return false;
    if(!sections){for(int i=0;i<n;i++)pr_rebuild_all_history(&mons[i].b);return true;}
    reader r=section(body,key),schema=r;int count=take16(&schema);unsigned seen=0;
    if(schema.bad||count!=NF(history_all_fields))return false;
    for(int i=0;i<count;i++){
        uint32_t field=take32(&schema);uint16_t size=take16(&schema);int found=-1;
        for(int j=0;j<NF(history_all_fields);j++)if(field==hash(history_all_fields[j].name)&&size==history_all_fields[j].size)found=j;
        if(found<0||(seen&(1u<<found)))return false;
        seen|=1u<<found;
    }
    if(schema.bad||get_block(&r,history_all_fields,NF(history_all_fields),&mons[0].b,sizeof(gc_mon),n,dropped)!=n)return false;
    for(int i=0;i<n;i++){
        pr_pokemon *p=&mons[i].b;
        if(p->history_all_known>3||(p->history_all_present&~p->history_all_known)||((p->history_all_present&2)&&!(p->history_all_present&1)))return false;
        /* Every cached TurnMove uses the same named schema as normal history. */
        reader entries=r;int fields=take16(&entries);unsigned fields_seen=0;
        if(fields!=NF(turn_move_fields))return false;
        for(int k=0;k<fields;k++){
            uint32_t field=take32(&entries);uint16_t size=take16(&entries);int found=-1;
            for(int j=0;j<NF(turn_move_fields);j++)if(field==hash(turn_move_fields[j].name)&&size==turn_move_fields[j].size)found=j;
            if(found<0||(fields_seen&(1u<<found)))return false;
            fields_seen|=1u<<found;
        }
        if(entries.bad||get_block(&r,turn_move_fields,NF(turn_move_fields),&p->history_aim,sizeof(pr_turn_move),1,dropped)!=1)return false;
        if(p->history_all_present&2){
            const pr_turn_move *m=&p->history_aim;
            if((m->move!=MOVE_ID_LOCK_ON&&m->move!=MOVE_ID_MIND_READER)
               ||m->use_mode<1||m->use_mode>6||m->n_targets>4
               ||(m->result>MOVE_RESULT_OTHER&&m->result!=PR_MOVE_RESULT_UNDEFINED)
               ||!p->history_aim_serial||p->history_aim_serial>p->history_serial)return false;
            for(int k=0;k<m->n_targets;k++)if(m->targets[k]<0||m->targets[k]>3)return false;
        }
    }
    return !r.bad&&r.pos==r.n;
}

static bool get_protect_history(const reader *body,const char *name,gc_mon *mons,int n,int *dropped)
{
    char key[48];snprintf(key,sizeof key,"%s_protect",name);
    reader scan=*body;int sections=0;
    while(scan.pos+8<=scan.n){uint32_t kind=take32(&scan),len=take32(&scan);if(len>scan.n-scan.pos)return false;if(kind==hash(key))sections++;scan.pos+=len;}
    if(scan.bad||scan.pos!=scan.n||sections>1)return false;
    if(!sections){for(int i=0;i<n;i++)pr_rebuild_protect_history(&mons[i].b);return true;}
    reader r=section(body,key),schema=r;int count=take16(&schema);unsigned seen=0;
    if(schema.bad||count!=NF(protect_history_fields))return false;
    for(int i=0;i<count;i++){
        uint32_t field=take32(&schema);uint16_t size=take16(&schema);int found=-1;
        for(int j=0;j<NF(protect_history_fields);j++)if(field==hash(protect_history_fields[j].name)&&size==protect_history_fields[j].size)found=j;
        if(found<0||(seen&(1u<<found)))return false;
        seen|=1u<<found;
    }
    if(schema.bad||get_block(&r,protect_history_fields,NF(protect_history_fields),&mons[0].b,sizeof(gc_mon),n,dropped)!=n||r.bad||r.pos!=r.n)return false;
    for(int i=0;i<n;i++){uint8_t complete;memcpy(&complete,&mons[i].b.history_protect_complete,1);if(complete>1)return false;}
    return true;
}

static bool get_mons(const reader *body, const char *name, gc_mon *mons, int n, int *dropped)
{
    reader r = section(body, name);
    for (int i = 0; i < 6; i++) mon_defaults(&mons[i]);
    if (r.bad) return false;
    if (get_block(&r, mon_fields, NF(mon_fields), mons, sizeof(gc_mon), n, dropped) != n) return false;
    const bool transform_schema = has_transform_schema(r);
    if (!berry_schema_ok(r) || !ability_schema_ok(r) || !moveset_schema_ok(r)) return false;
    if (get_block(&r, pokemon_fields, NF(pokemon_fields), &mons[0].b, sizeof(gc_mon), n, dropped) != n) return false;
    for (int i = 0; i < n; i++) {
        pr_pokemon *b = &mons[i].b;
        b->n_tags = (uint8_t)get_block(&r, tag_fields, NF(tag_fields), b->tags, sizeof b->tags[0], PR_MAX_BATTLER_TAGS, dropped);
        for (int j = 0; j < b->n_tags; j++)
            if (b->tags[j].commanded_form_key > PR_FORM_COUNT || !isfinite(b->tags[j].hp)
                || b->tags[j].hp != floor(b->tags[j].hp)) return false;
        const int nh = get_block(&r, turn_move_fields, NF(turn_move_fields), b->move_history, sizeof b->move_history[0],
                                 PR_MAX_MOVE_HISTORY, dropped);
        if (b->n_move_history < nh) b->n_move_history = (uint16_t)nh;
        b->n_move_queue = (uint8_t)get_block(&r, turn_move_fields, NF(turn_move_fields), b->move_queue,
                                             sizeof b->move_queue[0], PR_MAX_MOVE_QUEUE, dropped);
        if (!berry_history_ok(b)) return false;
        pr_reset_turn_data(b);
        if (b->transformed && !transform_schema) return false;
        uint8_t active_byte;memcpy(&active_byte,&b->summon_moves_active,1);if(active_byte>1)return false;
        b->variant = mons[i].variant; /* also restores this bridge for older saves */
        if (!b->transformed) {
            if (b->transform_species || b->transform_form || b->transform_gender) return false;
            if (!b->summon_moves_active) {
                if (b->transform_n_moves) return false;
                pr_restore_transform(b);
            }
        }
        if (r.bad || !mon_ok(&mons[i])) return false;
    }
    return !r.bad&&get_history_used(body,name,mons,n,dropped)&&get_nonvirtual_history(body,name,mons,n,dropped)&&get_protect_history(body,name,mons,n,dropped)&&get_all_history(body,name,mons,n,dropped);
}

static bool get_items(const reader *body, const char *name, gc_item *items, int n, int *dropped)
{
    static sv_item sv[GC_MAX_ITEMS];
    memset(sv, 0, sizeof sv);
    reader r = section(body, name);
    if (r.bad) return n == 0;
    /* Optional named fields are independent: older saves imply factory
     * Eviolite and active form items. Missing markers are supported schemas,
     * not lost state; present markers require exact widths and uniqueness. */
    reader schema=r;int fields=take16(&schema);bool class_marker=false, form_marker=false;
    if(schema.bad||fields>128)return false;
    for(int k=0;k<fields;k++){
        uint32_t name_hash=take32(&schema);uint16_t size=take16(&schema);
        if(name_hash==hash("eviolite_base")){
            if(class_marker||size!=sizeof sv[0].eviolite_base)return false;
            class_marker=true;
        }
        if(name_hash==hash("form_inactive")){
            if(form_marker||size!=sizeof sv[0].form_inactive)return false;
            form_marker=true;
        }
    }
    sfield expected[NF(item_fields)];int required=0;
    for(int k=0;k<NF(item_fields);k++){
        if(!strcmp(item_fields[k].name,"eviolite_base")&&!class_marker)continue;
        if(!strcmp(item_fields[k].name,"form_inactive")&&!form_marker)continue;
        expected[required++]=item_fields[k];
    }
    if(schema.bad||get_block(&r, expected, required, sv, sizeof sv[0], n, dropped) != n)return false;
    for (int i = 0; i < n; i++) {
        sv[i].key[sizeof sv[i].key - 1] = 0;
        items[i].key = intern(sv[i].key);
        if (!items[i].key) return false;
        items[i].arg = sv[i].arg;
        items[i].stack = sv[i].stack;
        items[i].pokemon_id = sv[i].pokemon_id;
        items[i].detached = sv[i].detached;
        items[i].battle_count = sv[i].battle_count;
        uint8_t lock;
        memcpy(&lock, &sv[i].non_transferable, sizeof lock);
        if (lock > 1) return false;
        items[i].non_transferable = lock != 0 || gc_item_key_locked(items[i].key);
        uint8_t base_class;memcpy(&base_class,&sv[i].eviolite_base,sizeof base_class);
        if(base_class>1||(base_class&&strcmp(items[i].key,"EVIOLITE")))return false;
        items[i].eviolite_base=base_class!=0;
        uint8_t inactive;memcpy(&inactive,&sv[i].form_inactive,sizeof inactive);
        if(inactive>1||(inactive&&(!gc_item_key_form_change(items[i].key)||!pr_form_item_known(items[i].arg))))return false;
        items[i].form_inactive=inactive!=0;
        /* ModifierData.toModifier reconstructs typeFunc(): cached tier is not saved. */
        items[i].type_tier = 0;
        items[i].type_ref = 0;
        items[i].loot_ref = 0;
        items[i].virtual_stack = 0; /* pinned ModifierData saves real stacks only */
    }
    return true;
}

bool gc_run_load(gc_run *run, const void *buf, size_t len, const char **why)
{
    const char *dummy;
    if (!why) why = &dummy;
    const uint8_t *p = buf;
    if (!little_endian()) { *why = "saves need a little-endian host"; return false; }
    if (!buf || len < 20 || memcmp(p, magic, 8)) { *why = "not a run save"; return false; }
    uint16_t format, api;
    uint32_t body_len, crc;
    memcpy(&format, p + 8, 2);
    memcpy(&api, p + 10, 2);
    memcpy(&body_len, p + 12, 4);
    memcpy(&crc, p + 16, 4);
    if (format > SAVE_FORMAT) { *why = "the save was made by a newer version of the game"; return false; }
    if (body_len < 8 || body_len > len - 20 || crc32(p + 20, body_len) != crc) { *why = "the save is damaged"; return false; }
    reader body = {p + 20, body_len, 0, false};

    sv_run v;
    memset(&v, 0, sizeof v);
    v.trainer_name = v.trainer_partner_name = -1;   /* a save without them: no names rolled */
    v.last_battle_type = -1;
    v.mystery_type = v.mystery_tier = -1;
    int dropped = 0;
    reader r = section(&body, "run");
    /* Historical runs could not use Tera: an absent counter is exactly zero,
     * not lost saved state. A present field must have its unique native shape. */
    reader counter_schema = r;
    int run_nf = take16(&counter_schema), counter_fields = 0;
    if (counter_schema.bad || run_nf > 128) { *why = "the save is damaged"; return false; }
    for (int i = 0; i < run_nf; i++) {
        uint32_t name = take32(&counter_schema); uint16_t size = take16(&counter_schema);
        if (name == hash("player_teras_used") && (++counter_fields > 1 || size != sizeof(double))) {
            *why = "the stored Tera counter is damaged"; return false;
        }
    }
    if (counter_schema.bad) { *why = "the save is damaged"; return false; }
    if (r.bad || get_block(&r, run_fields, NF(run_fields) - !counter_fields, &v, sizeof v, 1, &dropped) != 1) { *why = "the save is damaged"; return false; }
    uint8_t frozen_marker; memcpy(&frozen_marker, &v.frozen_names, 1);
    if (frozen_marker > 1) { *why = "the stored names are damaged"; return false; }
    sv_terminal terminal={0};reader tr=section(&body,"terminal");
    if (!tr.bad) {
        reader scan=body;scan.pos=0;int terminal_sections=0;
        while(scan.pos+8<=scan.n){uint32_t sh=take32(&scan),len=take32(&scan);if(sh==hash("terminal"))terminal_sections++;if(len>scan.n-scan.pos){scan.bad=true;break;}scan.pos+=len;}
        if(scan.bad||scan.pos!=scan.n||terminal_sections!=1||tr.n!=11){*why="the terminal save is damaged";return false;}
    }
    if (!tr.bad && (get_block(&tr,terminal_fields,NF(terminal_fields),&terminal,sizeof terminal,1,&dropped)!=1
        || tr.bad || tr.pos!=tr.n || terminal.terminal_result<1 || terminal.terminal_result>2)) { *why="the terminal save is damaged";return false; }
    if (terminal.terminal_result && (v.in_battle || v.mode!=GAME_MODES_CLASSIC || (terminal.terminal_result==2&&(v.wave<200||v.has_challenges))||(terminal.terminal_result==1&&v.wave>200))) { *why="the terminal save is damaged";return false; }
    if ((!v.in_battle && !terminal.terminal_result) || v.wave < 1 || v.n_party < 1 || v.n_party > 6 || v.n_enemy < 1 || v.n_enemy > 6
        || v.n_items < 0 || v.n_items > PR_MAX_MODIFIERS || v.n_enemy_items < 0 || v.n_enemy_items > PR_MAX_MODIFIERS
        || v.n_mystery_history > GC_MAX_MYSTERY_HISTORY
        || v.seed_len > 64 || v.wave_seed_len > 64 || v.n_arena_tags < 0 || v.n_arena_tags > PR_MAX_ARENA_TAGS
        || v.n_pos_tags < 0 || v.n_pos_tags > PR_MAX_POSITIONAL_TAGS || v.biome < 0 || v.biome >= PR_BIOME_ID_LIMIT) {
        *why = "the save is damaged";
        return false;
    }

    if (!isfinite(v.player_teras_used) || v.player_teras_used < 0
        || v.player_teras_used != floor(v.player_teras_used) || v.player_teras_used > 9007199254740992.0) {
        *why = "the stored Tera counter is damaged"; return false;
    }

    bool seen[6] = {false};
    for (int i = 0; i < v.n_party; i++) {
        if (v.party_order[i] < 0 || v.party_order[i] >= v.n_party || seen[v.party_order[i]]) { *why = "the save is damaged"; return false; }
        seen[v.party_order[i]] = true;
    }

    memset(run, 0, sizeof *run);
    run->api_version = GC_API_VERSION;
    gc_internal *in = gc_int(run);
    gc_rq_reset(&in->rq);
    run->run.mode = v.mode;
    run->run.has_challenges = v.has_challenges;
    run->run.offset_gym = v.offset_gym;
    run->run.daily_custom = v.daily_custom;
    memcpy(run->run.seed.u, v.seed, sizeof v.seed);
    run->run.seed.len = v.seed_len;
    memcpy(run->run.wave_seed.u, v.wave_seed, sizeof v.wave_seed);
    run->run.wave_seed.len = v.wave_seed_len;
    run->run.rng = v.rng;
    run->wave = v.wave;
    run->biome = v.biome;
    run->wave_cycle_offset = v.wave_cycle_offset;
    run->money = v.money;
    run->score = v.score;
    run->trainer_id = v.trainer_id;
    run->secret_id = v.secret_id;
    memcpy(run->pokeballs, v.pokeballs, sizeof run->pokeballs);
    run->n_party = v.n_party;
    memcpy(run->party_order, v.party_order, sizeof run->party_order);
    run->n_items = v.n_items;
    run->n_enemy_items = v.n_enemy_items;
    run->me = v.me;
    run->n_mystery_history = v.n_mystery_history;
    run->mystery_type = v.mystery_type;
    run->mystery_tier = v.mystery_tier;
    run->in_battle = true;
    run->me_battle = v.me_battle;
    run->me_catch_allowed = v.me_catch_allowed;
    sv_encounter encounter={0};reader encounter_reader=section(&body,"encounter");
    if(!encounter_reader.bad){
        if(get_block(&encounter_reader,encounter_fields,NF(encounter_fields),&encounter,sizeof encounter,1,&dropped)!=1||encounter.double_battle>1){
            *why="the stored encounter width is damaged";return false;
        }
    }
    run->double_battle=encounter.double_battle!=0;
    run->battle_type = v.battle_type;
    run->trainer_type = v.trainer_type;
    run->trainer_variant = v.trainer_variant;
    run->trainer_name = (int16_t)v.trainer_name;
    run->trainer_partner_name = (int16_t)v.trainer_partner_name;
    memcpy(run->battle_seed, v.battle_seed, sizeof run->battle_seed);
    run->battle_seed[sizeof run->battle_seed - 1] = 0;
    run->n_enemy = v.n_enemy;
    in->last_battle_type = v.last_battle_type;
    in->last_final_boss = v.last_final_boss;
    in->lock_rarities = v.lock_rarities;
    in->last_max_exp_level = v.last_max_exp_level;
    in->stub_rng = v.stub_rng;

    r = section(&body, "mystery_history");
    if (!r.bad) {
        const int n = get_block(&r, mystery_history_fields, NF(mystery_history_fields),
                                run->mystery_history, sizeof run->mystery_history[0],
                                GC_MAX_MYSTERY_HISTORY, &dropped);
        if (n != v.n_mystery_history || r.bad) {
            *why = "the save is damaged";
            return false;
        }
        for (int i = 0; i < n; i++) {
            const gc_mystery_seen *h = &run->mystery_history[i];
            if (h->type >= GC_ME_COUNT || h->tier > GC_ME_ROGUE || h->wave > run->wave) {
                *why = "the save is damaged";
                return false;
            }
        }
    } else if (v.n_mystery_history) {
        *why = "the save is damaged";
        return false;
    }

    r = section(&body, "depths");
    if (r.bad || get_block(&r, depth_fields, NF(depth_fields), &run->depths, sizeof run->depths, 1, &dropped) != 1) {
        /* initBiomeDepths again from the run seed, as gc_run_new does */
        pr_run tmp = run->run;
        pr_run_reset_seed(&tmp, 0);
        pr_rng dr = tmp.rng;
        pr_run_init_biome_depths(&run->depths, &dr);
    }
    if (!get_mons(&body, "party", run->party, run->n_party, &dropped)
        || !get_mons(&body, "enemy", run->enemy, run->n_enemy, &dropped)
        || !get_items(&body, "items", run->items, run->n_items, &dropped)
        || !get_items(&body, "enemy_items", run->enemy_items, run->n_enemy_items, &dropped)) {
        memset(run, 0, sizeof *run);
        *why = "the save is damaged";
        return false;
    }

    if (format < 2) {
        for (int i = 0; i < run->n_party; i++) {
            run->party[i].n_used_tms = 0;
            memset(run->party[i].used_tms, 0, sizeof run->party[i].used_tms);
        }
        for (int i = 0; i < run->n_enemy; i++) {
            run->enemy[i].n_used_tms = 0;
            memset(run->enemy[i].used_tms, 0, sizeof run->enemy[i].used_tms);
        }
    }

    /* Older saves have no disguise section; retain their empty default without
     * counting an optional extension as dropped legacy data. */
    reader illusions=section(&body,"illusions");
    if(!illusions.bad){
        if(!illusion_schema_ok(illusions)||get_block(&illusions,illusion_fields,NF(illusion_fields),&run->party[0].b.illusion,sizeof(gc_mon),run->n_party,&dropped)!=run->n_party
            ||!illusion_schema_ok(illusions)||get_block(&illusions,illusion_fields,NF(illusion_fields),&run->enemy[0].b.illusion,sizeof(gc_mon),run->n_enemy,&dropped)!=run->n_enemy
            ||illusions.bad||illusions.pos!=illusions.n){*why="the stored disguises are damaged";return false;}
        for(int side=0;side<2;side++)for(int k=0;k<(side?run->n_enemy:run->n_party);k++)
            if(!illusion_ok(&(side?run->enemy:run->party)[k].b.illusion)){*why="the stored disguise is damaged";return false;}
    }

    /* the arena the battle keeps, then the battle as gc_battle_setup_field builds it with the lead
     * still out */
    pr_battle *bt = &run->bt;
    bt->weather = v.weather;
    bt->terrain = v.terrain;
    bt->weather_turns_left = v.weather_turns_left;
    bt->weather_max_duration = v.weather_max_duration;
    bt->terrain_turns_left = v.terrain_turns_left;
    bt->terrain_max_duration = v.terrain_max_duration;
    bt->player_faints = v.player_faints;
    bt->player_teras_used_known = true;
    bt->player_teras_used = v.player_teras_used;
    r = section(&body, "arena");
    if (!r.bad) {
        bt->n_arena_tags = (uint8_t)get_block(&r, arena_tag_fields, NF(arena_tag_fields), bt->arena_tags,
                                              sizeof bt->arena_tags[0], PR_MAX_ARENA_TAGS, &dropped);
        int names_lost = 0;
        bt->n_pos_tags = (uint8_t)get_block(&r, pos_tag_fields, v.frozen_names ? NF(pos_tag_fields) : NF(pos_tag_fields)-2, bt->pos_tags,
                                            sizeof bt->pos_tags[0], PR_MAX_POSITIONAL_TAGS, &names_lost);
        if (v.frozen_names && (r.bad || names_lost || bt->n_pos_tags != v.n_pos_tags)) {
            *why = "the stored wish effects are damaged"; return false;
        }
        dropped += names_lost;
        for (int i=0; i<bt->n_pos_tags; i++) {
            const pr_positional_tag *t=&bt->pos_tags[i];
            const pr_species *named=gc_species((uint16_t)(t->pokemon_name >> 16));
            const pr_species *fused=t->fusion_name ? gc_species((uint16_t)(t->fusion_name >> 16)) : NULL;
            if ((v.frozen_names && t->type==POSITIONAL_TAG_TYPE_WISH && !t->pokemon_name)
                || (t->pokemon_name && (!named || ((t->pokemon_name >> 8)&255)>=named->nforms || (t->pokemon_name&255)>3))
                || (t->fusion_name && (!fused || ((t->fusion_name >> 8)&255)>=fused->nforms || (t->fusion_name&255)))) {
                *why="the stored wish effects are damaged"; return false;
            }
        }
    } else if (v.n_arena_tags || v.n_pos_tags) {
        if (v.frozen_names) { *why="the stored arena effects are missing"; return false; }
        dropped++;
    }
    r = section(&body, "pending-heals");
    if (!r.bad) {
        int np = take16(&r), lost = 0;
        if (np != bt->n_arena_tags) { *why = "the stored healing effects are damaged"; return false; }
        for (int i = 0; i < np; i++) {
            pr_arena_tag *tag = &bt->arena_tags[i];
            int expected = tag->n_pending_heals;
            tag->n_pending_heals = (uint8_t)get_block(&r, pending_heal_fields, v.frozen_names ? NF(pending_heal_fields) : NF(pending_heal_fields)-1,
                tag->pending_heals, sizeof tag->pending_heals[0], PR_MAX_PENDING_HEALS, &lost);
            if (tag->n_pending_heals != expected) r.bad = true;
            if (tag->n_pending_heals && tag->type != ARENA_TAG_TYPE_PENDING_HEAL) r.bad = true;
            for (int k = 0; k < tag->n_pending_heals; k++) {
                const pr_pending_heal *h = &tag->pending_heals[k];
                const pr_species *named = gc_species((uint16_t)(h->pokemon_name >> 16));
                const pr_species *fused = h->fusion_name ? gc_species((uint16_t)(h->fusion_name >> 16)) : NULL;
                if (h->target_index >= 4 || h->restore_pp > 1 || h->move_id >= PR_MOVE_COUNT
                    || !pr_move_has_attr(h->move_id, PR_AC_SacrificialFullRestoreAttr)
                    || !named || ((h->pokemon_name >> 8) & 255) >= named->nforms
                    || (h->fusion_name && (!fused || ((h->fusion_name >> 8) & 255) >= fused->nforms || (h->fusion_name & 255)))
                    || (h->pokemon_name & 255) > (PR_MSG_POKEMON_ENEMY | PR_MSG_POKEMON_FOE)) r.bad = true;
                for (int j = 0; j < k; j++)
                    if (tag->pending_heals[j].target_index == h->target_index
                        && tag->pending_heals[j].move_id == h->move_id) r.bad = true;
            }
        }
        if (r.bad || lost || r.pos != r.n) { *why = "the stored healing effects are damaged"; return false; }
    } else {
        for (int i = 0; i < bt->n_arena_tags; i++)
            if (bt->arena_tags[i].n_pending_heals) { *why = "the stored healing effects are missing"; return false; }
    }
    if (terminal.terminal_result) {
        bool used[6]={0};
        for(int k=0;k<run->n_party;k++){
            int j=run->party_order[k];if(j<0||j>=run->n_party||used[j]){*why="the terminal party is damaged";return false;}used[j]=true;
            const pr_pokemon *p=&run->party[j].b;const pr_species *species=gc_species(p->species);
            if(!p->id||!species||p->form>=species->nforms){*why="the terminal party is damaged";return false;}
            for(int q=0;q<k;q++)if(run->party[run->party_order[q]].b.id==p->id){*why="the terminal party is damaged";return false;}
            if(p->fusion_species){const pr_species *fusion=gc_species(p->fusion_species);if(!fusion||p->fusion_form>=fusion->nforms){*why="the terminal party is damaged";return false;}}
        }
        run->in_battle=false;run->finished=true;run->terminal_result=terminal.terminal_result;
        memset(&run->screen,0,sizeof run->screen);
        run->screen.kind=terminal.terminal_result==2?GC_SCREEN_CLEAR:GC_SCREEN_GAME_OVER;
        in->rq.waiting=true;in->rq.current=gc_rp(RP_GAME_OVER,terminal.terminal_result==2,0,0);
        r=section(&body,"stats");if(!r.bad)get_block(&r,stat_fields,NF(stat_fields),run,sizeof *run,1,&dropped);
        run->load_dropped=dropped;gc_items_to_battle(run);return true;
    }
    gc_trainer_names(run);
    gc_event *ev = gc_ev_new(run, GC_EV_WAVE);
    if (ev) { ev->a = run->wave; ev->b = run->biome; ev->c = run->battle_type; }
    if (run->battle_type == BATTLE_TYPE_TRAINER) {
        ev = gc_ev_new(run, GC_EV_TRAINER);
        if (ev) { ev->a = run->trainer_type; ev->b = run->trainer_variant; }
        gc_ev_trainer_dialogue(run, 0);   /* EncounterPhase shows the trainer's line again on a loaded battle */
    }
    gc_battle_restore_field(run);
    bt->battle_rng = v.battle_rng;
    bt->battle_rng_live = v.battle_rng_live;
    for(int k=0;k<(run->double_battle?2:1)&&k<run->n_party;k++){
        int idx=run->party_order[k];
        if(run->party[idx].b.on_field&&run->party[idx].b.hp>0){
            ev=gc_ev_new(run,GC_EV_SUMMON);if(ev){ev->a=0;ev->b=idx;ev->c=k;}
        }
    }
    gc_ai_reset(run);
    gc_ai_enemy_party(run);
    /* the species and forms the enemy party began with (the save point is before the first turn: they are the current ones) */
    for (int i = 0; i < run->n_enemy && i < 6; i++) {
        in->enemy_start_species[i] = run->enemy[i].b.species;
        in->enemy_start_form[i] = run->enemy[i].b.form;
    }

    /* the counters last: setting the field up counts its stand-ins again */
    r = section(&body, "stats");
    if (!r.bad) get_block(&r, stat_fields, NF(stat_fields), run, sizeof *run, 1, &dropped);
    run->load_dropped = dropped;
    in->at_save_point = true;
    gc_ev_new(run, GC_EV_SAVE_POINT);
    gc_rq_push(run, gc_rp(RP_TURN_INIT, 0, 0, 0));
    gc_run_until_screen(run);
    return true;
}

/* Football checkpoints reuse named Pokemon/item fields, without relaxing the
 * classic battle save gate or reconstructing a Pokemon battle on load. */
typedef struct { int32_t wave,trainer,secret,n_party,n_items; pr_rng rng; uint16_t seed[64],wave_seed[64],seed_len,wave_seed_len; } sv_football;
static const sfield football_fields[]={SF(sv_football,wave),SF(sv_football,trainer),SF(sv_football,secret),
    SF(sv_football,n_party),SF(sv_football,n_items),SF(sv_football,rng),SF(sv_football,seed),SF(sv_football,wave_seed),SF(sv_football,seed_len),SF(sv_football,wave_seed_len)};
size_t gc_football_roster_save(const gc_run *run,void *buf,size_t cap)
{
    if (!run || !buf || cap<20 || !little_endian() || run->in_battle || run->n_party<1 || run->n_party>6 ||
        run->n_items<0 || run->n_items>GC_MAX_ITEMS || run->screen.kind!=GC_SCREEN_NONE) return 0;
    const gc_rqueue *q=&gc_int((gc_run *)run)->rq;
    if (q->waiting || q->current.type!=RP_NONE || run->ev_count || run->ev_overflow) return 0;
    for (int i=0;i<q->n_levels;++i) if (q->n[i]) return 0;
    sv_football v={0};v.wave=run->wave;v.trainer=run->trainer_id;v.secret=run->secret_id;
    v.n_party=run->n_party;v.n_items=run->n_items;v.rng=run->run.rng;
    memcpy(v.seed,run->run.seed.u,sizeof v.seed);memcpy(v.wave_seed,run->run.wave_seed.u,sizeof v.wave_seed);
    v.seed_len=(uint16_t)run->run.seed.len;v.wave_seed_len=(uint16_t)run->run.wave_seed.len;
    writer w={(uint8_t *)buf,0,cap,false};
    put(&w,"PRFBROST",8);put16(&w,1);put16(&w,GC_API_VERSION);put32(&w,0);put32(&w,0);
    size_t section=section_start(&w,"football");
    put_block(&w,football_fields,NF(football_fields),&v,sizeof v,1);section_end(&w,section);
    put_mons(&w,"party",run->party,run->n_party);put_items(&w,"items",run->items,run->n_items);
    if (w.full) return 0;
    uint32_t length=(uint32_t)(w.n-20),crc=crc32(w.p+20,length);
    memcpy(w.p+12,&length,4);memcpy(w.p+16,&crc,4);return w.n;
}
bool gc_football_roster_load(gc_run *run,gc_system *profile,const void *buf,size_t len)
{
    if (!run || !buf || !little_endian() || len<20 || len>GC_SAVE_MAX) return false;
    const uint8_t *p=buf;uint16_t version,api;uint32_t length,crc;
    memcpy(&version,p+8,2);memcpy(&api,p+10,2);memcpy(&length,p+12,4);memcpy(&crc,p+16,4);
    if (memcmp(p,"PRFBROST",8) || version!=1 || api!=GC_API_VERSION || length!=len-20 || crc32(p+20,length)!=crc) return false;
    reader body={p+20,length,0,false},r=section(&body,"football");sv_football v={0};int dropped=0;
    if (r.bad || get_block(&r,football_fields,NF(football_fields),&v,sizeof v,1,&dropped)!=1 ||
        r.bad || r.pos!=r.n || dropped || v.n_party<1 || v.n_party>6 || v.n_items<0 || v.n_items>GC_MAX_ITEMS ||
        v.seed_len>64 || v.wave_seed_len>64 || v.wave<0 || v.wave>140 || !isfinite(v.rng.c) || !isfinite(v.rng.s0) || !isfinite(v.rng.s1) ||
        !isfinite(v.rng.s2) || !isfinite(v.rng.n)) return false;
    gc_football_new(run,profile,"football-resume",1);
    run->wave=v.wave;run->trainer_id=v.trainer;run->secret_id=v.secret;run->run.rng=v.rng;
    memcpy(run->run.seed.u,v.seed,sizeof v.seed);memcpy(run->run.wave_seed.u,v.wave_seed,sizeof v.wave_seed);
    run->run.seed.len=v.seed_len;run->run.wave_seed.len=v.wave_seed_len;
    run->n_party=v.n_party;run->n_items=v.n_items;
    if (!get_mons(&body,"party",run->party,run->n_party,&dropped) ||
        !get_items(&body,"items",run->items,run->n_items,&dropped) || dropped) return false;
    for (int i=0;i<run->n_party;++i) {
        if (!run->party[i].b.id) return false;
        for (int j=0;j<i;++j) if (run->party[i].b.id==run->party[j].b.id) return false;
        run->party_order[i]=(int8_t)i;run->bt.party[0][i]=&run->party[i].b;
    }
    for (int i=0;i<run->n_items;++i) {
        const gc_item *it=&run->items[i];int slot=-1;
        for(int j=0;j<run->n_party;++j)if(it->pokemon_id==run->party[j].b.id)slot=j;
        if (slot<0 || it->stack!=1 || it->detached || strcmp(it->key,"FORM_CHANGE_ITEM") ||
            !gc_football_mega_form(run,(unsigned)slot,it->arg)) return false;
    }
    run->bt.n_party[0]=run->n_party;run->screen.kind=GC_SCREEN_NONE;gc_items_to_battle(run);return true;
}

/* ---------------------------------------------------------------- system save/load */

typedef struct { uint16_t species; gc_dex_entry entry; } sv_dex_entry;
static const sfield dex_fields[] = {
    SF(sv_dex_entry, species), SF(sv_dex_entry, entry.seen_attr), SF(sv_dex_entry, entry.caught_attr),
    SF(sv_dex_entry, entry.nature_attr), SF(sv_dex_entry, entry.seen_count), SF(sv_dex_entry, entry.caught_count),
    SF(sv_dex_entry, entry.hatched_count), SF(sv_dex_entry, entry.ivs), SF(sv_dex_entry, entry.ribbon_flags),
};

typedef struct { uint16_t species; gc_starter_entry entry; } sv_starter_entry;
static const sfield starter_fields[] = {
    SF(sv_starter_entry, species), SF(sv_starter_entry, entry.ability_attr), SF(sv_starter_entry, entry.passive_attr),
    SF(sv_starter_entry, entry.value_reduction), SF(sv_starter_entry, entry.candy_count), SF(sv_starter_entry, entry.friendship),
    SF(sv_starter_entry, entry.egg_moves), SF(sv_starter_entry, entry.moveset), SF(sv_starter_entry, entry.classic_win_count),
    SF(sv_starter_entry, entry.preference_flags), SF(sv_starter_entry, entry.nature_preference),
    SF(sv_starter_entry, entry.appearance_preference), SF(sv_starter_entry, entry.ability_preference),
};

static const sfield unlock_fields[] = {
    SF(gc_unlocks, endless_mode), SF(gc_unlocks, mini_black_hole),
    SF(gc_unlocks, spliced_endless_mode), SF(gc_unlocks, eviolite),
};

typedef struct { uint32_t voucher_counts[4]; uint8_t voucher_unlocks[512]; } sv_vouchers;
static const sfield voucher_fields[] = {
    SF(sv_vouchers, voucher_counts), SF(sv_vouchers, voucher_unlocks),
};

typedef struct { uint8_t flags[256]; bool classic_voucher_unlocked; } sv_achievements;
static const sfield achievement_fields[] = {
    SF(sv_achievements, flags), SF(sv_achievements, classic_voucher_unlocked),
};

static const sfield stat_milestone_fields[] = {
    SF(gc_stat_milestones, ribbons_owned), SF(gc_stat_milestones, classic_wins), SF(gc_stat_milestones, endless_wins),
    SF(gc_stat_milestones, total_battles), SF(gc_stat_milestones, total_catches),
    SF(gc_stat_milestones, total_hatches), SF(gc_stat_milestones, total_shinies),
    SF(gc_stat_milestones, highest_wave), SF(gc_stat_milestones, highest_damage),
    SF(gc_stat_milestones, total_money_earned),
    SF(gc_stat_milestones, eggs_pulled), SF(gc_stat_milestones, rare_eggs_pulled),
    SF(gc_stat_milestones, epic_eggs_pulled), SF(gc_stat_milestones, legendary_eggs_pulled), SF(gc_stat_milestones, manaphy_eggs_pulled),
};

static const sfield pity_fields[]={SF(gc_system, egg_pity),SF(gc_system, unlock_pity)};
static const sfield egg_fields[] = {
    SF(gc_egg, id), SF(gc_egg, hatch_waves), SF(gc_egg, tier), SF(gc_egg, species), SF(gc_egg, source_type), SF(gc_egg, variant_tier), SF(gc_egg, egg_move_index),
    SF(gc_egg, is_shiny), SF(gc_egg, override_hidden_ability), SF(gc_egg, timestamp_ms),
};

size_t gc_system_save(const gc_system *sys, void *buf, size_t cap)
{
    if (!sys || !little_endian() || cap < 24) return 0;
    writer w = {(uint8_t *)buf, 0, cap, false};
    put(&w, sys_magic, 8);
    put16(&w, SAVE_FORMAT);
    put16(&w, GC_API_VERSION);
    put32(&w, 0);
    put32(&w, 0);
    const size_t body = w.n;

    size_t s = section_start(&w, "unlocks");
    put_block(&w, unlock_fields, NF(unlock_fields), &sys->unlocks, sizeof sys->unlocks, 1);
    section_end(&w, s);

    sv_vouchers sv_v;
    memcpy(sv_v.voucher_counts, sys->voucher_counts, sizeof sys->voucher_counts);
    memcpy(sv_v.voucher_unlocks, sys->voucher_unlocks, sizeof sys->voucher_unlocks);
    s = section_start(&w, "vouchers");
    put_block(&w, voucher_fields, NF(voucher_fields), &sv_v, sizeof sv_v, 1);
    section_end(&w, s);

    sv_achievements sv_a;
    memcpy(sv_a.flags, sys->achievement_flags, sizeof sv_a.flags);
    sv_a.classic_voucher_unlocked=sys->classic_voucher_unlocked;
    s = section_start(&w, "achievements");
    put_block(&w, achievement_fields, NF(achievement_fields), &sv_a, sizeof sv_a, 1);
    section_end(&w, s);

    s = section_start(&w, "stats");
    put_block(&w, stat_milestone_fields, NF(stat_milestone_fields), &sys->stats, sizeof sys->stats, 1);
    section_end(&w, s);

    if (sys->n_eggs > 0) {
        s = section_start(&w, "eggs");
        put_block(&w, egg_fields, NF(egg_fields), sys->eggs, sizeof sys->eggs[0], sys->n_eggs);
        section_end(&w, s);
    }

    s=section_start(&w,"egg_pity");
    put_block(&w,pity_fields,NF(pity_fields),sys,sizeof *sys,1);
    section_end(&w,s);

    int n_dex = 0;
    for (int i = 1; i <= GC_MAX_SPECIES_ID; i++) {
        if (sys->dex[i].seen_attr || sys->dex[i].caught_attr || sys->dex[i].ribbon_flags) n_dex++;
    }
    if (n_dex > 0) {
        sv_dex_entry *dex_buf = malloc(sizeof(sv_dex_entry) * n_dex);
        if (dex_buf) {
            int k = 0;
            for (int i = 1; i <= GC_MAX_SPECIES_ID; i++) {
                if (sys->dex[i].seen_attr || sys->dex[i].caught_attr || sys->dex[i].ribbon_flags) {
                    dex_buf[k].species = (uint16_t)i;
                    dex_buf[k].entry = sys->dex[i];
                    k++;
                }
            }
            s = section_start(&w, "dex");
            put_block(&w, dex_fields, NF(dex_fields), dex_buf, sizeof(sv_dex_entry), n_dex);
            section_end(&w, s);
            free(dex_buf);
        }
    }

    int n_starters = 0;
    for (int i = 1; i <= GC_MAX_SPECIES_ID; i++) {
        const gc_starter_entry *st = &sys->starters[i];
        if (st->ability_attr || st->passive_attr || st->value_reduction || st->candy_count ||
            st->friendship || st->egg_moves || st->classic_win_count || st->preference_flags ||
            st->moveset[0] || st->moveset[1] || st->moveset[2] || st->moveset[3]) {
            n_starters++;
        }
    }
    if (n_starters > 0) {
        sv_starter_entry *st_buf = malloc(sizeof(sv_starter_entry) * n_starters);
        if (st_buf) {
            int k = 0;
            for (int i = 1; i <= GC_MAX_SPECIES_ID; i++) {
                const gc_starter_entry *st = &sys->starters[i];
                if (st->ability_attr || st->passive_attr || st->value_reduction || st->candy_count ||
                    st->friendship || st->egg_moves || st->classic_win_count || st->preference_flags ||
                    st->moveset[0] || st->moveset[1] || st->moveset[2] || st->moveset[3]) {
                    st_buf[k].species = (uint16_t)i;
                    st_buf[k].entry = *st;
                    k++;
                }
            }
            s = section_start(&w, "starters");
            put_block(&w, starter_fields, NF(starter_fields), st_buf, sizeof(sv_starter_entry), n_starters);
            section_end(&w, s);
            free(st_buf);
        }
    }

    if (w.full) { memset(buf, 0, 8); return 0; }
    const uint32_t len = (uint32_t)(w.n - body), crc = crc32(w.p + body, len);
    memcpy(w.p + 12, &len, 4);
    memcpy(w.p + 16, &crc, 4);
    return w.n;
}

bool gc_system_load(gc_system *sys, const void *buf, size_t len, const char **why)
{
    const char *dummy;
    if (!why) why = &dummy;
    const uint8_t *p = buf;
    if (!little_endian()) { *why = "saves need a little-endian host"; return false; }
    if (!buf || len < 20 || memcmp(p, sys_magic, 8)) { *why = "not a system save"; return false; }
    uint16_t format, api;
    uint32_t body_len, crc;
    memcpy(&format, p + 8, 2);
    memcpy(&api, p + 10, 2);
    memcpy(&body_len, p + 12, 4);
    memcpy(&crc, p + 16, 4);
    if (format > SAVE_FORMAT) { *why = "the save was made by a newer version of the game"; return false; }
    if (body_len < 8 || body_len > len - 20 || crc32(p + 20, body_len) != crc) { *why = "the save is damaged"; return false; }
    reader body = {p + 20, body_len, 0, false};

    gc_system_init_defaults(sys);
    int dropped = 0;

    reader r = section(&body, "unlocks");
    if (!r.bad) get_block(&r, unlock_fields, NF(unlock_fields), &sys->unlocks, sizeof sys->unlocks, 1, &dropped);

    r = section(&body, "vouchers");
    if (!r.bad) {
        sv_vouchers sv_v;
        memset(&sv_v, 0, sizeof sv_v);
        get_block(&r, voucher_fields, NF(voucher_fields), &sv_v, sizeof sv_v, 1, &dropped);
        memcpy(sys->voucher_counts, sv_v.voucher_counts, sizeof sys->voucher_counts);
        memcpy(sys->voucher_unlocks, sv_v.voucher_unlocks, sizeof sys->voucher_unlocks);
    }

    r = section(&body, "achievements");
    if (!r.bad) {
        sv_achievements sv_a;
        memset(&sv_a, 0, sizeof sv_a);
        get_block(&r, achievement_fields, NF(achievement_fields), &sv_a, sizeof sv_a, 1, &dropped);
        memcpy(sys->achievement_flags, sv_a.flags, sizeof sys->achievement_flags);
        sys->classic_voucher_unlocked=sv_a.classic_voucher_unlocked;
    }

    r = section(&body, "stats");
    if (!r.bad) {
        get_block(&r, stat_milestone_fields, NF(stat_milestone_fields), &sys->stats, sizeof sys->stats, 1, &dropped);
    }

    r = section(&body, "eggs");
    if (!r.bad) {
        sys->n_eggs = get_block(&r, egg_fields, NF(egg_fields), sys->eggs, sizeof sys->eggs[0], GC_MAX_EGGS, &dropped);
    }

    r=section(&body,"egg_pity");
    if(!r.bad)get_block(&r,pity_fields,NF(pity_fields),sys,sizeof *sys,1,&dropped);

    r = section(&body, "dex");
    if (!r.bad) {
        sv_dex_entry *dex_buf=calloc(GC_MAX_SPECIES_ID+1,sizeof *dex_buf);
        if(!dex_buf){*why="system save allocation failed";return false;}
        int count;
        while (true) {
            memset(dex_buf,0,(GC_MAX_SPECIES_ID+1)*sizeof *dex_buf);
            count=get_block(&r,dex_fields,NF(dex_fields),dex_buf,sizeof(sv_dex_entry),GC_MAX_SPECIES_ID+1,&dropped);if(count<=0)break;
            for (int i = 0; i < count; i++) {
                uint16_t sp = dex_buf[i].species;
                if (sp > 0 && sp <= GC_MAX_SPECIES_ID) {
                    sys->dex[sp] = dex_buf[i].entry;
                }
            }
        }
        free(dex_buf);
    }

    r = section(&body, "starters");
    if (!r.bad) {
        sv_starter_entry *st_buf=calloc(GC_MAX_SPECIES_ID+1,sizeof *st_buf);
        if(!st_buf){*why="system save allocation failed";return false;}
        int count;
        while (true) {
            memset(st_buf,0,(GC_MAX_SPECIES_ID+1)*sizeof *st_buf);
            count=get_block(&r,starter_fields,NF(starter_fields),st_buf,sizeof(sv_starter_entry),GC_MAX_SPECIES_ID+1,&dropped);if(count<=0)break;
            for (int i = 0; i < count; i++) {
                uint16_t sp = st_buf[i].species;
                if (sp > 0 && sp <= GC_MAX_SPECIES_ID) {
                    const gc_starter_entry *p=&st_buf[i].entry;
                    if (p->preference_flags>7 || p->nature_preference>24 || p->appearance_preference>3 || p->ability_preference>2) {
                        free(st_buf); *why="invalid starter preferences"; return false;
                    }
                    sys->starters[sp] = st_buf[i].entry;
                }
            }
        }
        free(st_buf);
    }

    return true;
}
