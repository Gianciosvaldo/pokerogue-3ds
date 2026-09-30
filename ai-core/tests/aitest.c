#define _POSIX_C_SOURCE 200809L
/* C half of ai-core's differential test: replays the cases ai.producer.test.ts wrote inside
 * upstream. Each case gives the battle state before the enemy's command (battle-core's turntest
 * records, plus AIMON / TRAINER / AI / SESS for what the AI reads that those don't hold) and what
 * upstream's EnemyCommandPhase left: the command, the switch counter, both RNG states, the enemy's
 * move queue, the move scores and the trainer's pick after a faint. pr_ai_enemy_command runs from the
 * same state and every record is compared.
 *
 * A case whose choice touched something not ported (bt->unported, battle-core's reasons and the
 * AI's own) is "not covered" and listed by reason, never counted as a pass. The record reader is
 * battle-core/tests/turntest.c's, extended. */
#include "pr/ai.h"
#include "pr/engine.h"
#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const battler_tag_names[] = {BATTLER_TAG_TYPE_VALUES};
static const char *const arena_tag_names[] = {ARENA_TAG_TYPE_VALUES};
static const char *const pos_tag_names[] = {POSITIONAL_TAG_TYPE_VALUES};

#define MAX_LINES 160
#define LINE_LEN 4096
typedef struct { char s[MAX_LINES][LINE_LEN]; int n; } lines_t;

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
    int btag;                  /* a battler tag whose record doesn't hold all its state */
    pr_ai ai;                  /* AIMON, AI, SESS: what the AI reads that the battle records don't hold */
    pr_ai_trainer trainer;     /* TRAINER: the trainer in a trainer battle */
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
        } else if (!strncmp(l, "MON ", 4)) {
            int side;
            pr_pokemon *p = mon_for(cs, need(l, "b"), &side);
            memset(p, 0, sizeof *p);
            p->is_player = side == 0;
            p->id = (uint32_t)inum(l, "id");
            p->species = (uint16_t)inum(l, "sp");
            p->form = (uint8_t)inum(l, "form");
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
            p->has_trainer = false;
            p->turn_move_effectiveness = -1;
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
            if (!pr_tag_type_ported(type) && !(pr_battler_tag_is(type, PR_TAGCLASS_HIGHEST_STAT_BOOST) && field(l, "stat"))
                && type != BATTLER_TAG_TYPE_IMPRISON)   /* the AI reads only its source */
                cs->btag = 1;
        } else if (!strncmp(l, "MOD ", 4)) {
            cs->skip = 1;
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
        } else if (!strncmp(l, "AIMON ", 6)) {
            pr_pokemon *p = mon_for(cs, need(l, "b"), NULL);
            int slot = (int)inum(l, "slot");
            p->has_trainer = !p->is_player && slot != TRAINER_SLOT_NONE;   /* EnemyPokemon.hasTrainer() */
            pr_ai_mon *m = pr_ai_mon_of(&cs->ai, p);
            int type = (int)inum(l, "type");
            m->ai_type = (uint8_t)(type >= 0 ? type : pr_ai_default_type(p->is_boss, p->has_trainer));
            m->ability_revealed = inum(l, "revealed") != 0;
            m->initial_team_index = (int8_t)inum(l, "iti");
            m->trainer_slot = (uint8_t)slot;
        } else if (!strncmp(l, "TRAINER ", 8)) {
            pr_ai_trainer *t = &cs->trainer;
            t->is_boss = inum(l, "boss") != 0;
            t->is_double = inum(l, "double") != 0;
            t->tera_mode = (uint8_t)inum(l, "tera");
            char tv[8][64];
            int n = split(need(l, "instant"), ',', tv, 8);
            t->n_instant_teras = (uint8_t)(n > 6 ? 6 : n);
            for (int j = 0; j < t->n_instant_teras; j++) t->instant_teras[j] = (int8_t)atoi(tv[j]);
            cs->ai.trainer = t;
        } else if (!strncmp(l, "AI ", 3)) {
            cs->ai.enemy_switch_counter = (int)inum(l, "counter");
            cs->ai.skip_enemy_turns = inum(l, "skip") != 0;
            pr_seed_from_ascii(&cs->ai.seed, need(l, "seed"));
        } else if (!strncmp(l, "SESS ", 5)) {
            char sv[32][64];
            int n = split(need(l, "multi"), ',', sv, 32);
            for (int j = 0; j < n; j++) {
                char *c = strchr(sv[j], ':');
                pr_ai_session_set_multi_status(atoi(sv[j]), c ? atoi(c + 1) : 0);
            }
            n = split(need(l, "heal"), ',', sv, 32);
            for (int j = 0; j < n; j++) {
                char *c = strchr(sv[j], ':');
                pr_ai_session_set_heal_ratio(atoi(sv[j]), c ? strtod(c + 1, NULL) : 1);
            }
        } else {
            fprintf(stderr, "unknown record: %s\n", l);
            exit(2);
        }
    }
}

/* ------------------------------------------------------------------ comparing */

static bool same_num(double a, double b) { return a == b || (a != a && b != b); }

static void cmd_str(char *out, size_t n, int b, const struct pr_turn_command *c)
{
    int k = snprintf(out, n, "CMD b=%d cmd=%d move=%d targets=", b, c->command, c->no_move ? 0 : c->move);
    if (!c->n_targets || c->no_move) k += snprintf(out + k, n - (size_t)k, "-");
    for (int i = 0; !c->no_move && i < c->n_targets; i++) k += snprintf(out + k, n - (size_t)k, "%s%d", i ? "," : "", c->targets[i]);
    k += snprintf(out + k, n - (size_t)k, " mode=%d skip=%d cursor=", c->no_move ? 0 : c->use_mode, c->skip ? 1 : 0);
    if (c->cursor == PR_CURSOR_UNDEFINED) k += snprintf(out + k, n - (size_t)k, "u");
    else k += snprintf(out + k, n - (size_t)k, "%d", c->cursor);
    snprintf(out + k, n - (size_t)k, " baton=%d", c->baton ? 1 : 0);
}

static void rng_str(char *out, size_t n, const pr_battle *bt)
{
    int k = snprintf(out, n, "RNG g=%.17g,%.17g,%.17g,%.17g b=", bt->rng.c, bt->rng.s0, bt->rng.s1, bt->rng.s2);
    if (!bt->battle_rng_live) snprintf(out + k, n - (size_t)k, "none");
    else snprintf(out + k, n - (size_t)k, "%.17g,%.17g,%.17g,%.17g", bt->battle_rng.c, bt->battle_rng.s0,
                  bt->battle_rng.s1, bt->battle_rng.s2);
}

static bool same_rng(const pr_rng *a, const pr_rng *b)
{
    return a->c == b->c && a->s0 == b->s0 && a->s1 == b->s1 && a->s2 == b->s2;
}

static void mq_str(char *out, size_t n, const pr_pokemon *p)
{
    int k = snprintf(out, n, "MQ b=2 list=");
    if (!p->n_move_queue) snprintf(out + k, n - (size_t)k, "-");
    for (int i = 0; i < p->n_move_queue; i++) {
        const pr_turn_move *t = &p->move_queue[i];
        k += snprintf(out + k, n - (size_t)k, "%s%d:", i ? "," : "", t->move);
        if (!t->n_targets) k += snprintf(out + k, n - (size_t)k, "-");
        for (int j = 0; j < t->n_targets; j++) k += snprintf(out + k, n - (size_t)k, "%s%d", j ? "/" : "", t->targets[j]);
        k += snprintf(out + k, n - (size_t)k, ":%d:", t->use_mode);
        if (t->result == PR_MOVE_RESULT_UNDEFINED) k += snprintf(out + k, n - (size_t)k, "u");
        else k += snprintf(out + k, n - (size_t)k, "%d", t->result);
    }
}

/* What the command left behind, taken before the trainer's faint pick is worked out (upstream
 * writes these records first; the pick's own matchup scores queue and apply things too) */
typedef struct { char queue[LINE_LEN]; char applied[2][LINE_LEN]; } snapshot;

static void take_snapshot(snapshot *sn, pr_battle *bt, pr_ai *ai)
{
    const pr_engine *e = bt->engine;
    size_t n = sizeof sn->queue;
    int k = snprintf(sn->queue, n, "QUEUE levels=");
    for (int lv = 0; lv < e->n_levels; lv++) {
        if (lv) k += snprintf(sn->queue + k, n - (size_t)k, "|");
        if (!e->level_len[lv]) k += snprintf(sn->queue + k, n - (size_t)k, "-");
        for (int j = 0; j < e->level_len[lv]; j++) {
            const pr_phase *ph = &e->pool[e->levels[lv][j]];
            if (ph->type == PH_MARKER)
                k += snprintf(sn->queue + k, n - (size_t)k, "%sMarker:%s", j ? "," : "", pr_phase_name[ph->marker_of]);
            else
                k += snprintf(sn->queue + k, n - (size_t)k, "%s%s", j ? "," : "", pr_phase_name[ph->type]);
        }
    }
    for (int side = 0; side < 2; side++) {
        const pr_pokemon *p = bt->field[side * 2];
        char *o = sn->applied[side];
        n = sizeof sn->applied[side];
        k = snprintf(o, n, "APPLIED b=%d aas=", side * 2);
        if (!p->n_ab_applied_summon) k += snprintf(o + k, n - (size_t)k, "-");
        for (int j = 0; j < p->n_ab_applied_summon; j++) k += snprintf(o + k, n - (size_t)k, "%s%d", j ? "," : "", p->ab_applied_summon[j]);
        k += snprintf(o + k, n - (size_t)k, " aaw=");
        if (!p->n_ab_applied_wave) k += snprintf(o + k, n - (size_t)k, "-");
        for (int j = 0; j < p->n_ab_applied_wave; j++) k += snprintf(o + k, n - (size_t)k, "%s%d", j ? "," : "", p->ab_applied_wave[j]);
        const pr_ai_mon *m = pr_ai_mon_of(ai, p);
        snprintf(o + k, n - (size_t)k, " revealed=%d", m && m->ability_revealed ? 1 : 0);
    }
}

/* One expected record against the C result; on a mismatch, what C has (for the report) */
static bool check_line(const char *l, pr_battle *bt, const pr_ai *ai, const snapshot *snap, int next, bool *seen_pre,
                       bool *seen_scores, bool *seen_next, char *c_out, size_t n)
{
    c_out[0] = 0;
    if (!strncmp(l, "CMD ", 4)) {
        int b = (int)inum(l, "b");
        cmd_str(c_out, n, b, &bt->commands[b]);
        return bt->commands[b].command >= 0 && !strcmp(c_out, l);
    }
    if (!strncmp(l, "PRE ", 4)) {
        int b = (int)inum(l, "b");
        if (b == 2) *seen_pre = true;
        snprintf(c_out, n, "PRE b=%d cmd=%d skip=%d", b, bt->pre_commands[b].command, bt->pre_commands[b].skip ? 1 : 0);
        return bt->pre_commands[b].command >= 0 && !strcmp(c_out, l);
    }
    if (!strncmp(l, "AFTER ", 6)) {
        snprintf(c_out, n, "AFTER counter=%d", ai->enemy_switch_counter);
        return !strcmp(c_out, l);
    }
    if (!strncmp(l, "RNG ", 4)) {
        rng_str(c_out, n, bt);
        pr_rng g, b;
        read_rng(need(l, "g"), &g);
        const char *bs = need(l, "b");
        bool live = strcmp(bs, "none") != 0;
        if (live) read_rng(bs, &b);
        return same_rng(&g, &bt->rng) && live == bt->battle_rng_live && (!live || same_rng(&b, &bt->battle_rng));
    }
    if (!strncmp(l, "MQ ", 3)) {
        mq_str(c_out, n, bt->field[2]);
        return !strcmp(c_out, l);
    }
    if (!strncmp(l, "SCORES ", 7)) {
        *seen_scores = true;
        int k = snprintf(c_out, n, "SCORES list=");
        for (int i = 0; i < ai->n_move_scores; i++) k += snprintf(c_out + k, n - (size_t)k, "%s%.17g", i ? "," : "", ai->move_scores[i]);
        if (!ai->n_move_scores) snprintf(c_out + k, n - (size_t)k, "-");
        char sv[8][64];
        int ns = split(need(l, "list"), ',', sv, 8);
        if (ns != ai->n_move_scores) return false;
        for (int i = 0; i < ns; i++)
            if (!same_num(strtod(sv[i], NULL), ai->move_scores[i])) return false;
        return true;
    }
    if (!strncmp(l, "CALLSEL ", 8)) {
        uint16_t mv[8], sel[8];
        int ns = pr_called_selected_list(mv, sel, 8);
        int k = snprintf(c_out, n, "CALLSEL list=");
        if (!ns) snprintf(c_out + k, n - (size_t)k, "-");
        for (int j = 0; j < ns; j++) k += snprintf(c_out + k, n - (size_t)k, "%s%d:%d", j ? "," : "", mv[j], sel[j]);
        return !strcmp(c_out, l);
    }
    if (!strncmp(l, "QUEUE ", 6)) {
        snprintf(c_out, n, "%s", snap->queue);
        return !strcmp(c_out, l);
    }
    if (!strncmp(l, "APPLIED ", 8)) {
        int b = (int)inum(l, "b");
        snprintf(c_out, n, "%s", snap->applied[b == 2]);
        return !strcmp(c_out, l);
    }
    if (!strncmp(l, "NEXT ", 5)) {
        *seen_next = true;
        snprintf(c_out, n, "NEXT slot=%d", next);
        return !strcmp(c_out, l);
    }
    fprintf(stderr, "unknown result record: %s\n", l);
    exit(2);
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "ai.txt", "r");
    if (!f) { perror("ai cases"); return 2; }
    if (!pr_ai_tables_match()) printf("gen/out/ai_data.c doesn't match battle-core's move tables: every score reports it\n");
    static lines_t js;
    static case_state cs;
    static int uncovered_by[PR_AI_FEAT_COUNT_];
    int total = 0, covered = 0, pass = 0, fail = 0, skipped = 0, stat_fail = 0, shown = 0;
    int n_switch = 0, n_fight = 0, n_tera = 0, n_scores = 0, n_next = 0, n_queue = 0, n_tie = 0;
    static char line[LINE_LEN];
    char case_name[64] = "";
    int run_at = -1;
    const char *why = getenv("AITEST_WHY");

    for (;;) {
        char *got = fgets(line, sizeof line, f);
        if (!got) break;
        line[strcspn(line, "\n")] = 0;
        if (!strncmp(line, "CASE ", 5)) {
            snprintf(case_name, sizeof case_name, "%.63s", line);
            js.n = 0;
            run_at = -1;
            continue;
        }
        if (!strcmp(line, "RUN")) { run_at = js.n; continue; }
        if (strncmp(line, "END ", 4)) {
            if (js.n >= MAX_LINES) { fprintf(stderr, "%s: case too long\n", case_name); return 2; }
            snprintf(js.s[js.n++], LINE_LEN, "%s", line);
            continue;
        }
        if (run_at < 0) { fprintf(stderr, "%s: no RUN\n", case_name); return 2; }
        total++;

        memset(&cs, 0, sizeof cs);
        for (int k = 0; k < 4; k++) cs.bt.commands[k].command = cs.bt.pre_commands[k].command = -1;
        pr_status_healed_clear();
        pr_called_selected_reset();
        pr_ai_session_reset();
        read_state(&js, 0, run_at, &cs);
        if (cs.skip || cs.btag) { skipped++; continue; }
        pr_battle *bt = &cs.bt;
        bt->field[0] = &cs.mon[0][0];
        bt->field[2] = &cs.mon[1][0];
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

        pr_ai *ai = &cs.ai;
        /* the game's engine between turns: nothing queued (upstream's queue holds no MovePhase at
         * command time either) */
        static pr_engine eng;
        memset(&eng, 0, sizeof eng);
        eng.n_levels = 1;
        eng.current = -1;
        for (int i = 0; i < 4; i++) eng.command_targets[i] = -1;
        bt->engine = &eng;
        bt->n_unported = bt->n_deferred = 0;
        bool queued = bt->field[2]->n_move_queue > 0;
        pr_ai_enemy_command(bt, ai, 0);
        static snapshot snap;
        take_snapshot(&snap, bt, ai);
        int next = ai->trainer ? pr_ai_next_summon_index(bt, ai, TRAINER_SLOT_TRAINER) : -1;

        if (bt->n_unported || bt->n_deferred) {
            for (int i = 0; why && i < bt->n_unported; i++)
                if (!strcmp(why, pr_ai_unported_name(bt->unported[i]))) printf("WHY %s\n", case_name);
            for (int i = 0; i < bt->n_unported; i++) uncovered_by[bt->unported[i]]++;
            for (int i = 0; i < bt->n_deferred; i++) uncovered_by[bt->deferred[i]]++;
            continue;
        }
        covered++;

        bool ok = true, seen_pre = false, seen_scores = false, seen_next = false;
        static char c_out[LINE_LEN];
        int bad = -1;
        for (int i = run_at; i < js.n && ok; i++)
            if (!check_line(js.s[i], bt, ai, &snap, next, &seen_pre, &seen_scores, &seen_next, c_out, sizeof c_out)) ok = false, bad = i;
        /* what C produced that upstream didn't */
        if (ok && !seen_pre && bt->pre_commands[2].command >= 0) ok = false, snprintf(c_out, sizeof c_out, "PRE b=2 (upstream has none)");
        if (ok && !seen_scores && ai->n_move_scores) ok = false, snprintf(c_out, sizeof c_out, "SCORES (upstream logged none)");
        if (ok && !seen_next && ai->trainer) ok = false, snprintf(c_out, sizeof c_out, "NEXT (upstream has none)");
        if (ok) {
            pass++;
            if (bt->commands[2].command == COMMAND_POKEMON) n_switch++;
            else n_fight++;
            n_tera += bt->pre_commands[2].command == COMMAND_TERA;
            n_scores += ai->n_move_scores > 0;
            n_next += next >= 0;
            n_queue += queued;
            continue;
        }
        fail++;
        if (shown++ < 8) {
            printf("FAIL %s\n", case_name);
            if (getenv("AITEST_STATE"))
                for (int i = 0; i < run_at; i++) printf("  before: %s\n", js.s[i]);
            for (int i = run_at; i < js.n; i++) printf("  JS: %s\n", js.s[i]);
            printf("  C : %s%s\n", c_out, bad < 0 ? "" : "   <- first difference");
        }
    }
    (void)n_tie;

    printf("cases %d (skipped %d: records not read yet), stats mismatches %d\n", total, skipped, stat_fail);
    printf("covered %d: pass %d, fail %d; not covered %d\n", covered, pass, fail, total - skipped - covered);
    printf("passing cases: %d switches, %d moves (%d from the queue, %d scored), %d instant Tera, %d trainer picks\n",
           n_switch, n_fight, n_queue, n_scores, n_tera, n_next);
    int order[PR_AI_FEAT_COUNT_], n = 0;
    for (int i = 0; i < PR_AI_FEAT_COUNT_; i++)
        if (uncovered_by[i]) order[n++] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && uncovered_by[order[j - 1]] < uncovered_by[order[j]]; j--) {
            int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
        }
    if (n) printf("not covered, by reason (a case can have several):\n");
    for (int i = 0; i < n && i < 40; i++) printf("  %6d  %s\n", uncovered_by[order[i]], pr_ai_unported_name((uint16_t)order[i]));
    return fail ? 1 : 0;
}
