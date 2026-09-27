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

#define MAX_LINES 128
#define LINE_LEN 2048
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
    pr_pokemon mon[2];         /* [0] the player's (A, battler 0), [1] the enemy (D, battler 2) */
    int skip;                  /* the case has records this test doesn't read (modifiers) */
    int btag;                  /* battler tags: only their type, turns, source, stockpile and crit stages are in
                                * the record, which is all the condition probes read */
} case_state;

static pr_pokemon *mon_for(case_state *cs, int b)
{
    if (b == 0) return &cs->mon[0];
    if (b == 2) return &cs->mon[1];
    fprintf(stderr, "battler %d in a singles case\n", b);
    exit(2);
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
        } else if (!strncmp(l, "ARENA ", 6)) {
            bt->weather = (uint8_t)inum(l, "weather");
            bt->weather_turns_left = (int16_t)inum(l, "wturns");
            bt->weather_max_duration = (int16_t)inum(l, "wmax");
            bt->terrain = (uint8_t)inum(l, "terrain");
            bt->terrain_turns_left = (int16_t)inum(l, "tturns");
            bt->terrain_max_duration = (int16_t)inum(l, "tmax");
            bt->ignore_abilities = inum(l, "ignab") != 0;
            bt->ignoring_effect_source = (int8_t)unum(l, "ignsrc", -1);
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
        } else if (!strncmp(l, "MON ", 4)) {
            pr_pokemon *p = mon_for(cs, (int)inum(l, "b"));
            memset(p, 0, sizeof *p);
            p->is_player = inum(l, "b") < 2;
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
            p->switch_out_status = inum(l, "out") != 0;
            p->on_field = inum(l, "onfield") != 0 || p->switch_out_status;
            p->summon_hit_count = (uint8_t)inum(l, "shc");
            int nt = split(need(l, "types"), ',', it, 3);
            if (nt > 2) { cs->skip = 1; nt = 2; }
            p->type_override[0] = p->type_override[1] = -2;
            for (int j = 0; j < nt; j++) p->type_override[j] = (int8_t)atoi(it[j]);
            if (nt == 1) p->type_override[1] = -1;
            p->added_type = field(l, "added") ? (int8_t)inum(l, "added") : 0;
            p->ability_suppressed = inum(l, "abilsup") != 0;
            p->temp_turn_count = (int32_t)inum(l, "tsd");
            p->temp_wave_turn_count = (int32_t)inum(l, "tswd");
            p->n_ab_applied_summon = (uint8_t)split(need(l, "aas"), ',', it, 4);
            for (int j = 0; j < p->n_ab_applied_summon; j++) p->ab_applied_summon[j] = (uint16_t)atoi(it[j]);
            p->n_ab_applied_wave = (uint8_t)split(need(l, "aaw"), ',', it, 4);
            for (int j = 0; j < p->n_ab_applied_wave; j++) p->ab_applied_wave[j] = (uint16_t)atoi(it[j]);
            p->has_trainer = false;
            p->turn_move_effectiveness = -1;
        } else if (!strncmp(l, "TURN ", 5)) {
            pr_pokemon *p = mon_for(cs, (int)inum(l, "b"));
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
            pr_pokemon *p = mon_for(cs, (int)inum(l, "b"));
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
            pr_pokemon *p = mon_for(cs, (int)inum(l, "b"));
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
            pr_pokemon *p = mon_for(cs, (int)inum(l, "b"));
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
            /* the line holds all of a ported tag type's state, and of Protosynthesis's and Quark
             * Drive's (which only the damage path reads and a weather or terrain ending removes);
             * the others can hold more */
            if (!pr_tag_type_ported(type) && !(pr_battler_tag_is(type, PR_TAGCLASS_HIGHEST_STAT_BOOST) && field(l, "stat")))
                cs->btag = 1;
        } else if (!strncmp(l, "MOD ", 4)) {
            cs->skip = 1;
        } else if (!strncmp(l, "PARTY ", 6)) {
            int side = (int)inum(l, "side");
            static char ms[PR_MAX_PARTY + 1][64];
            int n = split(need(l, "list"), ',', ms, PR_MAX_PARTY + 1);
            if (side < 0 || side > 1 || n > PR_MAX_PARTY) { cs->skip = 1; continue; }
            bt->n_party[side] = (uint8_t)n;
            for (int j = 0; j < n; j++) {
                char f[4][64];
                split(ms[j], ':', f, 4);
                bt->party[side][j].id = (uint32_t)strtoul(f[0], NULL, 10);
                bt->party[side][j].species = (uint16_t)atoi(f[1]);
                bt->party[side][j].status = (uint8_t)atoi(f[2]);
                bt->party[side][j].hp = atoi(f[3]);
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
        } else if (!strncmp(l, "SESSION ", 8)) {
            char hs[16][64];
            int n = split(need(l, "healed"), ',', hs, 16);
            for (int j = 0; j < n; j++) {
                char *c = strchr(hs[j], ':');
                pr_status_healed_set((uint16_t)atoi(hs[j]), c ? atoi(c + 1) : 0);
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
    if (t->type == BATTLER_TAG_TYPE_SEEDED) {                 /* SeedTag.sourceIndex */
        snprintf(buf, sizeof buf, " sidx=%d", t->source_index);
        return buf;
    }
    if (!pr_battler_tag_is(t->type, PR_TAGCLASS_HIGHEST_STAT_BOOST)) return "";
    snprintf(buf, sizeof buf, " stat=%d mult=%.17g", t->stat, t->multiplier);
    return buf;
}

static void dump_mon(lines_t *L, const pr_battle *bt, const pr_pokemon *p, int b, bool *incomplete)
{
    sbuf s = {{0}, 0};
    sb(&s, "MON b=%d id=%u sp=%d form=%d lv=%d ivs=", b, p->id, p->species, p->form, p->level);
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
    sb(&s, " boss=%d onfield=%d out=%d shc=%d types=", p->is_boss, p->on_field && !p->switch_out_status,
       p->switch_out_status, p->summon_hit_count);
    int nt = p->type_override[0] == -2 ? 0 : p->type_override[1] == -1 ? 1 : 2;
    LIST(&s, nt, ',', "%d", p->type_override[i]);
    if (has_money) sb(&s, " added=%d", p->added_type);
    sb(&s, " abilsup=%d tsd=%d tswd=%d aas=", p->ability_suppressed, p->temp_turn_count, p->temp_wave_turn_count);
    LIST(&s, p->n_ab_applied_summon, ',', "%d", p->ab_applied_summon[i]);
    sb(&s, " aaw=");
    LIST(&s, p->n_ab_applied_wave, ',', "%d", p->ab_applied_wave[i]);
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "TURN b=%d acted=%d order=", b, p->acted);
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
    sb(&s, "MOVES b=%d list=", b);
    for (int i = 0; i < p->n_moves; i++) {
        if (i) sb(&s, ",");
        sb(&s, "%d:%d:%d:", p->moves[i], p->pp_used[i], p->pp_up[i]);
        num_or_u(&s, p->max_pp_override[i], -1);
    }
    if (!p->n_moves) sb(&s, "-");
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "HIST b=%d list=", b);
    if (p->n_move_history > PR_MAX_MOVE_HISTORY) *incomplete = true;
    int kept = p->n_move_history < PR_MAX_MOVE_HISTORY ? p->n_move_history : PR_MAX_MOVE_HISTORY;
    for (int i = 0; i < kept; i++) {
        if (i) sb(&s, ",");
        turn_move_str(&s, &p->move_history[i]);
    }
    if (!kept) sb(&s, "-");
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "MQ b=%d list=", b);
    for (int i = 0; i < p->n_move_queue; i++) {
        if (i) sb(&s, ",");
        turn_move_str(&s, &p->move_queue[i]);
    }
    if (!p->n_move_queue) sb(&s, "-");
    add_line(L, "%s", s.s);

    for (int i = 0; i < p->n_tags; i++)
        add_line(L, "BTAG b=%d type=%s turns=%d smove=%d sid=%lld stock=%d crit=%d auto=%d%s", b,
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
    LIST(&s, bt->n_participants, ',', "%u", bt->participants[i]);
    sb(&s, " loot=%d score=%.17g", bt->post_battle_loot, bt->battle_score);
    if (has_money) sb(&s, " money=%d", bt->money_scattered);
    sb(&s, " type=%d double=%d mel=%d bar=%d ball=", bt->battle_type, bt->double_battle, bt->max_exp_level,
       bt->ability_bar_visible);
    num_or_u(&s, bt->last_used_pokeball, -1);
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "ARENA weather=%d wturns=%d wmax=%d terrain=%d tturns=%d tmax=%d ignab=%d ignsrc=", bt->weather,
       bt->weather ? bt->weather_turns_left : 0, bt->weather ? bt->weather_max_duration : 0, bt->terrain,
       bt->terrain ? bt->terrain_turns_left : 0, bt->terrain ? bt->terrain_max_duration : 0, bt->ignore_abilities);
    num_or_u(&s, bt->ignoring_effect_source, -1);
    add_line(L, "%s", s.s);
    for (int i = 0; i < bt->n_arena_tags; i++) {
        const pr_arena_tag *t = &bt->arena_tags[i];
        char layers[16] = "";
        if (pr_arena_tag_is(t->type, PR_ARENACLASS_ENTRY_HAZARD)) snprintf(layers, sizeof layers, " layers=%d", t->layers);
        add_line(L, "ATAG type=%s side=%d turns=%d max=%d smove=%d sid=%lld%s", arena_tag_names[t->type], t->side,
                 t->turn_count, t->max_duration, t->source_move, (long long)t->source_id, layers);
    }
    dump_mon(L, bt, &cs->mon[0], 0, incomplete);
    dump_mon(L, bt, &cs->mon[1], 2, incomplete);
    for (int side = 0; side < 2; side++) {
        if (!bt->n_party[side]) continue;
        s.n = 0;
        sb(&s, "PARTY side=%d list=", side);
        for (int i = 0; i < bt->n_party[side]; i++) {
            const pr_party_member *m = &bt->party[side][i];
            int st = m->status, hp = m->hp;
            /* the Pokemon on the field: its live state */
            for (int k = 0; k < 2; k++)
                if (cs->mon[k].id == m->id) { st = cs->mon[k].status; hp = cs->mon[k].hp; }
            if (i) sb(&s, ",");
            sb(&s, "%u:%d:%d:%d", m->id, m->species, st, hp);
        }
        add_line(L, "%s", s.s);
    }
    for (int k = 0; k < 4; k++) {
        const struct pr_turn_command *c = &bt->commands[k];
        if (c->command >= 0) {
            s.n = 0;
            sb(&s, "CMD b=%d cmd=%d move=%d targets=", k, c->command, c->move);
            LIST(&s, c->n_targets, ',', "%d", c->targets[i]);
            sb(&s, " mode=%d skip=%d", c->use_mode, c->skip);
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
}

static void dump_trace_queue(lines_t *L, const pr_engine *e)
{
    sbuf s = {{0}, 0};
    sb(&s, "TRACE list=");
    LIST(&s, e->n_trace, ',', "%s", e->trace[i]);
    add_line(L, "%s", s.s);

    s.n = 0;
    sb(&s, "QUEUE levels=");
    for (int lv = 0; lv < e->n_levels; lv++) {
        if (lv) sb(&s, "|");
        if (!e->level_len[lv]) sb(&s, "-");
        for (int k = 0; k < e->level_len[lv]; k++) {
            const pr_phase *p = &e->pool[e->levels[lv][k]];
            if (k) sb(&s, ",");
            if (p->type == PH_MARKER) sb(&s, "Marker:%s", pr_phase_name[p->marker_of]);
            else sb(&s, "%s", pr_phase_name[p->type]);
        }
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
static bool is_probe_line(const char *l) { return !strncmp(l, "PROBE ", 6) || !strncmp(l, "SPROBE ", 7); }

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
        bool sprobe = l[0] == 'S';
        pr_pokemon *u = sprobe ? NULL : bt->field[inum(l, "u")], *t = sprobe ? NULL : bt->field[inum(l, "t")];
        int move = sprobe ? -1 : (int)inum(l, "move"), seq = sprobe ? -1 : (int)inum(l, "seq");
        bt->n_unported = bt->n_deferred = 0;
        static uint32_t hits_before[PR_CLOSURE_COUNT];
        memcpy(hits_before, pr_closure_hits, sizeof hits_before);
        bool res;
        if (sprobe) {
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
    char case_name[64] = "";
    int run_at = -1;

    for (;;) {
        char *got = fgets(line, sizeof line, f);
        if (got) line[strcspn(line, "\n")] = 0;
        if (got && line[0] == '#') { printf("%s\n", line); continue; }
        if (got && strncmp(line, "END ", 4)) {
            if (!strncmp(line, "CASE ", 5)) {
                snprintf(case_name, sizeof case_name, "%s", line);
                js.n = 0;
                run_at = -1;
                continue;
            }
            if (!strcmp(line, "RUN")) { run_at = js.n; continue; }
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
        read_state(&js, 0, run_at, &cs);
        bool probe = run_at < js.n && is_probe_line(js.s[run_at]);
        if (cs.skip || (cs.btag && !probe)) { skipped++; continue; }
        pr_battle *bt = &cs.bt;
        bt->field[0] = &cs.mon[0];
        bt->field[2] = &cs.mon[1];
        for (int k = 0; k < 2; k++) {
            pr_pokemon *p = &cs.mon[k];
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
        dump_state(&c_before, bt, &cs, &incomplete);
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
        static uint32_t hits_before[PR_CLOSURE_COUNT];
        memcpy(hits_before, pr_closure_hits, sizeof hits_before);
        pr_run_turn(bt, &eng);
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
        if (getenv("TURNTEST_COVERED")) printf("COVERED %s\n", case_name);   /* which cases were covered */
        count_phases(&eng);
        for (int i = 0; i < PR_CLOSURE_COUNT; i++) covered_hits[i] += pr_closure_hits[i] - hits_before[i];
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
    fail += probes.fail;
    (void)cs0;
    return fail || parse_fail ? 1 : 0;
}
