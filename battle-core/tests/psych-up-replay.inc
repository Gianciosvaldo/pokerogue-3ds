#define _POSIX_C_SOURCE 200809L
/* C half of the whole-turn differential test: replays the cases turntest.producer.test.ts
 * wrote inside upstream. Each case gives the state before a turn, the phases upstream ran,
 * the state after and what was left queued. The C engine runs the same turn from the same
 * state and prints the same records, which are compared field by field.
 *
 * A case whose turn touched something the C port doesn't have yet (pr_note_unported, or a
 * side effect only recorded with pr_note_deferred) is "not covered" and listed by reason,
 * never counted as a pass. A covered case must match on every record. */
#include "pr/engine.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const battler_tag_names[] = {BATTLER_TAG_TYPE_VALUES};
static const char *const arena_tag_names[] = {ARENA_TAG_TYPE_VALUES};
static const char *const pos_tag_names[] = {POSITIONAL_TAG_TYPE_VALUES};

#define MAX_LINES 256   /* a trainer case has up to 12 Pokemon of about 5 lines each */
#define LINE_LEN 4096
typedef struct { char s[MAX_LINES][LINE_LEN]; int n; } lines_t;

static void add_line(lines_t *L, const char *fmt, ...)
{
    if (L->n >= MAX_LINES) { fprintf(stderr, "too many lines\n"); exit(2); }
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(L->s[L->n++], LINE_LEN, fmt, ap);
    va_end(ap);
}

/* ------------------------------------------------------------------ reading records */

static int name_index(const char *const *names, int n, const char *s)
{
    for (int i = 0; i < n; i++)
        if (!strcmp(names[i], s)) return i;
    return -1;
}

/* whether the data file carries moneyScattered and addedType (files written before they were dumped don't) */
static bool has_money;
static bool has_biome;         /* the ARENA line gives the biome */

/* The value of key=... in a record line (NULL if absent). Returns a pointer into a static
 * buffer, valid until the next call. */
static const char *field(const char *line, const char *key)
{
    static char out[LINE_LEN];
    size_t kl = strlen(key);
    for (const char *p = line; (p = strstr(p, key)); p++) {
        if ((p == line || p[-1] == ' ') && p[kl] == '=') {
            const char *v = p + kl + 1, *e = strchr(v, ' ');
            size_t n = e ? (size_t)(e - v) : strlen(v);
            memcpy(out, v, n);
            out[n] = 0;
            return out;
        }
    }
    return NULL;
}

static const char *need(const char *line, const char *key)
{
    const char *v = field(line, key);
    if (!v) { fprintf(stderr, "missing %s= in: %s\n", key, line); exit(2); }
    return v;
}

static long long inum(const char *line, const char *key) { return strtoll(need(line, key), NULL, 10); }
static double dnum(const char *line, const char *key) { return strtod(need(line, key), NULL); }
/* num(): "u" for undefined/null */
static long long unum(const char *line, const char *key, long long undef)
{
    const char *v = need(line, key);
    return !strcmp(v, "u") ? undef : strtoll(v, NULL, 10);
}

/* Splits a list value ("-" = empty) on sep into at most max items; returns the count */
static int split(const char *v, char sep, char items[][64], int max)
{
    if (!strcmp(v, "-")) return 0;
    int n = 0;
    const char *p = v;
    for (;;) {
        const char *e = strchr(p, sep);
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if (n >= max) { fprintf(stderr, "list too long: %s\n", v); exit(2); }
        if (len > 63) len = 63;
        memcpy(items[n], p, len);
        items[n++][len] = 0;
        if (!e) break;
        p = e + 1;
    }
    return n;
}

/* A JSON array of strings (JSON.stringify's output) into out[0..max-1]; the rest are left "" */
static void json_strings(const char *v, char out[][96], int max)
{
    for (int i = 0; i < max; i++) out[i][0] = 0;
    const char *p = strchr(v, '[');
    for (int n = 0; p && n < max; n++) {
        p = strchr(p, '"');
        if (!p) break;
        size_t k = 0;
        for (p++; *p && *p != '"'; p++) {
            unsigned c = (unsigned char)*p;
            if (c == '\\') {
                c = (unsigned char)*++p;
                if (c == 'n') c = '\n';
                else if (c == 't') c = '\t';
                else if (c == 'u') {
                    c = (unsigned)strtoul((char[5]){p[1], p[2], p[3], p[4], 0}, NULL, 16);
                    p += 4;
                    if (c >= 0x800 && k + 3 < 95) { out[n][k++] = (char)(0xe0 | c >> 12); out[n][k++] = (char)(0x80 | ((c >> 6) & 0x3f)); c = 0x80 | (c & 0x3f); }
                    else if (c >= 0x80 && k + 2 < 95) { out[n][k++] = (char)(0xc0 | c >> 6); c = 0x80 | (c & 0x3f); }
                }
            }
            if (k < 95) out[n][k++] = (char)c;
        }
        out[n][k] = 0;
        if (*p) p++;
    }
}

static void read_rng(const char *v, pr_rng *r)
{
    char it[4][64];
    if (split(v, ',', it, 4) != 4) { fprintf(stderr, "bad RNG state %s\n", v); exit(2); }
    r->c = strtod(it[0], NULL);
    r->s0 = strtod(it[1], NULL);
    r->s1 = strtod(it[2], NULL);
    r->s2 = strtod(it[3], NULL);
}

/* "move:t1/t2:useMode:result" */
static pr_turn_move read_turn_move(const char *s)
{
    char it[4][64], tg[4][64];
    if (split(s, ':', it, 4) != 4) { fprintf(stderr, "bad turn move %s\n", s); exit(2); }
    pr_turn_move tm = {0};
    tm.move = (uint16_t)atoi(it[0]);
    tm.n_targets = (uint8_t)split(it[1], '/', tg, 4);
    for (int i = 0; i < tm.n_targets; i++) tm.targets[i] = (int8_t)atoi(tg[i]);
    tm.use_mode = (uint8_t)atoi(it[2]);
    tm.result = !strcmp(it[3], "u") ? PR_MOVE_RESULT_UNDEFINED : (uint8_t)atoi(it[3]);
    return tm;
}

typedef struct {
    pr_battle bt;
    /* the parties' Pokemon by party slot, [0] the player's and [1] the enemy's; slot 0 is the one on
     * the field (battler 0 and 2), the others the bench */
    pr_pokemon mon[2][PR_MAX_PARTY];
    int picks[8], n_picks;     /* PICK: the player's picks on the party screens of the turn */
    int skip;                  /* the case has records this test doesn't read (modifiers) */
    long long wisher[PR_MAX_POSITIONAL_TAGS];   /* a Wish the setup made: its Pokemon's id + 1, 0 = none */
    char trainer_json[256];    /* TRAINER: the names as the record has them (JSON), printed back as they are */
    char trainer_names[2][96];
    int epicks[16][2], n_epicks;   /* EPICK: the trainer's picks during the turn, {trainerSlot, pick} */
    int btag;                  /* battler tags: only their type, turns, source, stockpile and crit stages are in
                                * the record, which is all the condition probes read */
} case_state;

/* A Pokemon record's key: its battler index on the field (0, 2), or p<slot> / e<slot> for the
 * player's or the enemy's bench */
static pr_pokemon *mon_for(case_state *cs, const char *b, int *side)
{
    int s = -1, k = -1;
    if (!strcmp(b, "0")) s = 0, k = 0;
    else if (!strcmp(b, "2")) s = 1, k = 0;
    else if ((b[0] == 'p' || b[0] == 'e') && b[1] >= '1' && b[1] < '0' + PR_MAX_PARTY && !b[2]) s = b[0] == 'e', k = b[1] - '0';
    if (s < 0) {
        fprintf(stderr, "Pokemon %s in a singles case\n", b);
        exit(2);
    }
    if (side) *side = s;
    return &cs->mon[s][k];
}

static void mon_key(char out[8], const pr_battle *bt, int side, int k)
{
    if (k == 0) snprintf(out, 8, "%d", side * 2);
    else snprintf(out, 8, "%c%d", side ? 'e' : 'p', k);
    (void)bt;
}

static void read_state(const lines_t *L, int from, int to, case_state *cs)
{
    pr_battle *bt = &cs->bt;
    for (int i = from; i < to; i++) {
        const char *l = L->s[i];
        char it[16][64];
        if (!strncmp(l, "RNG ", 4)) {
            read_rng(need(l, "g"), &bt->rng);
            bt->rng_live = true;
            const char *b = need(l, "b");
            bt->battle_rng_live = strcmp(b, "none") != 0;
            if (bt->battle_rng_live) read_rng(b, &bt->battle_rng);
        } else if (!strncmp(l, "BATTLE ", 7)) {
            bt->turn = (int)inum(l, "turn");
            bt->wave_index = (int)inum(l, "wave");
            pr_seed_from_ascii(&bt->battle_seed, need(l, "bseed"));
            pr_seed_from_ascii(&bt->wave_seed, need(l, "wseed"));
            bt->last_move = (uint16_t)inum(l, "lastmove");
            bt->enemy_faints = (int)inum(l, "efaints");
            bt->player_faints = (int)inum(l, "pfaints");
            const char *keys[2] = {"pfh", "efh"};
            for (int k = 0; k < 2; k++) {
                char fh[12][64];
                int n = split(need(l, keys[k]), ',', fh, 12);
                bt->n_faint_history[k] = (uint8_t)n;
                for (int j = 0; j < n; j++) {
                    char *c = strchr(fh[j], ':');
                    bt->faint_history[k][j].id = (uint32_t)strtoul(fh[j], NULL, 10);
                    bt->faint_history[k][j].turn = c ? atoi(c + 1) : 0;
                }
            }
            bt->last_player_involved = (int8_t)unum(l, "lpi", -1);
            bt->last_enemy_involved = (int8_t)unum(l, "lei", -1);
            char pids[PR_MAX_PARTY][64];
            bt->n_participants = (uint8_t)split(need(l, "participants"), ',', pids, PR_MAX_PARTY);
            for (int j = 0; j < bt->n_participants; j++) bt->participants[j] = (uint32_t)strtoul(pids[j], NULL, 10);
            bt->post_battle_loot = (int)inum(l, "loot");
            bt->battle_score = dnum(l, "score");
            has_money = field(l, "money") != NULL;
            bt->money_scattered = has_money ? (int32_t)inum(l, "money") : 0;
            bt->battle_type = (uint8_t)inum(l, "type");
            bt->double_battle = inum(l, "double") != 0;
            bt->max_exp_level = (int)inum(l, "mel");
            bt->ability_bar_visible = inum(l, "bar") != 0;
            bt->last_used_pokeball = (int8_t)unum(l, "ball", -1);
            bt->game_mode = field(l, "gm") ? (int8_t)inum(l, "gm") : -1;   /* older files have none */
        } else if (!strncmp(l, "ARENA ", 6)) {
            bt->weather = (uint8_t)inum(l, "weather");
            bt->weather_turns_left = (int16_t)inum(l, "wturns");
            bt->weather_max_duration = (int16_t)inum(l, "wmax");
            bt->terrain = (uint8_t)inum(l, "terrain");
            bt->terrain_turns_left = (int16_t)inum(l, "tturns");
            bt->terrain_max_duration = (int16_t)inum(l, "tmax");
            bt->ignore_abilities = inum(l, "ignab") != 0;
            bt->ignoring_effect_source = (int8_t)unum(l, "ignsrc", -1);
            has_biome = field(l, "biome") != NULL;
            bt->has_biome = has_biome;
            bt->biome = has_biome ? (int16_t)inum(l, "biome") : 0;
        } else if (!strncmp(l, "ATAG ", 5)) {
            if (bt->n_arena_tags >= PR_MAX_ARENA_TAGS) { fprintf(stderr, "too many arena tags\n"); exit(2); }
            pr_arena_tag *t = &bt->arena_tags[bt->n_arena_tags++];
            memset(t, 0, sizeof *t);
            int type = name_index(arena_tag_names, ARENA_TAG_TYPE_COUNT_, need(l, "type"));
            if (type < 0) { fprintf(stderr, "unknown arena tag %s\n", need(l, "type")); exit(2); }
            t->type = (uint8_t)type;
            t->side = (uint8_t)inum(l, "side");
            t->turn_count = (int16_t)inum(l, "turns");
            t->max_duration = (int16_t)inum(l, "max");
            t->source_move = (uint16_t)inum(l, "smove");
            t->source_id = inum(l, "sid");
            t->layers = field(l, "layers") ? (int8_t)inum(l, "layers") : 1;   /* EntryHazardTag */
        } else if (!strncmp(l, "POS ", 4)) {             /* positionalTagManager.tags */
            if (bt->n_pos_tags >= PR_MAX_POSITIONAL_TAGS) { fprintf(stderr, "too many positional tags\n"); exit(2); }
            pr_positional_tag *t = &bt->pos_tags[bt->n_pos_tags++];
            memset(t, 0, sizeof *t);
            int type = name_index(pos_tag_names, POSITIONAL_TAG_TYPE_COUNT_, need(l, "type"));
            if (type < 0) { fprintf(stderr, "unknown positional tag %s\n", need(l, "type")); exit(2); }
            t->type = (uint8_t)type;
            t->turn_count = (int16_t)inum(l, "turns");
            t->target_index = (int8_t)inum(l, "target");
            t->source_id = inum(l, "sid");
            t->source_move = (uint16_t)inum(l, "smove");
            t->heal_hp = dnum(l, "heal");
            const char *w = field(l, "wisher");
            if (w) cs->wisher[bt->n_pos_tags - 1] = strtoll(w, NULL, 10) + 1;
        } else if (!strncmp(l, "MON ", 4)) {
            int side;
            pr_pokemon *p = mon_for(cs, need(l, "b"), &side);
            memset(p, 0, sizeof *p);
            p->is_player = side == 0;
            p->id = (uint32_t)inum(l, "id");
            p->species = (uint16_t)inum(l, "sp");
            p->form = (uint8_t)inum(l, "form");
            p->fusion_species = field(l, "fsp") ? (uint16_t)inum(l, "fsp") : 0;
            p->fusion_form = field(l, "fform") ? (uint8_t)inum(l, "fform") : 0;
            p->fusion_ability_index = field(l, "fabil") ? (int8_t)inum(l, "fabil") : -1;
            p->level = (uint8_t)inum(l, "lv");
            if (split(need(l, "ivs"), ',', it, 6) != 6) { fprintf(stderr, "bad ivs\n"); exit(2); }
            for (int j = 0; j < 6; j++) p->ivs[j] = (uint8_t)atoi(it[j]);
            p->nature = (uint8_t)inum(l, "nat");
            p->ability_override = (int32_t)unum(l, "ab", -1);
            p->ability = (uint16_t)inum(l, "abil");
            p->base_ability = (uint16_t)inum(l, "babil");
            p->passive_flag = inum(l, "passive") != 0;
            p->custom_passive = (int16_t)inum(l, "cpass");
            p->gender = (int8_t)inum(l, "gender");
            p->friendship = (uint8_t)inum(l, "friend");
            p->hp = (int32_t)inum(l, "hp");
            if (split(need(l, "stats"), ',', it, 6) != 6) { fprintf(stderr, "bad stats\n"); exit(2); }
            for (int j = 0; j < 6; j++) p->stats[j] = atoi(it[j]);
            p->status = (uint8_t)inum(l, "st");
            p->status_toxic_turns = (int16_t)inum(l, "toxic");
            p->status_sleep_turns = (int16_t)unum(l, "sleep", -1);
            p->status_freeze_turns = (int16_t)unum(l, "freeze", -1);
            if (split(need(l, "stages"), ',', it, 7) != 7) { fprintf(stderr, "bad stages\n"); exit(2); }
            for (int j = 0; j < 7; j++) p->stat_stages[j] = (int8_t)atoi(it[j]);
            p->terastallized = inum(l, "tera") != 0;
            p->tera_type = (int8_t)inum(l, "teratype");
            int ns = split(need(l, "stellar"), ',', it, 16);
            for (int j = 0; j < ns; j++) p->stellar_types_boosted |= 1u << atoi(it[j]);
            p->is_boss = inum(l, "boss") != 0;
            p->shiny = inum(l, "shiny") != 0;
            p->switch_out_status = inum(l, "out") != 0;
            /* isOnField() is false while switchOutStatus is set: a Pokemon in a field slot is
             * still in the scene's field then, a bench Pokemon isn't */
            p->on_field = inum(l, "onfield") != 0 || (p->switch_out_status && p == &cs->mon[side][0]);
            p->summon_hit_count = (uint8_t)inum(l, "shc");
            int nt = split(need(l, "types"), ',', it, PR_MAX_SUMMON_TYPES + 1);
            if (nt > PR_MAX_SUMMON_TYPES) { cs->skip = 1; nt = PR_MAX_SUMMON_TYPES; }
            p->n_summon_types = (uint8_t)nt;
            for (int j = 0; j < nt; j++) p->summon_types[j] = (int8_t)atoi(it[j]);
            p->added_type = field(l, "added") ? (int8_t)inum(l, "added") : 0;
            p->ability_suppressed = inum(l, "abilsup") != 0;
            p->temp_turn_count = (int32_t)inum(l, "tsd");
            p->temp_wave_turn_count = (int32_t)inum(l, "tswd");
            p->n_ab_applied_summon = (uint8_t)split(need(l, "aas"), ',', it, 4);
            for (int j = 0; j < p->n_ab_applied_summon; j++) p->ab_applied_summon[j] = (uint16_t)atoi(it[j]);
            p->n_ab_applied_wave = (uint8_t)split(need(l, "aaw"), ',', it, 4);
            for (int j = 0; j < p->n_ab_applied_wave; j++) p->ab_applied_wave[j] = (uint16_t)atoi(it[j]);
            p->has_eaten_berry = field(l, "eb") && inum(l, "eb") != 0;
            if (field(l, "ss")) {                      /* summonData.stats */
                if (split(need(l, "ss"), ',', it, 6) != 6) { fprintf(stderr, "bad ss\n"); exit(2); }
                for (int j = 0; j < 6; j++) p->summon_stats[j] = strtod(it[j], NULL);
            }
            p->trainer_slot = field(l, "tslot") ? (int8_t)inum(l, "tslot") : 0;
            p->has_trainer = p->trainer_slot != 0;             /* EnemyPokemon.hasTrainer() */
            p->turn_move_effectiveness = -1;
            if (field(l, "qmega")) p->queued_mega = inum(l, "qmega") != 0;
            if (field(l, "qdmax")) p->queued_dynamax = inum(l, "qdmax") != 0;
            if (field(l, "dmax")) p->is_dynamaxed = inum(l, "dmax") != 0;
            if (field(l, "dmleft")) p->dynamax_turns_left = (uint8_t)inum(l, "dmleft");
            if (field(l, "dmlv")) p->dynamax_level = (uint8_t)inum(l, "dmlv");
        } else if (!strncmp(l, "TURN ", 5)) {
            pr_pokemon *p = mon_for(cs, need(l, "b"), NULL);
            p->acted = inum(l, "acted") != 0;
            p->order = (int8_t)unum(l, "order", -1);
            p->hit_count = (int8_t)inum(l, "hc");
            p->hits_left = (int8_t)inum(l, "hl");
            p->total_damage_dealt = (int32_t)inum(l, "dealt");
            p->single_hit_damage_dealt = (int32_t)inum(l, "single");
            p->damage_taken = (int32_t)inum(l, "taken");
            p->stat_stages_increased = inum(l, "inc") != 0;
            p->stat_stages_decreased = inum(l, "dec") != 0;
            const char *eff = need(l, "eff");
            p->turn_move_effectiveness = !strcmp(eff, "u") ? -1 : strtod(eff, NULL);
            p->pending_status = (uint8_t)inum(l, "pending");
            p->switched_in_this_turn = inum(l, "switched") != 0;
            p->summoned_this_turn = inum(l, "summoned") != 0;
            p->joined_round = inum(l, "joined") != 0;
            p->combining_pledge = (uint16_t)inum(l, "pledge");
            static char recs[32][64];
            int n = split(need(l, "recv"), ',', recs, 32);
            p->n_attacks_received = 0;
            for (int j = n - 1; j >= 0; j--) { /* newest first: unshift from the oldest */
                char f[6][64];
                if (split(recs[j], ':', f, 6) != 6) { fprintf(stderr, "bad recv %s\n", recs[j]); exit(2); }
                pr_attack_record r = {0};
                r.move = (uint16_t)atoi(f[0]);
                r.result = (uint8_t)atoi(f[1]);
                r.damage = strtod(f[2], NULL);
                r.critical = atoi(f[3]) != 0;
                r.source_id = strtoll(f[4], NULL, 10);
                r.source_battler_index = (int8_t)atoi(f[5]);
                pr_unshift_attack_received(p, &r);
            }
        } else if (!strncmp(l, "MOVES ", 6)) {
            pr_pokemon *p = mon_for(cs, need(l, "b"), NULL);
            char mv[4][64];
            p->n_moves = (uint8_t)split(need(l, "list"), ',', mv, 4);
            for (int j = 0; j < p->n_moves; j++) {
                char f[4][64];
                if (split(mv[j], ':', f, 4) != 4) { fprintf(stderr, "bad move %s\n", mv[j]); exit(2); }
                p->moves[j] = (uint16_t)atoi(f[0]);
                p->pp_used[j] = (uint8_t)atoi(f[1]);
                p->pp_up[j] = (uint8_t)atoi(f[2]);
                p->max_pp_override[j] = !strcmp(f[3], "u") ? -1 : (int16_t)atoi(f[3]);
            }
        } else if (!strncmp(l, "HIST ", 5) || !strncmp(l, "MQ ", 3)) {
            pr_pokemon *p = mon_for(cs, need(l, "b"), NULL);
            static char tm[64][64];
            int n = split(need(l, "list"), ',', tm, 64);
            if (l[0] == 'H') {
                p->n_move_history = 0;
                for (int j = 0; j < n; j++) pr_push_move_history(p, read_turn_move(tm[j]));
            } else {
                if (n > PR_MAX_MOVE_QUEUE) { cs->skip = 1; n = PR_MAX_MOVE_QUEUE; }
                p->n_move_queue = (uint8_t)n;
                for (int j = 0; j < n; j++) p->move_queue[j] = read_turn_move(tm[j]);
            }
        } else if (!strncmp(l, "BTAG ", 5)) {
            pr_pokemon *p = mon_for(cs, need(l, "b"), NULL);
            if (p->n_tags >= PR_MAX_BATTLER_TAGS) { fprintf(stderr, "too many battler tags\n"); exit(2); }
            pr_battler_tag *t = &p->tags[p->n_tags++];
            memset(t, 0, sizeof *t);
            int type = name_index(battler_tag_names, BATTLER_TAG_TYPE_COUNT_, need(l, "type"));
            if (type < 0) { fprintf(stderr, "unknown battler tag %s\n", need(l, "type")); exit(2); }
            t->type = (uint8_t)type;
            t->turn_count = (int16_t)inum(l, "turns");
            t->source_move = (uint16_t)inum(l, "smove");
            t->source_id = inum(l, "sid");
            t->stockpiled_count = (int8_t)inum(l, "stock");
            t->crit_stages = (int8_t)inum(l, "crit");
            t->autotomize_count = (int16_t)inum(l, "auto");
            if (field(l, "stat")) {                    /* HighestStatBoostTag: Protosynthesis, Quark Drive */
                t->stat = (int8_t)inum(l, "stat");
                t->multiplier = dnum(l, "mult");
            }
            t->source_index = -1;
            if (field(l, "hp")) {                      /* SubstituteTag */
                t->hp = (int32_t)inum(l, "hp");
                t->in_focus = inum(l, "focus") != 0;
            }
            if (field(l, "sidx")) t->source_index = (int8_t)inum(l, "sidx");   /* SeedTag */
            if (field(l, "act")) t->activated = inum(l, "act") != 0;           /* ShellTrapTag */
            if (field(l, "bpf")) t->base_pure_flying = inum(l, "bpf") != 0;    /* RoostedTag */
            if (field(l, "mid")) t->move_id = (uint16_t)inum(l, "mid");         /* Disabled, Encore, Gorilla Tactics */
            if (field(l, "fc")) t->faint_count = (int8_t)inum(l, "fc");         /* SupremeOverlordTag */
            if (field(l, "scd")) {                                              /* StockpilingTag */
                t->stock_def = (int8_t)inum(l, "scd");
                t->stock_spdef = (int8_t)inum(l, "scs");
            }
            /* the line holds all of a ported tag type's state, and of Protosynthesis's and Quark
             * Drive's (which only the damage path reads and a weather or terrain ending removes);
             * the others can hold more */
            if (!pr_tag_type_ported(type) && !(pr_battler_tag_is(type, PR_TAGCLASS_HIGHEST_STAT_BOOST) && field(l, "stat")))
                cs->btag = 1;
        } else if(!strncmp(l,"EFFECTIVE ",10)){
            /* Compared with fresh actual effective-stat calculations by dump_mon. */
            need(l,"b");need(l,"stats");
        } else if (!strncmp(l, "MOD ", 4)) {
            int side = (int)inum(l, "side");
            if (side < 0 || side > 1 || bt->n_mods[side] >= PR_MAX_MODIFIERS) { cs->skip = 1; continue; }
            pr_modifier *m = &bt->mods[side][bt->n_mods[side]++];
            memset(m, 0, sizeof *m);
            const char *cls_name = need(l, "cls");
            int c = pr_mod_class_by_name(cls_name);
            m->cls = c > 0 ? (uint8_t)c : PR_MOD_PERSISTENT;
            m->unknown_subclass = c <= 0;
            m->pokemon_id = inum(l, "pid");
            m->stack_count = (int32_t)inum(l, "stack");
            m->is_transferable = true;
            if (field(l, "btype")) m->berry_type = (int8_t)inum(l, "btype");
            if (m->cls == PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE) {
                if (!field(l, "effect")) { fprintf(stderr, "missing enemy status-token effect\n"); exit(2); }
                m->berry_type = (int8_t)inum(l, "effect");
            }
        } else if (!strncmp(l, "PARTY ", 6)) {
            int side = (int)inum(l, "side");
            static char ms[PR_MAX_PARTY + 1][64];
            int n = split(need(l, "list"), ',', ms, PR_MAX_PARTY + 1);
            if (side < 0 || side > 1 || n > PR_MAX_PARTY) { cs->skip = 1; continue; }
            /* the party's ids in order: each member's state is its MON record, keyed by slot */
            bt->n_party[side] = (uint8_t)n;
            for (int j = 0; j < n; j++) {
                pr_pokemon *p = &cs->mon[side][j];
                if (p->id != (uint32_t)strtoul(ms[j], NULL, 10)) {
                    fprintf(stderr, "party slot %d.%d is %s, its record says %u\n", side, j, ms[j], p->id);
                    exit(2);
                }
                bt->party[side][j] = p;
            }
        } else if (!strncmp(l, "CMD ", 4)) {
            int b = (int)inum(l, "b");
            struct pr_turn_command *c = &bt->commands[b];
            c->command = (int8_t)inum(l, "cmd");
            c->move = (uint16_t)inum(l, "move");
            char tg[4][64];
            c->n_targets = (uint8_t)split(need(l, "targets"), ',', tg, 4);
            for (int j = 0; j < c->n_targets; j++) c->targets[j] = (int8_t)atoi(tg[j]);
            c->use_mode = (uint8_t)inum(l, "mode");
            c->skip = inum(l, "skip") != 0;
            c->cursor = (int8_t)unum(l, "cursor", PR_CURSOR_UNDEFINED);
            c->baton = inum(l, "baton") != 0;
        } else if (!strncmp(l, "PICK ", 5)) {
            char ps[8][64];
            cs->n_picks = split(need(l, "list"), ',', ps, 8);
            for (int j = 0; j < cs->n_picks; j++) cs->picks[j] = atoi(ps[j]);
        } else if (!strncmp(l, "TRAINER ", 8)) {               /* currentBattle.trainer */
            bt->trainer_partner = inum(l, "partner") != 0;
            const char *js = strstr(l, " names=");
            if (!js) { fprintf(stderr, "missing names= in: %s\n", l); exit(2); }
            snprintf(cs->trainer_json, sizeof cs->trainer_json, "%s", js + 7);
            json_strings(cs->trainer_json, cs->trainer_names, 2);
            bt->trainer_names[0] = cs->trainer_names[0];
            bt->trainer_names[1] = cs->trainer_names[1];
        } else if (!strncmp(l, "SESSION ", 8)) {
            char hs[16][64];
            int n = split(need(l, "healed"), ',', hs, 16);
            for (int j = 0; j < n; j++) {
                char *c = strchr(hs[j], ':');
                pr_status_healed_set((uint16_t)atoi(hs[j]), c ? atoi(c + 1) : 0);
            }
        } else if (!strncmp(l, "CALLSEL ", 8)) {
            /* RandomMovesetMoveAttr.selectedMove of Sleep Talk and Assist: move:selected */
            char cs2[8][64];
            int n = split(need(l, "list"), ',', cs2, 8);
            for (int j = 0; j < n; j++) {
                char *c = strchr(cs2[j], ':');
                pr_called_selected_set(atoi(cs2[j]), c ? atoi(c + 1) : 0);
            }
        } else if (!strncmp(l, "PRE ", 4)) {
            int b = (int)inum(l, "b");
            bt->pre_commands[b].command = (int8_t)inum(l, "cmd");
            bt->pre_commands[b].skip = inum(l, "skip") != 0;
        } else {
            fprintf(stderr, "unknown record: %s\n", l);
            exit(2);
        }
    }
}

/* ------------------------------------------------------------------ printing records */

typedef struct { char s[LINE_LEN]; size_t n; } sbuf;
static void sb(sbuf *b, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int k = vsnprintf(b->s + b->n, sizeof b->s - b->n, fmt, ap);
    va_end(ap);
    if (k > 0) b->n += (size_t)k;
    if (b->n >= sizeof b->s) b->n = sizeof b->s - 1;
}
/* a list: "-" when empty, items joined by sep */
#define LIST(b, n, sep, ...)                                  \
    do {                                                      \
        if ((n) == 0) sb(b, "-");                             \
        for (int i_ = 0; i_ < (n); i_++) {                    \
            if (i_) sb(b, "%c", sep);                         \
            int i = i_; (void)i;                              \
            sb(b, __VA_ARGS__);                               \
        }                                                     \
    } while (0)

static void num_or_u(sbuf *b, long long v, long long undef)
{
    if (v == undef) sb(b, "u");
    else sb(b, "%lld", v);
}

static void seed_str(sbuf *b, const pr_seedstr *s)
{
    for (size_t i = 0; i < s->len; i++) sb(b, "%c", (char)s->u[i]);
}

static void turn_move_str(sbuf *b, const pr_turn_move *t)
{
    sb(b, "%d:", t->move);
    LIST(b, t->n_targets, '/', "%d", t->targets[i]);
    sb(b, ":%d:", t->use_mode);
    num_or_u(b, t->result, PR_MOVE_RESULT_UNDEFINED);
}

/* " stat=S mult=M" for a HighestStatBoostTag, "" for the others */
static const char *boost_fields(const pr_battler_tag *t)
{
    static char buf[64];
    if (t->type == BATTLER_TAG_TYPE_SUBSTITUTE) {             /* SubstituteTag.hp, sourceInFocus */
        snprintf(buf, sizeof buf, " hp=%d focus=%d", t->hp, t->in_focus ? 1 : 0);
        return buf;
    }
    if (t->type == BATTLER_TAG_TYPE_SHELL_TRAP && t->activated) return " act=1";   /* ShellTrapTag.activated */
    if (t->type == BATTLER_TAG_TYPE_ROOSTED && t->base_pure_flying) return " bpf=1";   /* RoostedTag.isBasePureFlying */
    if (t->type == BATTLER_TAG_TYPE_SUPREME_OVERLORD) {       /* SupremeOverlordTag.faintCount */
        snprintf(buf, sizeof buf, " fc=%d", t->faint_count);
        return buf;
    }
    if (t->type == BATTLER_TAG_TYPE_DISABLED || t->type == BATTLER_TAG_TYPE_ENCORE
        || t->type == BATTLER_TAG_TYPE_GORILLA_TACTICS) {     /* moveId */
        snprintf(buf, sizeof buf, " mid=%d", t->move_id);
        return buf;
    }
    if (t->type == BATTLER_TAG_TYPE_STOCKPILING) {            /* StockpilingTag.statChangeCounts */
        snprintf(buf, sizeof buf, " scd=%d scs=%d", t->stock_def, t->stock_spdef);
        return buf;
    }
    if (t->type == BATTLER_TAG_TYPE_SEEDED) {                 /* SeedTag.sourceIndex */
        snprintf(buf, sizeof buf, " sidx=%d", t->source_index);
        return buf;
    }
    if (!pr_battler_tag_is(t->type, PR_TAGCLASS_HIGHEST_STAT_BOOST)) return "";
    snprintf(buf, sizeof buf, " stat=%d mult=%.17g", t->stat, t->multiplier);
    return buf;
}

static void dump_mon(lines_t *L, const pr_battle *bt, const pr_pokemon *p, const char *b, bool *incomplete)
{
    sbuf s = {{0}, 0};
    sb(&s, "MON b=%s id=%u sp=%d form=%d", b, p->id, p->species, p->form);
    if (p->fusion_species)
        sb(&s, " fsp=%d fform=%d fabil=%d", p->fusion_species, p->fusion_form, p->fusion_ability_index);
    sb(&s, " lv=%d ivs=", p->level);
    LIST(&s, 6, ',', "%d", p->ivs[i]);
    sb(&s, " nat=%d ab=", p->nature);
    num_or_u(&s, p->ability_override, -1);
    sb(&s, " abil=%d babil=%d passive=%d cpass=%d gender=%d friend=%d hp=%d stats=", p->ability, p->base_ability,
       p->passive_flag, p->custom_passive, p->gender, p->friendship, p->hp);
    LIST(&s, 6, ',', "%d", p->stats[i]);
    sb(&s, " st=%d toxic=%d sleep=", p->status, p->status ? p->status_toxic_turns : 0);
    num_or_u(&s, p->status ? p->status_sleep_turns : -1, -1);
    sb(&s, " freeze=");
    num_or_u(&s, p->status ? p->status_freeze_turns : -1, -1);
    sb(&s, " stages=");
    LIST(&s, 7, ',', "%d", p->stat_stages[i]);
    sb(&s, " tera=%d teratype=%d stellar=", p->terastallized, p->tera_type);
    int st[32], nst = 0;
    for (int t = 0; t < 32; t++)
        if (p->stellar_types_boosted & (1u << t)) st[nst++] = t;
    LIST(&s, nst, ',', "%d", st[i]);
    sb(&s, " boss=%d shiny=%d onfield=%d out=%d shc=%d types=", p->is_boss, p->shiny, p->on_field && !p->switch_out_status,
       p->switch_out_status, p->summon_hit_count);
    LIST(&s, p->n_summon_types, ',', "%d", p->summon_types[i]);
    if (has_money) sb(&s, " added=%d", p->added_type);
    sb(&s, " abilsup=%d tsd=%d tswd=%d aas=", p->ability_suppressed, p->temp_turn_count, p->temp_wave_turn_count);
    LIST(&s, p->n_ab_applied_summon, ',', "%d", p->ab_applied_summon[i]);
    sb(&s, " aaw=");
    LIST(&s, p->n_ab_applied_wave, ',', "%d", p->ab_applied_wave[i]);
    if (p->has_eaten_berry) sb(&s, " eb=1");
    if (p->trainer_slot) sb(&s, " tslot=%d", p->trainer_slot);
    bool ss = false;
    for (int i = 0; i < 6; i++) ss |= p->summon_stats[i] != 0;
    if (ss) {                                  /* summonData.stats (Power Trick), when any is set */
        sb(&s, " ss=");
        LIST(&s, 6, ',', "%.17g", p->summon_stats[i]);
    }
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "TURN b=%s acted=%d order=", b, p->acted);
    num_or_u(&s, p->order, -1);
    sb(&s, " hc=%d hl=%d dealt=%d single=%d taken=%d inc=%d dec=%d eff=", p->hit_count, p->hits_left,
       p->total_damage_dealt, p->single_hit_damage_dealt, p->damage_taken, p->stat_stages_increased,
       p->stat_stages_decreased);
    if (p->turn_move_effectiveness < 0) sb(&s, "u");
    else sb(&s, "%.17g", p->turn_move_effectiveness);
    sb(&s, " pending=%d switched=%d summoned=%d joined=%d pledge=%d recv=", p->pending_status,
       p->switched_in_this_turn, p->summoned_this_turn, p->joined_round, p->combining_pledge);
    int nr = p->n_attacks_received;
    if (nr > PR_MAX_ATTACKS_RECEIVED) { *incomplete = true; nr = PR_MAX_ATTACKS_RECEIVED; }
    LIST(&s, nr, ',', "%d:%d:%.17g:%d:%lld:%d", p->attacks_received[i].move, p->attacks_received[i].result,
         p->attacks_received[i].damage, p->attacks_received[i].critical, (long long)p->attacks_received[i].source_id,
         p->attacks_received[i].source_battler_index);
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "MOVES b=%s list=", b);
    for (int i = 0; i < p->n_moves; i++) {
        if (i) sb(&s, ",");
        sb(&s, "%d:%d:%d:", p->moves[i], p->pp_used[i], p->pp_up[i]);
        num_or_u(&s, p->max_pp_override[i], -1);
    }
    if (!p->n_moves) sb(&s, "-");
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "HIST b=%s list=", b);
    if (p->n_move_history > PR_MAX_MOVE_HISTORY) *incomplete = true;
    int kept = p->n_move_history < PR_MAX_MOVE_HISTORY ? p->n_move_history : PR_MAX_MOVE_HISTORY;
    for (int i = 0; i < kept; i++) {
        if (i) sb(&s, ",");
        turn_move_str(&s, &p->move_history[i]);
    }
    if (!kept) sb(&s, "-");
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "MQ b=%s list=", b);
    for (int i = 0; i < p->n_move_queue; i++) {
        if (i) sb(&s, ",");
        turn_move_str(&s, &p->move_queue[i]);
    }
    if (!p->n_move_queue) sb(&s, "-");
    add_line(L, "%s", s.s);
    s.n=0;sb(&s,"EFFECTIVE b=%s stats=",b);
    for(int stat=STAT_ATK;stat<=STAT_SPD;stat++)
        sb(&s,"%s%d",stat==STAT_ATK?"":",",pr_get_effective_stat((pr_battle *)bt,p,stat,NULL,-1,false,false));
    add_line(L,"%s",s.s);

    for (int i = 0; i < p->n_tags; i++)
        add_line(L, "BTAG b=%s type=%s turns=%d smove=%d sid=%lld stock=%d crit=%d auto=%d%s", b,
                 battler_tag_names[p->tags[i].type], p->tags[i].turn_count, p->tags[i].source_move,
                 (long long)p->tags[i].source_id, p->tags[i].stockpiled_count, p->tags[i].crit_stages,
                 p->tags[i].autotomize_count, boost_fields(&p->tags[i]));
    (void)bt;
}

static void rng_line(sbuf *s, const pr_battle *bt)
{
    sb(s, "RNG g=%.17g,%.17g,%.17g,%.17g b=", bt->rng.c, bt->rng.s0, bt->rng.s1, bt->rng.s2);
    if (bt->battle_rng_live) sb(s, "%.17g,%.17g,%.17g,%.17g", bt->battle_rng.c, bt->battle_rng.s0, bt->battle_rng.s1, bt->battle_rng.s2);
    else sb(s, "none");
}

/* Psych Up suite: after the turn, the attacker's critical-hit result against the defender
 * for a high-critical move, through pr_get_critical_hit_result, and the RNG it leaves */
static int line_diff(const char *c, const char *js);
static struct { int total, pass, fail; } crit_probes;
static void crit_probe(pr_battle *bt, case_state *cs, const char *case_name, const char *js)
{
    char att_b[8], def_b[8];
    snprintf(att_b, sizeof att_b, "%s", need(js, "att"));
    snprintf(def_b, sizeof def_b, "%s", need(js, "def"));
    if (!strcmp(att_b, def_b)) { fprintf(stderr, "%s: probe attacker is its defender\n", case_name); exit(2); }
    pr_pokemon *att = mon_for(cs, att_b, NULL), *def = mon_for(cs, def_b, NULL);
    int move = (int)inum(js, "move");
    if (move <= 0 || move >= PR_MOVE_COUNT) { fprintf(stderr, "%s: bad probe move\n", case_name); exit(2); }
    bool crit = pr_get_critical_hit_result(bt, def, att, move);
    static sbuf s;
    s.n = 0; s.s[0] = 0;
    sb(&s, "CRITPROBE att=%s def=%s move=%d crit=%d ", att_b, def_b, move, crit ? 1 : 0);
    rng_line(&s, bt);
    crit_probes.total++;
    if (line_diff(s.s, js) < 0) { crit_probes.pass++; return; }
    crit_probes.fail++;
    printf("CRITPROBE FAIL %s\n  C : %s\n  JS: %s\n", case_name, s.s, js);
}

static bool dump_before;   /* dump_state prints the state a case starts from */
static void dump_state(lines_t *L, const pr_battle *bt, const case_state *cs, bool *incomplete)
{
    sbuf s = {{0}, 0};
    rng_line(&s, bt);
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "BATTLE turn=%d wave=%d bseed=", bt->turn, bt->wave_index);
    seed_str(&s, &bt->battle_seed);
    sb(&s, " wseed=");
    seed_str(&s, &bt->wave_seed);
    sb(&s, " lastmove=%d efaints=%d pfaints=%d pfh=", bt->last_move, bt->enemy_faints, bt->player_faints);
    LIST(&s, bt->n_faint_history[0], ',', "%u:%d", bt->faint_history[0][i].id, bt->faint_history[0][i].turn);
    sb(&s, " efh=");
    LIST(&s, bt->n_faint_history[1], ',', "%u:%d", bt->faint_history[1][i].id, bt->faint_history[1][i].turn);
    sb(&s, " lpi=");
    num_or_u(&s, bt->last_player_involved, -1);
    sb(&s, " lei=");
    num_or_u(&s, bt->last_enemy_involved, -1);
    sb(&s, " participants=");
    uint32_t part[PR_MAX_PARTY];                  /* printed in id order, as the producer sorts them */
    int np = bt->n_participants;
    for (int i = 0; i < np; i++) {
        int j = i;
        for (; j > 0 && part[j - 1] > bt->participants[i]; j--) part[j] = part[j - 1];
        part[j] = bt->participants[i];
    }
    LIST(&s, np, ',', "%u", part[i]);
    sb(&s, " loot=%d score=%.17g", bt->post_battle_loot, bt->battle_score);
    if (has_money) sb(&s, " money=%d", bt->money_scattered);
    sb(&s, " type=%d double=%d mel=%d bar=%d ball=", bt->battle_type, bt->double_battle, bt->max_exp_level,
       bt->ability_bar_visible);
    num_or_u(&s, bt->last_used_pokeball, -1);
    if (bt->game_mode >= 0) sb(&s, " gm=%d", bt->game_mode);
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "ARENA weather=%d wturns=%d wmax=%d terrain=%d tturns=%d tmax=%d ignab=%d ignsrc=", bt->weather,
       bt->weather ? bt->weather_turns_left : 0, bt->weather ? bt->weather_max_duration : 0, bt->terrain,
       bt->terrain ? bt->terrain_turns_left : 0, bt->terrain ? bt->terrain_max_duration : 0, bt->ignore_abilities);
    num_or_u(&s, bt->ignoring_effect_source, -1);
    if (has_biome) sb(&s, " biome=%d", bt->biome);
    add_line(L, "%s", s.s);
    for (int i = 0; i < bt->n_arena_tags; i++) {
        const pr_arena_tag *t = &bt->arena_tags[i];
        char layers[16] = "";
        if (pr_arena_tag_is(t->type, PR_ARENACLASS_ENTRY_HAZARD)) snprintf(layers, sizeof layers, " layers=%d", t->layers);
        add_line(L, "ATAG type=%s side=%d turns=%d max=%d smove=%d sid=%lld%s", arena_tag_names[t->type], t->side,
                 t->turn_count, t->max_duration, t->source_move, (long long)t->source_id, layers);
    }
    for (int i = 0; i < bt->n_pos_tags; i++) {
        const pr_positional_tag *t = &bt->pos_tags[i];
        char wisher[32] = "";
        if (dump_before && cs->wisher[i]) snprintf(wisher, sizeof wisher, " wisher=%lld", cs->wisher[i] - 1);
        add_line(L, "POS type=%s turns=%d target=%d sid=%lld smove=%d heal=%.17g%s", pos_tag_names[t->type],
                 t->turn_count, t->target_index, (long long)t->source_id, t->source_move, t->heal_hp, wisher);
    }
    /* every party member in party order, the player's first; a side with no party given: its
     * Pokemon on the field */
    for (int side = 0; side < 2; side++) {
        char key[8];
        if (!bt->n_party[side]) {
            dump_mon(L, bt, bt->field[side * 2], side ? "2" : "0", incomplete);
            continue;
        }
        for (int k = 0; k < bt->n_party[side]; k++) {
            mon_key(key, bt, side, k);
            dump_mon(L, bt, bt->party[side][k], key, incomplete);
        }
    }
    for (int side = 0; side < 2; side++) {
        for (int i = 0; i < bt->n_mods[side]; i++) {
            const pr_modifier *m = &bt->mods[side][i];
            if (m->cls == PR_MOD_BERRY)
                add_line(L, "MOD side=%d cls=%s pid=%lld stack=%d btype=%d", side,
                         pr_mod_class_name[m->cls], (long long)m->pokemon_id, m->stack_count, m->berry_type);
            else if (m->cls == PR_MOD_ENEMY_ATTACK_STATUS_EFFECT_CHANCE)
                add_line(L, "MOD side=%d cls=%s pid=%lld stack=%d effect=%d", side,
                         pr_mod_class_name[m->cls], (long long)m->pokemon_id, m->stack_count, m->berry_type);
            else
                add_line(L, "MOD side=%d cls=%s pid=%lld stack=%d", side,
                         pr_mod_class_name[m->cls], (long long)m->pokemon_id, m->stack_count);
        }
    }
    for (int side = 0; side < 2; side++) {
        if (!bt->n_party[side]) continue;
        s.n = 0;
        sb(&s, "PARTY side=%d list=", side);
        LIST(&s, bt->n_party[side], ',', "%u", bt->party[side][i]->id);
        add_line(L, "%s", s.s);
    }
    if (cs->n_picks) {
        s.n = 0;
        sb(&s, "PICK list=");
        LIST(&s, cs->n_picks, ',', "%d", cs->picks[i]);
        add_line(L, "%s", s.s);
    }
    if (cs->trainer_json[0]) add_line(L, "TRAINER partner=%d names=%s", bt->trainer_partner, cs->trainer_json);
    (void)cs;
    for (int k = 0; k < 4; k++) {
        const struct pr_turn_command *c = &bt->commands[k];
        if (c->command >= 0) {
            s.n = 0;
            sb(&s, "CMD b=%d cmd=%d move=%d targets=", k, c->command, c->move);
            LIST(&s, c->n_targets, ',', "%d", c->targets[i]);
            sb(&s, " mode=%d skip=%d cursor=", c->use_mode, c->skip);
            num_or_u(&s, c->cursor, PR_CURSOR_UNDEFINED);
            sb(&s, " baton=%d", c->baton);
            add_line(L, "%s", s.s);
        }
        const struct pr_turn_command *pc = &bt->pre_commands[k];
        if (pc->command >= 0) add_line(L, "PRE b=%d cmd=%d skip=%d", k, pc->command, pc->skip);
    }
    /* session state kept on ability attr objects upstream */
    uint16_t hab[16];
    uint8_t hef[16];
    int nh = pr_status_healed_list(hab, hef, 16);
    if (nh) {
        s.n = 0;
        sb(&s, "SESSION healed=");
        LIST(&s, nh, ',', "%d:%d", hab[i], hef[i]);
        add_line(L, "%s", s.s);
    }
    uint16_t cm[8], csel[8];
    int nc = pr_called_selected_list(cm, csel, 8);
    if (nc) {
        s.n = 0;
        sb(&s, "CALLSEL list=");
        LIST(&s, nc, ',', "%d:%d", cm[i], csel[i]);
        add_line(L, "%s", s.s);
    }
}

static void dump_trace_queue(lines_t *L, const pr_engine *e)
{
    sbuf s = {{0}, 0};
    sb(&s, "TRACE list=");
    LIST(&s, e->n_trace, ',', "%s", e->trace[i]);
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "QUEUE levels=");
    /* what is queued after a BattleEndPhase for the game is left out, as the producer's afterBattle */
    for (int lv = 0; lv < e->n_levels; lv++) {
        if (lv) sb(&s, "|");
        int shown = 0;
        for (int k = 0; k < e->level_len[lv]; k++) {
            const pr_phase *p = &e->pool[e->levels[lv][k]];
            if (p->type == PH_GAME_OVER || p->type == PH_SELECT_MODIFIER || p->type == PH_SELECT_BIOME ||
                p->type == PH_NEW_BATTLE)
                continue;
            if (shown++) sb(&s, ",");
            if (p->type == PH_MARKER) sb(&s, "Marker:%s", pr_phase_name[p->marker_of]);
            else sb(&s, "%s", pr_phase_name[p->type]);
        }
        if (!shown) sb(&s, "-");
    }
    sb(&s, " deferred=%d dyn=", e->deferred_active);
    /* the non-empty dynamic queues, "Name:b/b", sorted by string like the producer's dyn.sort() */
    char items[PH_COUNT_][256];
    int n = 0;
    for (int t = 0; t < PH_COUNT_; t++) {
        if (!e->n_dyn[t]) continue;
        sbuf q = {{0}, 0};
        sb(&q, "%s:", pr_phase_name[t]);
        for (int k = 0; k < e->n_dyn[t]; k++) sb(&q, k ? "/%d" : "%d", e->pool[e->dyn[t][k]].battler);
        snprintf(items[n++], sizeof items[0], "%s", q.s);
    }
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && strcmp(items[j - 1], items[j]) > 0; j--) {
            char tmp[256];
            memcpy(tmp, items[j], sizeof tmp);
            memcpy(items[j], items[j - 1], sizeof tmp);
            memcpy(items[j - 1], tmp, sizeof tmp);
        }
    LIST(&s, n, ',', "%s", items[i]);
    add_line(L, "%s", s.s);
    /* the arguments of the switch prompt the turn hands over to */
    const pr_phase *sp = &e->stop_phase;
    if (e->stop == PH_SWITCH)
        add_line(L, "END stop=%s type=%d field=%d modal=%d return=%d", pr_phase_name[e->stop], sp->sw_type,
                 sp->sw_field_index, sp->sw_modal, sp->sw_do_return);
    else if (e->stop == PH_SWITCH_SUMMON)
        add_line(L, "END stop=%s type=%d field=%d slot=%d return=%d player=%d", pr_phase_name[e->stop], sp->sw_type,
                 sp->sw_field_index, sp->sw_slot, sp->sw_do_return, sp->sw_player);
    else
        add_line(L, "END stop=%s", pr_phase_name[e->stop]);
}

/* ------------------------------------------------------------------ comparing */

/* Compares two record lines atom by atom (split on ' ', '=', ',', ':', '/', '|'); atoms that
 * both parse as numbers compare as doubles. Returns the byte offset of the first difference
 * in `js`, or -1 when equal. */
static int line_diff(const char *c, const char *js)
{
    const char *delim = " =,:/|";
    const char *a = c, *b = js;
    for (;;) {
        size_t la = strcspn(a, delim), lb = strcspn(b, delim);
        char x[128], y[128];
        snprintf(x, sizeof x, "%.*s", (int)(la < 127 ? la : 127), a);
        snprintf(y, sizeof y, "%.*s", (int)(lb < 127 ? lb : 127), b);
        char *ex, *ey;
        double dx = strtod(x, &ex), dy = strtod(y, &ey);
        bool numeric = *x && *y && !*ex && !*ey;
        if (numeric ? !(dx == dy || (dx != dx && dy != dy)) : strcmp(x, y)) return (int)(b - js);
        a += la;
        b += lb;
        if (*a != *b) return (int)(b - js);
        if (!*a) return -1;
        a++;
        b++;
    }
}

/* ------------------------------------------------------------------ main */

/* ported calls made during covered turns, per closure id */
static uint32_t covered_hits[PR_CLOSURE_COUNT];

/* How many covered turns ran each phase (by upstream name, battler dropped) */
static struct { char name[40]; int n; } ran_phase[64];
static int n_ran_phase;
static void count_phases(const pr_engine *e)
{
    int seen[64] = {0};
    for (int t = 0; t < e->n_trace; t++) {
        char nm[40];
        snprintf(nm, sizeof nm, "%s", e->trace[t]);
        nm[strcspn(nm, ":")] = 0;
        int k = 0;
        while (k < n_ran_phase && strcmp(ran_phase[k].name, nm)) k++;
        if (k == n_ran_phase) {
            if (n_ran_phase == 64) continue;
            snprintf(ran_phase[n_ran_phase++].name, sizeof ran_phase[0].name, "%s", nm);
        }
        if (!seen[k]) { seen[k] = 1; ran_phase[k].n++; }
    }
}

/* The move conditions (pr_moves[].conditions*) with a C port, and how many of them the covered
 * turns ran: a port no covered turn reaches is untested by this test */
static void report_conditions(void)
{
    static bool is_cond[PR_CLOSURE_COUNT];
    for (int m = 0; m < PR_MOVE_COUNT; m++) {
        const pr_fnlist *ls[3] = {&pr_moves[m].conditions, &pr_moves[m].conditions_seq2, &pr_moves[m].conditions_seq3};
        for (int l = 0; l < 3; l++)
            for (int i = 0; i < ls[l]->n; i++) is_cond[ls[l]->v[i]] = true;
    }
    int n = 0, ran = 0;
    char line[4096];
    int len = 0;
    line[0] = 0;
    for (int i = 0; i < PR_CLOSURE_COUNT; i++) {
        if (!is_cond[i]) continue;
        n++;
        if (covered_hits[i]) {
            ran++;
            if (len < (int)sizeof line - 40)
                len += snprintf(line + len, sizeof line - len, " %s:%s=%u", pr_closures_meta[i].owner,
                                pr_closures_meta[i].hash, covered_hits[i]);
        }
    }
    printf("move conditions: %d closures; %d ran on covered turns:%s\n", n, ran, line);
}

/* TURN_PROBE cases: each PROBE line is move.applyConditions(user, target, seq) run upstream on
 * the state before, in order, with its result and the RNG state after it; seq=0 lines call a
 * StatStageChangeAttr's own condition (attr = its index on the move). TURN_SPROBE cases: each
 * SPROBE line is pokemon.canSetStatus(eff, quiet, ovr, src, ign), with the abilities applied so
 * far on both sides */
static bool is_probe_line(const char *l)
{
    return !strncmp(l, "PROBE ", 6) || !strncmp(l, "SPROBE ", 7) || !strncmp(l, "CPROBE ", 7);
}
/* TURN_CPROBE cases: each CPROBE line is a command screen check on the state before. kind=move:
 * moveset[slot].isUsable(p, false, true); kind=trap: p.isTrapped(messages), with the text the
 * switch command shows (the player's only; the enemy's: the ability's message). text= is the
 * rest of the line, JSON. */
static void msg_json(sbuf *b, const pr_msg *m);

static void ab_applied(sbuf *s, const pr_pokemon *p)
{
    LIST(s, p->n_ab_applied_summon, ',', "%d", p->ab_applied_summon[i]);
    sb(s, "/");
    LIST(s, p->n_ab_applied_wave, ',', "%d", p->ab_applied_wave[i]);
}

static struct { int total, covered, pass, fail, unported, after_unported, threw, shown; } probes;
static void probe_case(pr_battle *bt, const lines_t *js, int from, const char *case_name)
{
    static pr_engine eng;              /* an empty phase queue, as the producer's */
    memset(&eng, 0, sizeof eng);
    eng.n_levels = 1;
    eng.current = -1;
    bt->engine = &eng;
    bool stop = false;
    for (int i = from; i < js->n && is_probe_line(js->s[i]); i++) {
        const char *l = js->s[i];
        probes.total++;
        if (!strcmp(need(l, "res"), "throw") || stop) {
            if (stop) probes.after_unported++;
            else probes.threw++;
            stop = true;
            continue;
        }
        bool sprobe = l[0] == 'S', cprobe = l[0] == 'C';
        pr_msg why = {0};
        pr_pokemon *u = sprobe || cprobe ? NULL : bt->field[inum(l, "u")], *t = sprobe || cprobe ? NULL : bt->field[inum(l, "t")];
        int move = sprobe || cprobe ? -1 : (int)inum(l, "move"), seq = sprobe || cprobe ? -1 : (int)inum(l, "seq");
        bt->n_unported = bt->n_deferred = 0;
        static uint32_t hits_before[PR_CLOSURE_COUNT];
        memcpy(hits_before, pr_closure_hits, sizeof hits_before);
        bool res;
        if (cprobe) {
            pr_pokemon *p = bt->field[inum(l, "p")];
            if (!strcmp(need(l, "kind"), "move")) {
                res = pr_move_usable(bt, p, (int)inum(l, "slot"), false, true, &why);
            } else {
                pr_msg ab;
                res = pr_is_trapped(bt, p, &ab);
                why = ab;
                if (res && p->is_player) pr_can_switch(bt, p, false, &why);
            }
        } else if (sprobe) {
            /* what the call queues (ability bars) never runs: start each one from an empty queue */
            memset(&eng, 0, sizeof eng);
            eng.n_levels = 1;
            eng.current = -1;
            int src = (int)inum(l, "src");
            res = pr_can_set_status(bt, &eng, bt->field[inum(l, "p")], (int)inum(l, "eff"), true, inum(l, "ovr") != 0,
                                    src < 0 ? NULL : bt->field[src], inum(l, "ign") != 0);
        } else if (seq == 0) {
            /* a StatStageChangeAttr's options.condition(user, target, move) */
            const pr_move *mv = &pr_moves[move];
            unsigned k = (unsigned)inum(l, "attr");
            if (k >= mv->nattr || pr_attrs[mv->attr0 + k].cls != PR_AC_StatStageChangeAttr
                || pr_attr_effects[mv->attr0 + k].condition == PR_NO_FN) {
                printf("PROBE ATTR MISMATCH %s: %s\n", case_name, l);
                probes.fail++;
                stop = true;
                continue;
            }
            res = pr_closure_call(bt, pr_attr_effects[mv->attr0 + k].condition, PR_AC_StatStageChangeAttr, 0, u, t,
                                  move) != 0;
        } else {
            res = pr_move_apply_conditions(bt, move, u, t, seq);
        }
        if (bt->n_unported || bt->n_deferred || eng.overflow) {
            /* upstream may have moved the RNG where C can't follow: the rest of the case is out */
            probes.unported++;
            stop = true;
            continue;
        }
        probes.covered++;
        for (int k = 0; k < PR_CLOSURE_COUNT; k++) covered_hits[k] += pr_closure_hits[k] - hits_before[k];
        sbuf got = {{0}, 0};
        rng_line(&got, bt);
        char want[LINE_LEN], g[LINE_LEN];
        snprintf(g, sizeof g, "%s", need(l, "g"));             /* need() reuses one buffer */
        snprintf(want, sizeof want, "RNG g=%s b=%s", g, need(l, "b"));
        bool ok = res == (inum(l, "res") != 0) && line_diff(got.s, want) < 0;
        if (cprobe) {
            sbuf txt = {{0}, 0};
            msg_json(&txt, &why);
            const char *want_text = strstr(l, " text=");
            if (!want_text || strcmp(txt.s, want_text + 6)) ok = false;
            if (!ok) sb(&got, " text=%s", txt.s);
        }
        if (sprobe) {
            sbuf aa = {{0}, 0};
            ab_applied(&aa, bt->field[0]);
            sb(&aa, "|");
            ab_applied(&aa, bt->field[2]);
            if (strcmp(aa.s, need(l, "aa"))) ok = false;
            if (!ok) sb(&got, " aa=%s", aa.s);
        }
        if (ok) probes.pass++;
        else {
            probes.fail++;
            if (probes.shown++ < 8)
                printf("PROBE FAIL %s: %s\n  C : res=%d %s\n", case_name, l, res, got.s);
            stop = true;                           /* later probes start from a different RNG */
        }
    }
    bt->engine = NULL;
}

/* ---- the texts a turn shows (TEXTS line): C's messages rendered with upstream's English
 * locale files (TURNTEST_LOCALES, default ../../pokerogue/locales/en next to this checkout) ---- */
static const char *locales_dir(void)
{
    const char *d = getenv("TURNTEST_LOCALES");
    return d ? d : "/home/claude/pokerogue/locales/en";
}

static const char *locale_get(void *ctx, const char *file, int fallback, size_t *len)
{
    (void)ctx;
    static struct { char name[64]; char *text; size_t len; } cache[96];
    static int n_cache;
    if (fallback) return NULL;
    for (int i = 0; i < n_cache; i++)
        if (!strcmp(cache[i].name, file)) { *len = cache[i].len; return cache[i].text; }
    if (n_cache >= 96) return NULL;
    char path[512];
    snprintf(path, sizeof path, "%s/%s.json", locales_dir(), file);
    FILE *f = fopen(path, "rb");
    char *text = NULL;
    size_t n = 0;
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        text = malloc((size_t)sz + 1);
        n = fread(text, 1, (size_t)sz, f);
        text[n] = 0;
        fclose(f);
        /* upstream's tests load each locale file as a module namespace (import.meta.glob, eager),
         * whose named exports can't be reserved words: there move "return" and ability "static"
         * are missing, and i18next shows "return.name". Hide them the same way (the game has them). */
        static const char *const hidden[] = {"\n  \"return\": ", "\n  \"static\": "};
        for (int i = 0; i < 2; i++) {
            char *k = strstr(text, hidden[i]);
            if (k) k[4] = '_';                               /* "return" -> "_eturn" */
        }
    }
    snprintf(cache[n_cache].name, sizeof cache[n_cache].name, "%s", file);
    cache[n_cache].text = text;
    cache[n_cache].len = n;
    n_cache++;
    *len = n;
    return text;
}

/* JSON.stringify of one string */
static void json_str(sbuf *b, const char *s)
{
    sb(b, "\"");
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        switch (*p) {
        case '"': sb(b, "\\\""); break;
        case '\\': sb(b, "\\\\"); break;
        case '\n': sb(b, "\\n"); break;
        case '\r': sb(b, "\\r"); break;
        case '\t': sb(b, "\\t"); break;
        case '\b': sb(b, "\\b"); break;
        case '\f': sb(b, "\\f"); break;
        default:
            if (*p < 0x20) sb(b, "\\u%04x", *p);
            else sb(b, "%c", *p);
        }
    }
    sb(b, "\"");
}

/* engine source lines whose message has no text yet, by covered turns showing them */
typedef struct { const char *site; int n; } todo_site;
static todo_site todo_sites[256];
static int n_todo_sites;
static void count_todo_site(const char *site)
{
    if (!site) site = "?";
    for (int i = 0; i < n_todo_sites; i++)
        if (!strcmp(todo_sites[i].site, site)) { todo_sites[i].n++; return; }
    if (n_todo_sites < 256) todo_sites[n_todo_sites++] = (todo_site){site, 1};
}

static int leftovers_fired = 0;
static int shell_bell_fired = 0;
static int focus_band_fired = 0;
static int quick_claw_fired = 0;
static int kings_rock_fired = 0;
static int white_herb_fired = 0;

/* The TEXTS line C's events give: false when a message has no text port yet */
static bool c_texts(sbuf *b, const pr_event *ev, int n)
{
    static const pr_text_source src = {locale_get, NULL, "en"};
    bool known = true;
    sb(b, "TEXTS [");
    int k = 0;
    for (int i = 0; i < n; i++) {
        if (ev[i].type != PR_EV_TEXT) continue;
        if (ev[i].msg.key) {
            if (!strcmp(ev[i].msg.key, "modifier:turnHealApply")) leftovers_fired++;
            if (!strcmp(ev[i].msg.key, "modifier:hitHealApply")) shell_bell_fired++;
            if (!strcmp(ev[i].msg.key, "modifier:surviveDamageApply")) focus_band_fired++;
            if (!strcmp(ev[i].msg.key, "modifier:bypassSpeedChanceApply")) quick_claw_fired++;
            if (!strcmp(ev[i].msg.key, "modifier:resetNegativeStatStageApply")) white_herb_fired++;
        }
        if (k++) sb(b, ",");
        if (!ev[i].msg.key) {
            if (known) count_todo_site(ev[i].msg.args[0].s);   /* the first one in the turn */
            known = false;
            sb(b, "\"<?>\"");
            continue;
        }
        char text[1024];
        pr_msg_format(&ev[i].msg, &src, text, sizeof text);
        json_str(b, text);
    }
    sb(b, "]");
    return known;
}

/* a message as JSON text; no message (key NULL) is upstream's "" */
static void msg_json(sbuf *b, const pr_msg *m)
{
    static const pr_text_source src = {locale_get, NULL, "en"};
    char text[1024] = "";
    if (m->key) pr_msg_format(m, &src, text, sizeof text);
    json_str(b, text);
}

static struct { int cases, same, differ, unported, shown; } texts;
static pr_event all_events[PR_MAX_EVENTS * 4];
static int n_all_events;
/* The trainer's picks the producer recorded (EPICK), handed back in order as getNextSummonIndex;
 * a call the record doesn't have, or with another trainer slot, fails the case */
static int js_epicks[16][2], n_js_epicks, epick_at;
static bool epick_bad;
static int epick_cb(pr_battle *bt, int trainer_slot, void *ctx)
{
    (void)bt;
    (void)ctx;
    if (epick_at >= n_js_epicks) { epick_bad = true; return -1; }
    if (js_epicks[epick_at][0] != trainer_slot) epick_bad = true;
    return js_epicks[epick_at++][1];
}

static void keep_events(const pr_engine *e)
{
    for (int i = 0; i < e->n_events && n_all_events < PR_MAX_EVENTS * 4; i++) all_events[n_all_events++] = e->events[i];
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "turns.txt", "r");
    if (!f) { perror("turns"); return 2; }
    static lines_t js;
    static lines_t c_before, c_after;
    static case_state cs, cs0;
    static pr_engine eng;
    static int uncovered_by[PR_FEAT_COUNT_];
    int total = 0, covered = 0, pass = 0, fail = 0, parse_fail = 0, skipped = 0, stat_fail = 0;
    int shown = 0;
    static char line[LINE_LEN * 2];
    static char js_texts[LINE_LEN * 2];
    static char js_crit[LINE_LEN * 2];
    char case_name[64] = "";
    int run_at = -1;
    int js_start = 0, js_start_set = 0, js_start_yes = 0;

    for (;;) {
        char *got = fgets(line, sizeof line, f);
        if (got) line[strcspn(line, "\n")] = 0;
        if (got && line[0] == '#') { printf("%s\n", line); continue; }
        if (got && strncmp(line, "END ", 4)) {
            if (!strncmp(line, "CASE ", 5)) {
                snprintf(case_name, sizeof case_name, "%s", line);
                js.n = 0;
                js_texts[0] = 0;
                js_crit[0] = 0;
                n_js_epicks = 0;
                js_start = js_start_set = js_start_yes = 0;
                run_at = -1;
                continue;
            }
            if (!strncmp(line, "START ", 6)) {           /* TURN_START: a battle's start, not a turn */
                js_start = 1;
                js_start_set = (int)inum(line, "set");
                js_start_yes = (int)inum(line, "yes");
                continue;
            }
            if (!strncmp(line, "EPICK ", 6)) {
                char ps[16][64];
                n_js_epicks = split(need(line, "list"), ',', ps, 16);
                for (int j = 0; j < n_js_epicks; j++) sscanf(ps[j], "%d:%d", &js_epicks[j][0], &js_epicks[j][1]);
                continue;
            }
            if (!strcmp(line, "RUN")) { run_at = js.n; continue; }
            if (!strncmp(line, "TEXTS ", 6)) { snprintf(js_texts, sizeof js_texts, "%s", line); continue; }
            if (!strncmp(line, "CRITPROBE ", 10)) { snprintf(js_crit, sizeof js_crit, "%s", line); continue; }
            if (js.n >= MAX_LINES) { fprintf(stderr, "case too long\n"); return 2; }
            snprintf(js.s[js.n++], LINE_LEN, "%s", line);
            continue;
        }
        if (!got) break;
        /* END: the case is complete */
        if (js.n >= MAX_LINES) { fprintf(stderr, "case too long\n"); return 2; }
        snprintf(js.s[js.n++], LINE_LEN, "%s", line);
        if (run_at < 0) { fprintf(stderr, "%s: no RUN\n", case_name); return 2; }
        total++;

        memset(&cs, 0, sizeof cs);
        for (int k = 0; k < 4; k++) cs.bt.commands[k].command = cs.bt.pre_commands[k].command = -1;
        pr_status_healed_clear();
        pr_called_selected_reset();
        read_state(&js, 0, run_at, &cs);
        bool probe = run_at < js.n && is_probe_line(js.s[run_at]);
        if (cs.skip || (cs.btag && !probe)) { skipped++; continue; }
        pr_battle *bt = &cs.bt;
        bt->field[0] = &cs.mon[0][0];
        bt->field[2] = &cs.mon[1][0];
        /* the name a Wish from the setup was made with: getPokemonNameWithAffix of its Pokemon */
        for (int i = 0; i < bt->n_pos_tags; i++)
            for (int k = 0; cs.wisher[i] && k < 2 * PR_MAX_PARTY; k++)
                if (cs.mon[k / PR_MAX_PARTY][k % PR_MAX_PARTY].id == (uint32_t)(cs.wisher[i] - 1))
                    bt->pos_tags[i].pokemon_name = pr_arg_pokemon(bt, "", &cs.mon[k / PR_MAX_PARTY][k % PR_MAX_PARTY]).v;
        for (int k = 0; k < 2 * PR_MAX_PARTY; k++) {
            int side = k / PR_MAX_PARTY, slot = k % PR_MAX_PARTY;
            if (slot && slot >= bt->n_party[side]) continue;
            pr_pokemon *p = &cs.mon[side][slot];
            pr_update_passive(bt, p);
            int32_t hp = p->hp, stats[6];
            memcpy(stats, p->stats, sizeof stats);
            pr_calculate_stats(bt, p);
            p->hp = hp;
            if (memcmp(stats, p->stats, sizeof stats)) {
                if (++stat_fail <= 3) printf("STATS MISMATCH %s mon %d\n", case_name, k);
                memcpy(p->stats, stats, sizeof stats);
            }
        }

        /* the parser and the printer agree on the state before */
        c_before.n = 0;
        bool incomplete = false;
        dump_before = true;
        dump_state(&c_before, bt, &cs, &incomplete);
        dump_before = false;
        bool parse_ok = c_before.n == run_at;
        for (int i = 0; parse_ok && i < run_at; i++)
            if (line_diff(c_before.s[i], js.s[i]) >= 0) {
                if (++parse_fail <= 3) printf("PARSE %s\n  C : %s\n  JS: %s\n", case_name, c_before.s[i], js.s[i]);
                parse_ok = false;
            }
        if (!parse_ok) continue;

        if (probe) {
            probe_case(bt, &js, run_at, case_name);
            continue;
        }

        cs0 = cs;
        bt->n_unported = bt->n_deferred = 0;
        bt->next_summon_index = epick_cb;
        epick_at = 0;
        epick_bad = false;
        static uint32_t hits_before[PR_CLOSURE_COUNT];
        memcpy(hits_before, pr_closure_hits, sizeof hits_before);
        bt->battle_style_set = js_start_set != 0;
        pr_kings_rock_fired = 0;
        if (js_start) pr_start_battle(bt, &eng);
        else pr_run_turn(bt, &eng);
        n_all_events = 0;
        keep_events(&eng);
        /* the party screens: the n-th one of the turn takes the n-th pick, modulo the Pokemon it
         * can send in (the bench, not fainted), as the producer's stand-in for the screen does;
         * a victory goes on with no experience handed out, as the producer's stubbed applyPartyExp */
        for (int n = 0, guard = 0; guard < 32; guard++) {
            if (eng.stop == PH_SWITCH && eng.awaiting_pick) {
                int eligible[PR_MAX_PARTY], ne = 0;
                for (int k = bt->double_battle ? 2 : 1; k < bt->n_party[0]; k++)
                    if (bt->party[0][k]->hp > 0) eligible[ne++] = k;
                int r = cs.n_picks ? cs.picks[n++ % cs.n_picks] : 0;
                pr_resume_switch(bt, &eng, ne ? eligible[r % ne] : -1);
            } else if (eng.stop == PH_VICTORY) {
                pr_continue(bt, &eng);
            } else if (eng.stop == PH_CHECK_SWITCH && eng.awaiting_confirm) {
                pr_answer_check_switch(bt, &eng, js_start_yes != 0);
            } else break;
            keep_events(&eng);
        }
        c_after.n = 0;
        dump_trace_queue(&c_after, &eng);
        /* TRACE first, then the state, then QUEUE and END, as the producer writes them */
        static lines_t ordered;
        ordered.n = 0;
        add_line(&ordered, "%s", c_after.s[0]);
        dump_state(&ordered, bt, &cs, &incomplete);
        add_line(&ordered, "%s", c_after.s[1]);
        add_line(&ordered, "%s", c_after.s[2]);
        if (incomplete) pr_note_unported(bt, PR_FEAT_MOVE_HISTORY);
        /* the trainer's picks: asked for as often as upstream did, for the same trainer slots */
        if (epick_bad || (!bt->n_unported && !bt->n_deferred && epick_at != n_js_epicks)) {
            fail++;
            if (shown++ < 8)
                printf("FAIL %s\n  trainer picks: C asked %d times%s, upstream %d\n", case_name, epick_at,
                       epick_bad ? " (one upstream didn't make, or another trainer slot)" : "", n_js_epicks);
            continue;
        }

        if (bt->n_unported || bt->n_deferred) {
            /* TURNTEST_WHY=<reason> lists the cases a reason left out */
            const char *why = getenv("TURNTEST_WHY");
            for (int i = 0; why && i < bt->n_unported; i++)
                if (!strcmp(why, pr_unported_name(bt->unported[i]))) printf("WHY %s\n", case_name);
            for (int i = 0; i < bt->n_unported; i++) uncovered_by[bt->unported[i]]++;
            for (int i = 0; i < bt->n_deferred; i++) uncovered_by[bt->deferred[i]]++;
            continue;
        }
        covered++;
        kings_rock_fired += pr_kings_rock_fired;
        if (getenv("TURNTEST_COVERED")) printf("COVERED %s\n", case_name);   /* which cases were covered */
        count_phases(&eng);
        for (int i = 0; i < PR_CLOSURE_COUNT; i++) covered_hits[i] += pr_closure_hits[i] - hits_before[i];
        if (js_texts[0]) {
            static sbuf tb;
            tb.n = 0;
            tb.s[0] = 0;
            bool known = c_texts(&tb, all_events, n_all_events);
            texts.cases++;
            if (!known) texts.unported++;
            else if (!strcmp(tb.s, js_texts)) texts.same++;
            else {
                texts.differ++;
                if (texts.shown++ < (getenv("TURNTEST_TEXTS") ? atoi(getenv("TURNTEST_TEXTS")) : 5))
                    printf("TEXT DIFF %s\n  C : %s\n  JS: %s\n", case_name, tb.s, js_texts);
            }
        }
        if (js_crit[0]) crit_probe(bt, &cs, case_name, js_crit);
        int njs = js.n - run_at;
        bool ok = ordered.n == njs;
        int bad = -1;
        for (int i = 0; i < ordered.n && i < njs; i++)
            if (line_diff(ordered.s[i], js.s[run_at + i]) >= 0) { ok = false; bad = i; break; }
        if (ok) { pass++; continue; }
        fail++;
        if (shown++ < 8) {
            printf("FAIL %s\n", case_name);
            for (int i = 0; i < run_at; i++) printf("  before: %s\n", js.s[i]);
            if (bad >= 0) {
                int at = line_diff(ordered.s[bad], js.s[run_at + bad]);
                printf("  C : %s\n  JS: %s\n  (differs at JS column %d)\n", ordered.s[bad], js.s[run_at + bad], at);
            } else {
                printf("  line count: C %d, JS %d\n", ordered.n, njs);
            }
            printf("  C  trace: %s\n  JS trace: %s\n", ordered.s[0], js.s[run_at]);
        }
    }

    printf("cases %d (skipped %d: records not read yet), parse mismatches %d, stats mismatches %d\n", total,
           skipped, parse_fail, stat_fail);
    printf("covered %d: pass %d, fail %d; not covered %d\n", covered, pass, fail,
           total - skipped - parse_fail - covered);
    /* the reasons, most frequent first */
    int order[PR_FEAT_COUNT_], n = 0;
    for (int i = 0; i < PR_FEAT_COUNT_; i++)
        if (uncovered_by[i]) order[n++] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && uncovered_by[order[j - 1]] < uncovered_by[order[j]]; j--) {
            int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
        }
    if (n) printf("not covered, by reason (a case can have several):\n");
    for (int i = 0; i < n && i < 40; i++) printf("  %6d  %s\n", uncovered_by[order[i]], pr_unported_name((uint16_t)order[i]));
    /* the closures with no C port that were called, most frequent first */
    int top[12], nt = 0;
    for (int i = 0; i < PR_CLOSURE_COUNT; i++) {
        if (!pr_closure_misses[i]) continue;
        int j;
        if (nt < 12) j = nt++;
        else if (pr_closure_misses[top[11]] >= pr_closure_misses[i]) continue;   /* full: keep the 12 most called */
        else j = 11;
        top[j] = i;
        for (; j > 0 && pr_closure_misses[top[j - 1]] < pr_closure_misses[top[j]]; j--) {
            int t = top[j]; top[j] = top[j - 1]; top[j - 1] = t;
        }
    }
    if (nt) printf("closures with no C port, by calls:\n");
    for (int i = 0; i < nt; i++)
        printf("  %6u  %s %s\n", pr_closure_misses[top[i]], pr_closures_meta[top[i]].owner, pr_closures_meta[top[i]].hash);
    if (texts.cases)
        printf("texts of covered turns %d: identical %d, differ %d, a message with no text yet %d\n", texts.cases,
               texts.same, texts.differ, texts.unported);
    if (getenv("TURNTEST_SITES")) {
        for (int i = 0; i < n_todo_sites; i++)
            for (int j = i; j > 0 && todo_sites[j].n > todo_sites[j - 1].n; j--) {
                todo_site t = todo_sites[j]; todo_sites[j] = todo_sites[j - 1]; todo_sites[j - 1] = t;
            }
        for (int i = 0; i < n_todo_sites; i++) printf("TODO SITE %6d %s\n", todo_sites[i].n, todo_sites[i].site);
    }
    if (probes.total)
        printf("condition probes %d: covered %d, pass %d, fail %d; not covered %d (unported %d, after one %d, upstream threw %d)\n",
               probes.total, probes.covered, probes.pass, probes.fail, probes.total - probes.covered,
               probes.unported, probes.after_unported, probes.threw);
    if (n_ran_phase) {
        printf("covered turns that ran:");
        for (int k = 0; k < n_ran_phase; k++) printf(" %s=%d", ran_phase[k].name, ran_phase[k].n);
        printf("\n");
    }
    report_conditions();
    printf("item firings: Leftovers=%d, Shell Bell=%d, Focus Band=%d, Quick Claw=%d, King's Rock=%d, White Herb=%d, Sitrus=%d, Lum=%d, Leppa=%d, Liechi=%d, Ganlon=%d, Petaya=%d, Apicot=%d, Salac=%d\n",
           leftovers_fired, shell_bell_fired, focus_band_fired, quick_claw_fired, kings_rock_fired, white_herb_fired,
           pr_sitrus_berry_fired, pr_lum_berry_fired, pr_leppa_berry_fired,
           pr_liechi_berry_fired, pr_ganlon_berry_fired, pr_petaya_berry_fired, pr_apicot_berry_fired, pr_salac_berry_fired);
    printf("crit probes %d: pass %d, fail %d\n", crit_probes.total, crit_probes.pass, crit_probes.fail);
    fail += probes.fail + crit_probes.fail;
    (void)cs0;
    return fail || parse_fail ? 1 : 0;
}
