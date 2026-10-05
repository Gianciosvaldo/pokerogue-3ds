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
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAVE_FORMAT 1
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
} sv_run;
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
};

static const sfield depth_fields[] = {SF(pr_biome_depths, set), SF(pr_biome_depths, depth), SF(pr_biome_depths, chance)};
static const sfield mystery_history_fields[] = {
    SF(gc_mystery_seen, type), SF(gc_mystery_seen, tier), SF(gc_mystery_seen, wave),
};

/* An item, its key as text (gc_item.key is a pointer) */
typedef struct { char key[40]; int16_t arg; int32_t stack; uint32_t pokemon_id; } sv_item;
static const sfield item_fields[] = {SF(sv_item, key), SF(sv_item, arg), SF(sv_item, stack), SF(sv_item, pokemon_id)};

/* What the run keeps of a Pokemon beside battle-core's part */
static const sfield mon_fields[] = {
    SF(gc_mon, exp), SF(gc_mon, ability_index), SF(gc_mon, variant), SF(gc_mon, pokerus),
    SF(gc_mon, pause_evolutions), SF(gc_mon, luck), SF(gc_mon, pokeball), SF(gc_mon, met_wave),
    SF(gc_mon, trainer_slot), SF(gc_mon, met_level), SF(gc_mon, met_biome), SF(gc_mon, met_species),
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
};
static const sfield tag_fields[] = {
    SF(pr_battler_tag, type), SF(pr_battler_tag, turn_count), SF(pr_battler_tag, source_move),
    SF(pr_battler_tag, source_id), SF(pr_battler_tag, crit_stages), SF(pr_battler_tag, stockpiled_count),
    SF(pr_battler_tag, stock_def), SF(pr_battler_tag, stock_spdef), SF(pr_battler_tag, autotomize_count),
    SF(pr_battler_tag, faint_count), SF(pr_battler_tag, stat), SF(pr_battler_tag, multiplier), SF(pr_battler_tag, hp),
    SF(pr_battler_tag, in_focus), SF(pr_battler_tag, source_index), SF(pr_battler_tag, activated),
    SF(pr_battler_tag, base_pure_flying), SF(pr_battler_tag, move_id), SF(pr_battler_tag, added_this_turn),
};
static const sfield turn_move_fields[] = {
    SF(pr_turn_move, move), SF(pr_turn_move, use_mode), SF(pr_turn_move, result), SF(pr_turn_move, targets),
    SF(pr_turn_move, n_targets),
};
static const sfield arena_tag_fields[] = {
    SF(pr_arena_tag, type), SF(pr_arena_tag, side), SF(pr_arena_tag, turn_count), SF(pr_arena_tag, max_duration),
    SF(pr_arena_tag, source_move), SF(pr_arena_tag, source_id), SF(pr_arena_tag, source_count),
    SF(pr_arena_tag, layers), SF(pr_arena_tag, being_removed),
};
static const sfield pos_tag_fields[] = {
    SF(pr_positional_tag, type), SF(pr_positional_tag, turn_count), SF(pr_positional_tag, target_index),
    SF(pr_positional_tag, source_id), SF(pr_positional_tag, source_move), SF(pr_positional_tag, heal_hp),
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
    for (int i = 0; i < nf && i < MAX_SAVED; i++) {
        at[i] = -1;
        const uint32_t h = hash(f[i].name);
        size_t off = 0;
        for (int k = 0; k < n; k++) {
            if (sh[k] == h && ss[k] == f[i].size) { at[i] = (long)off; break; }
            off += ss[k];
        }
        if (at[i] < 0 && count > 0) (*dropped)++;
    }
    const int keep = count < max ? count : max;
    for (int k = 0; k < keep; k++) {
        const uint8_t *src = r->p + r->pos + rec * (size_t)k;
        uint8_t *dst = (uint8_t *)base + (size_t)k * stride;
        for (int i = 0; i < nf && i < MAX_SAVED; i++)
            if (at[i] >= 0) memcpy(dst + f[i].off, src + at[i], f[i].size);
    }
    if (count > max) (*dropped)++;
    r->pos += rec * (size_t)count;
    return keep;
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
    gc_internal *in = gc_int((gc_run *)run);
    if (!in->at_save_point || !run->in_battle || run->screen.kind != GC_SCREEN_COMMAND) return false;
    /* only the turn this COMMAND screen belongs to may be waiting: the load starts it again */
    const gc_rqueue *q = &in->rq;
    if (q->current.type != RP_COMMAND) return false;
    for (int l = 0; l < q->n_levels; l++)
        for (int k = 0; k < q->n[l]; k++) if (q->q[l][k].type != RP_TURN) return false;
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
    v.n_arena_tags = bt->n_arena_tags;
    v.n_pos_tags = bt->n_pos_tags;

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
    s = section_start(&w, "stats");
    put_block(&w, stat_fields, NF(stat_fields), run, sizeof *run, 1);
    section_end(&w, s);
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

/* A Pokemon that can't be played with (a species or move this build lacks, counts out of range) */
static bool mon_ok(const gc_mon *m)
{
    const pr_pokemon *b = &m->b;
    const pr_species *sp = gc_species(b->species);
    if (!sp || b->form >= sp->nforms || b->level < 1) return false;
    if (b->fusion_species != 0) {
        const pr_species *fsp = gc_species(b->fusion_species);
        if (!fsp || b->fusion_form >= fsp->nforms) return false;
        if (b->fusion_ability_index < -1 || b->fusion_ability_index > 2) return false;
    }
    if (b->n_moves > 4 || b->n_tags > PR_MAX_BATTLER_TAGS || b->n_move_queue > PR_MAX_MOVE_QUEUE) return false;
    for (int i = 0; i < b->n_moves; i++) if (b->moves[i] >= PR_MOVE_COUNT) return false;
    if (b->n_summon_types > PR_MAX_SUMMON_TYPES || b->n_ab_applied_summon > 4 || b->n_ab_applied_wave > 4) return false;
    return true;
}

static bool get_mons(const reader *body, const char *name, gc_mon *mons, int n, int *dropped)
{
    reader r = section(body, name);
    for (int i = 0; i < 6; i++) mon_defaults(&mons[i]);
    if (r.bad) return false;
    if (get_block(&r, mon_fields, NF(mon_fields), mons, sizeof(gc_mon), n, dropped) != n) return false;
    if (get_block(&r, pokemon_fields, NF(pokemon_fields), &mons[0].b, sizeof(gc_mon), n, dropped) != n) return false;
    for (int i = 0; i < n; i++) {
        pr_pokemon *b = &mons[i].b;
        b->n_tags = (uint8_t)get_block(&r, tag_fields, NF(tag_fields), b->tags, sizeof b->tags[0], PR_MAX_BATTLER_TAGS, dropped);
        const int nh = get_block(&r, turn_move_fields, NF(turn_move_fields), b->move_history, sizeof b->move_history[0],
                                 PR_MAX_MOVE_HISTORY, dropped);
        if (b->n_move_history < nh) b->n_move_history = (uint16_t)nh;
        b->n_move_queue = (uint8_t)get_block(&r, turn_move_fields, NF(turn_move_fields), b->move_queue,
                                             sizeof b->move_queue[0], PR_MAX_MOVE_QUEUE, dropped);
        pr_reset_turn_data(b);
        if (r.bad || !mon_ok(&mons[i])) return false;
    }
    return !r.bad;
}

static bool get_items(const reader *body, const char *name, gc_item *items, int n, int *dropped)
{
    static sv_item sv[GC_MAX_ITEMS];
    memset(sv, 0, sizeof sv);
    reader r = section(body, name);
    if (r.bad) return n == 0;
    if (get_block(&r, item_fields, NF(item_fields), sv, sizeof sv[0], n, dropped) != n) return false;
    for (int i = 0; i < n; i++) {
        sv[i].key[sizeof sv[i].key - 1] = 0;
        items[i].key = intern(sv[i].key);
        if (!items[i].key) return false;
        items[i].arg = sv[i].arg;
        items[i].stack = sv[i].stack;
        items[i].pokemon_id = sv[i].pokemon_id;
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
    if (r.bad || get_block(&r, run_fields, NF(run_fields), &v, sizeof v, 1, &dropped) != 1) { *why = "the save is damaged"; return false; }
    if (!v.in_battle || v.wave < 1 || v.n_party < 1 || v.n_party > 6 || v.n_enemy < 1 || v.n_enemy > 6
        || v.n_items < 0 || v.n_items > GC_MAX_ITEMS || v.n_enemy_items < 0 || v.n_enemy_items > GC_MAX_ITEMS
        || v.n_mystery_history > GC_MAX_MYSTERY_HISTORY
        || v.seed_len > 64 || v.wave_seed_len > 64 || v.n_arena_tags < 0 || v.n_arena_tags > PR_MAX_ARENA_TAGS
        || v.n_pos_tags < 0 || v.n_pos_tags > PR_MAX_POSITIONAL_TAGS || v.biome < 0 || v.biome >= PR_BIOME_ID_LIMIT) {
        *why = "the save is damaged";
        return false;
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
    r = section(&body, "arena");
    if (!r.bad) {
        bt->n_arena_tags = (uint8_t)get_block(&r, arena_tag_fields, NF(arena_tag_fields), bt->arena_tags,
                                              sizeof bt->arena_tags[0], PR_MAX_ARENA_TAGS, &dropped);
        bt->n_pos_tags = (uint8_t)get_block(&r, pos_tag_fields, NF(pos_tag_fields), bt->pos_tags,
                                            sizeof bt->pos_tags[0], PR_MAX_POSITIONAL_TAGS, &dropped);
    } else if (v.n_arena_tags || v.n_pos_tags) {
        dropped++;
    }
    gc_trainer_names(run);
    gc_event *ev = gc_ev_new(run, GC_EV_WAVE);
    if (ev) { ev->a = run->wave; ev->b = run->biome; ev->c = run->battle_type; }
    if (run->battle_type == BATTLE_TYPE_TRAINER) {
        ev = gc_ev_new(run, GC_EV_TRAINER);
        if (ev) { ev->a = run->trainer_type; ev->b = run->trainer_variant; }
    }
    const int lead = run->party_order[0];
    const bool lead_out = run->party[lead].b.on_field && run->party[lead].b.hp > 0;
    gc_battle_restore_field(run);
    bt->battle_rng = v.battle_rng;
    bt->battle_rng_live = v.battle_rng_live;
    if (lead_out) {
        ev = gc_ev_new(run, GC_EV_SUMMON);
        if (ev) { ev->a = 0; ev->b = lead; ev->c = 0; }
    }
    gc_ai_reset(run);
    gc_ai_enemy_party(run);

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

/* ---------------------------------------------------------------- system save/load */

typedef struct { uint16_t species; gc_dex_entry entry; } sv_dex_entry;
static const sfield dex_fields[] = {
    SF(sv_dex_entry, species), SF(sv_dex_entry, entry.seen_attr), SF(sv_dex_entry, entry.caught_attr),
    SF(sv_dex_entry, entry.nature_attr), SF(sv_dex_entry, entry.seen_count), SF(sv_dex_entry, entry.caught_count),
    SF(sv_dex_entry, entry.hatched_count), SF(sv_dex_entry, entry.ivs),
};

typedef struct { uint16_t species; gc_starter_entry entry; } sv_starter_entry;
static const sfield starter_fields[] = {
    SF(sv_starter_entry, species), SF(sv_starter_entry, entry.ability_attr), SF(sv_starter_entry, entry.passive_attr),
    SF(sv_starter_entry, entry.value_reduction), SF(sv_starter_entry, entry.candy_count), SF(sv_starter_entry, entry.friendship),
    SF(sv_starter_entry, entry.egg_moves), SF(sv_starter_entry, entry.moveset), SF(sv_starter_entry, entry.classic_win_count),
};

static const sfield unlock_fields[] = {
    SF(gc_unlocks, endless_mode), SF(gc_unlocks, mini_black_hole),
    SF(gc_unlocks, spliced_endless_mode), SF(gc_unlocks, eviolite),
};

typedef struct { uint32_t voucher_counts[4]; uint8_t voucher_unlocks[512]; } sv_vouchers;
static const sfield voucher_fields[] = {
    SF(sv_vouchers, voucher_counts), SF(sv_vouchers, voucher_unlocks),
};

typedef struct { uint8_t flags[256]; } sv_achievements;
static const sfield achievement_fields[] = {
    SF(sv_achievements, flags),
};

static const sfield stat_milestone_fields[] = {
    SF(gc_stat_milestones, classic_wins), SF(gc_stat_milestones, endless_wins),
    SF(gc_stat_milestones, total_battles), SF(gc_stat_milestones, total_catches),
    SF(gc_stat_milestones, total_hatches), SF(gc_stat_milestones, total_shinies),
    SF(gc_stat_milestones, highest_wave), SF(gc_stat_milestones, highest_damage),
    SF(gc_stat_milestones, total_money_earned),
};

static const sfield egg_fields[] = {
    SF(gc_egg, id), SF(gc_egg, hatch_waves), SF(gc_egg, tier), SF(gc_egg, species),
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

    int n_dex = 0;
    for (int i = 1; i <= GC_MAX_SPECIES_ID; i++) {
        if (sys->dex[i].seen_attr || sys->dex[i].caught_attr) n_dex++;
    }
    if (n_dex > 0) {
        sv_dex_entry *dex_buf = malloc(sizeof(sv_dex_entry) * n_dex);
        if (dex_buf) {
            int k = 0;
            for (int i = 1; i <= GC_MAX_SPECIES_ID; i++) {
                if (sys->dex[i].seen_attr || sys->dex[i].caught_attr) {
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
            st->friendship || st->egg_moves || st->classic_win_count ||
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
                    st->friendship || st->egg_moves || st->classic_win_count ||
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
    }

    r = section(&body, "stats");
    if (!r.bad) {
        get_block(&r, stat_milestone_fields, NF(stat_milestone_fields), &sys->stats, sizeof sys->stats, 1, &dropped);
    }

    r = section(&body, "eggs");
    if (!r.bad) {
        sys->n_eggs = get_block(&r, egg_fields, NF(egg_fields), sys->eggs, sizeof sys->eggs[0], GC_MAX_EGGS, &dropped);
    }

    r = section(&body, "dex");
    if (!r.bad) {
        sv_dex_entry dex_buf[256];
        int count;
        while ((count = get_block(&r, dex_fields, NF(dex_fields), dex_buf, sizeof(sv_dex_entry), 256, &dropped)) > 0) {
            for (int i = 0; i < count; i++) {
                uint16_t sp = dex_buf[i].species;
                if (sp > 0 && sp <= GC_MAX_SPECIES_ID) {
                    sys->dex[sp] = dex_buf[i].entry;
                }
            }
        }
    }

    r = section(&body, "starters");
    if (!r.bad) {
        sv_starter_entry st_buf[256];
        int count;
        while ((count = get_block(&r, starter_fields, NF(starter_fields), st_buf, sizeof(sv_starter_entry), 256, &dropped)) > 0) {
            for (int i = 0; i < count; i++) {
                uint16_t sp = st_buf[i].species;
                if (sp > 0 && sp <= GC_MAX_SPECIES_ID) {
                    sys->starters[sp] = st_buf[i].entry;
                }
            }
        }
    }

    return true;
}
