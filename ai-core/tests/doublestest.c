#define _POSIX_C_SOURCE 200809L
/* Task 74: the C half of ai-core's doubles suite. It replays the cases doubles.producer.test.ts
 * wrote inside upstream: a double battle's state before the enemy commands, then, per enemy that
 * TurnInitPhase gives an EnemyCommandPhase to (field index 0 then 1), what upstream's phase left.
 *
 * The established singles comparator (tests/aitest.c) stays byte-for-byte unchanged: this file
 * includes it (its main renamed) and reuses its record reader (read_state) and its record
 * comparison (check_line) for every record kind that is the same in doubles: CMD, PRE, AFTER, RNG,
 * SCORES, CALLSEL, QUEUE and the enemy move queue. Four records are new or keyed differently in
 * doubles and are compared here, in the same shape and with the same strictness: STEP (which
 * enemy's block follows), MQ for battler 3, APPLIED for all four field Pokemon, and MATCHUP /
 * NEXT per trainer slot. Nothing is compared more loosely than in singles.
 *
 * The file must carry its contract: "# AI_DOUBLES_CONTRACT 1 <N>" first and a matching
 * "# AI_DOUBLES_COMPLETE <written> <threw>" last (written = cases read, written + threw = N), else
 * this program exits 2 without a verdict. It also fails (exit 1) unless the compared commands
 * exercise what the card asks for: a command aimed at the user's ally, a command with two or more
 * targets, a command aimed at both opponents, a case with a Tatsugiri commanding (the COMMANDED
 * tag), a case with a fainted Pokemon in a field slot, and, in a trainer battle, switch-in scores
 * for both of a double trainer's slots.
 *
 * usage: doublestest <trace>            (tools/doublesdiff.sh)
 *   AIDBL_WHY=<reason> lists the cases a not-covered reason left out, AITEST_STATE=1 prints a
 *   failing case's state, AIDBL_COVERED=1 lists the compared case numbers. */
#include <stdarg.h>
#include <stdio.h>
#define main established_ai_main
#include "aitest.c"
#undef main

/* ---- the doubles-only records ---- */

/* APPLIED b=<battler>: the same string aitest.c's take_snapshot builds, for any field slot */
static void dbl_applied_str(char *o, size_t n, const pr_battle *bt, pr_ai *ai, int b)
{
    const pr_pokemon *p = bt->field[b];
    int k = snprintf(o, n, "APPLIED b=%d aas=", b);
    if (!p) { snprintf(o + k, n - (size_t)k, "- aaw=- revealed=0"); return; }
    if (!p->n_ab_applied_summon) k += snprintf(o + k, n - (size_t)k, "-");
    for (int j = 0; j < p->n_ab_applied_summon; j++)
        k += snprintf(o + k, n - (size_t)k, "%s%d", j ? "," : "", p->ab_applied_summon[j]);
    k += snprintf(o + k, n - (size_t)k, " aaw=");
    if (!p->n_ab_applied_wave) k += snprintf(o + k, n - (size_t)k, "-");
    for (int j = 0; j < p->n_ab_applied_wave; j++)
        k += snprintf(o + k, n - (size_t)k, "%s%d", j ? "," : "", p->ab_applied_wave[j]);
    const pr_ai_mon *m = pr_ai_mon_of(ai, p);
    snprintf(o + k, n - (size_t)k, " revealed=%d", m && m->ability_revealed ? 1 : 0);
}

/* the trainer's switch-in scores and faint pick, per trainer slot (upstream runs them for
 * TRAINER and TRAINER_PARTNER in a double battle) */
typedef struct {
    int n, slot[PR_MAX_PARTY];
    double score[PR_MAX_PARTY];
    int next;
} slot_scores;

/* ---- the state this file keeps per case ---- */

typedef struct {
    char queue[LINE_LEN];              /* aitest.c's take_snapshot, for the QUEUE record */
    char applied[4][LINE_LEN];
    slot_scores ts[2];                 /* [0] TRAINER, [1] TRAINER_PARTNER */
} dbl_snapshot;

static int dbl_step;                   /* the field index whose block is being compared */
static bool dbl_seen_step[2];
static bool dbl_seen_scores[2], dbl_seen_pre[2];

/* One expected record against the C result, for the kinds doubles keys differently; the rest goes
 * to the established comparator unchanged. */
static bool dbl_check_line(const char *l, pr_battle *bt, const pr_ai *ai, const dbl_snapshot *sn,
                           const snapshot *base, char *c_out, size_t n)
{
    c_out[0] = 0;
    if (!strncmp(l, "STEP ", 5)) {
        int f = (int)inum(l, "f");
        snprintf(c_out, n, "STEP f=%d", dbl_step);
        return f == dbl_step;
    }
    if (!strncmp(l, "MQ b=3", 6)) {
        mq_str(c_out, n, bt->field[3]);                      /* prints b=2; this record is b=3 */
        char *b = strstr(c_out, "b=");
        if (b) b[2] = '3';
        return !strcmp(c_out, l);
    }
    if (!strncmp(l, "APPLIED ", 8)) {
        int b = (int)inum(l, "b");
        if (b < 0 || b > 3) { fprintf(stderr, "APPLIED b=%d\n", b); exit(2); }
        snprintf(c_out, n, "%s", sn->applied[b]);
        return !strcmp(c_out, l);
    }
    if (!strncmp(l, "MATCHUP ", 8)) {
        int ts = (int)inum(l, "ts");
        const slot_scores *s = &sn->ts[ts == TRAINER_SLOT_TRAINER_PARTNER];
        int k = snprintf(c_out, n, "MATCHUP ts=%d list=", ts);
        if (!s->n) snprintf(c_out + k, n - (size_t)k, "-");
        for (int i = 0; i < s->n; i++)
            k += snprintf(c_out + k, n - (size_t)k, "%s%d:%.17g", i ? "," : "", s->slot[i], s->score[i]);
        char sv[PR_MAX_PARTY + 2][64];
        int ns = split(need(l, "list"), ',', sv, PR_MAX_PARTY + 2);
        if (ns != s->n) return false;
        for (int i = 0; i < ns; i++) {
            char *colon = strchr(sv[i], ':');
            if (!colon || atoi(sv[i]) != s->slot[i] || !same_num(strtod(colon + 1, NULL), s->score[i])) return false;
        }
        return true;
    }
    if (!strncmp(l, "NEXT ", 5)) {
        int ts = (int)inum(l, "ts");
        const slot_scores *s = &sn->ts[ts == TRAINER_SLOT_TRAINER_PARTNER];
        snprintf(c_out, n, "NEXT ts=%d slot=%d", ts, s->next);
        return !strcmp(c_out, l);
    }
    /* the established comparator for everything else; its own "upstream has none" checks are done
     * per step below, so the flags it takes are this file's */
    bool pre = false, scores = false, next = false;
    bool ok = check_line(l, bt, ai, base, 0, &pre, &scores, &next, c_out, n);
    if (!strncmp(l, "PRE ", 4)) dbl_seen_pre[dbl_step] = true;   /* check_line flags b=2 only */
    if (scores) dbl_seen_scores[dbl_step] = true;
    return ok;
}

/* Whether a case's battler tags are all ones the AI reads in full. aitest.c's reader sets cs.btag
 * for a tag type whose record doesn't hold all its state; in doubles the producer also plants
 * Commander's COMMANDED tag, whose extra state is the Tatsugiri's cached form key, which no AI
 * path reads (the AI reads the tag's presence and its source), the same exception aitest.c already
 * makes for Imprison. Re-scanned here so the established reader stays unchanged. */
static bool tags_readable(const lines_t *L, int to)
{
    for (int i = 0; i < to; i++) {
        if (strncmp(L->s[i], "BTAG ", 5)) continue;
        const char *tn = need(L->s[i], "type");
        int type = name_index(battler_tag_names, BATTLER_TAG_TYPE_COUNT_, tn);
        if (type < 0) { fprintf(stderr, "unknown battler tag %s\n", tn); exit(2); }
        if (pr_tag_type_ported(type)) continue;
        if (pr_battler_tag_is(type, PR_TAGCLASS_HIGHEST_STAT_BOOST) && field(L->s[i], "stat")) continue;
        if (type == BATTLER_TAG_TYPE_IMPRISON || type == BATTLER_TAG_TYPE_COMMANDED) continue;
        return false;
    }
    return true;
}

int main(int argc, char **argv)
{
    FILE *f = fopen(argc > 1 ? argv[1] : "ai-doubles.txt", "r");
    if (!f) { perror("ai doubles cases"); return 2; }
    if (!pr_ai_tables_match()) printf("gen/out/ai_data.c doesn't match battle-core's move tables: every score reports it\n");
    static lines_t js;
    static case_state cs;
    static int uncovered_by[PR_AI_FEAT_COUNT_];
    int total = 0, covered = 0, pass = 0, fail = 0, skipped = 0, stat_fail = 0, shown = 0;
    int n_switch = 0, n_fight = 0, n_scores = 0, n_queue = 0;
    /* what the compared commands exercised */
    int x_ally = 0, x_multi = 0, x_both = 0, x_commanded = 0, x_fainted = 0, x_partner = 0, x_double_trainer = 0;
    static char line[LINE_LEN];
    char case_name[64] = "";
    int case_no = -1, run_at = -1;
    int contract = 0, want = 0, said_written = -1, said_threw = -1;
    bool first = true;
    const char *why = getenv("AIDBL_WHY");
    const char *list_covered = getenv("AIDBL_COVERED");

    for (;;) {
        char *got = fgets(line, sizeof line, f);
        if (!got) break;
        line[strcspn(line, "\n")] = 0;
        if (first) {
            if (sscanf(line, "# AI_DOUBLES_CONTRACT %d %d", &contract, &want) != 2 || contract != 1 || want <= 0) {
                fprintf(stderr, "the trace does not start with a known AI_DOUBLES_CONTRACT header\n");
                return 2;
            }
            first = false;
            continue;
        }
        if (line[0] == '#') {
            sscanf(line, "# AI_DOUBLES_COMPLETE %d %d", &said_written, &said_threw);
            continue;
        }
        if (!strncmp(line, "CASE ", 5)) {
            snprintf(case_name, sizeof case_name, "%.63s", line);
            case_no = atoi(line + 5);
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
        if (cs.skip || (cs.btag && !tags_readable(&js, run_at))) { skipped++; continue; }
        pr_battle *bt = &cs.bt;
        /* the AIMON record's slot= is upstream's EnemyPokemon.trainerSlot; aitest.c's reader keeps it
         * in the AI's table only, and battle-core's switch-out condition reads its own copy
         * (pr_pokemon.trainer_slot). In singles every trainer Pokemon has one slot, so the copy
         * never mattered there; a double trainer splits its Pokemon between two */
        for (int side = 0; side < 2; side++)
            for (int k = 0; k < bt->n_party[side]; k++) {
                const pr_ai_mon *m = pr_ai_mon_of(&cs.ai, &cs.mon[side][k]);
                if (m && !cs.mon[side][k].is_player) cs.mon[side][k].trainer_slot = (int8_t)m->trainer_slot;
            }
        if (!bt->double_battle) { fprintf(stderr, "%s: not a double battle\n", case_name); return 2; }
        if (bt->n_party[0] < 2 || bt->n_party[1] < 2) { fprintf(stderr, "%s: a side has one Pokemon\n", case_name); return 2; }
        /* upstream's field in a double battle: party.slice(0, 2) on each side, whether or not a
         * Pokemon there has fainted and left */
        for (int side = 0; side < 2; side++)
            for (int k = 0; k < 2; k++) bt->field[side * 2 + k] = &cs.mon[side][k];
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
        static pr_engine eng;
        memset(&eng, 0, sizeof eng);
        eng.n_levels = 1;
        eng.current = -1;
        for (int i = 0; i < 4; i++) eng.command_targets[i] = -1;
        bt->engine = &eng;
        bt->n_unported = bt->n_deferred = 0;
        memset(dbl_seen_step, 0, sizeof dbl_seen_step);
        memset(dbl_seen_scores, 0, sizeof dbl_seen_scores);
        memset(dbl_seen_pre, 0, sizeof dbl_seen_pre);

        /* run the commands and compare each enemy's block before the next one runs, as the turn
         * does: upstream's TurnInitPhase queues an EnemyCommandPhase for each enemy that is allowed
         * in battle, field index 0 then 1 */
        bool ok = true, queued_any = false;
        int bad = -1;
        static char c_out[LINE_LEN];
        int at = run_at;
        for (int fi = 0; fi < 2 && ok; fi++) {
            pr_pokemon *e = bt->field[2 + fi];
            bool active = e && e->hp > 0;
            bool has_block = at < js.n && !strncmp(js.s[at], "STEP ", 5) && (int)inum(js.s[at], "f") == fi;
            if (active != has_block) {
                ok = false;
                snprintf(c_out, sizeof c_out, "STEP f=%d %s in C, %s in upstream's trace", fi,
                         active ? "run" : "skipped", has_block ? "run" : "skipped");
                bad = at;
                break;
            }
            if (!active) continue;
            dbl_step = fi;
            dbl_seen_step[fi] = true;
            queued_any |= e->n_move_queue > 0;
            ai->n_move_scores = 0;   /* the producer resets its log per phase */
            pr_ai_enemy_command(bt, ai, fi);
            if (bt->n_unported || bt->n_deferred) break;   /* not covered: judged below */
            /* the block's records, up to the next STEP or the tail (CALLSEL) */
            int end = at + 1;
            while (end < js.n && strncmp(js.s[end], "STEP ", 5) && strncmp(js.s[end], "CALLSEL ", 8)) end++;
            for (int i = at; i < end && ok; i++) {
                static snapshot base;                       /* only QUEUE/APPLIED read it; not in a block */
                static dbl_snapshot sn;
                if (!dbl_check_line(js.s[i], bt, ai, &sn, &base, c_out, sizeof c_out)) ok = false, bad = i;
            }
            if (ok && !dbl_seen_pre[fi] && bt->pre_commands[2 + fi].command >= 0)
                ok = false, snprintf(c_out, sizeof c_out, "PRE b=%d (upstream has none)", 2 + fi);
            if (ok && !dbl_seen_scores[fi] && ai->n_move_scores)
                ok = false, snprintf(c_out, sizeof c_out, "SCORES (upstream logged none) for f=%d", fi);
            at = end;
        }

        if (bt->n_unported || bt->n_deferred) {
            for (int i = 0; why && i < bt->n_unported; i++)
                if (!strcmp(why, pr_ai_unported_name(bt->unported[i]))) printf("WHY %s\n", case_name);
            for (int i = 0; i < bt->n_unported; i++) uncovered_by[bt->unported[i]]++;
            for (int i = 0; i < bt->n_deferred; i++) uncovered_by[bt->deferred[i]]++;
            continue;
        }
        covered++;
        if (list_covered) printf("COVERED CASE %d\n", case_no);

        /* the tail: what the choices left behind, then the trainer's switch-in scores per slot
         * (upstream writes the first before working out the second, which scores and applies too) */
        static dbl_snapshot sn;
        static snapshot base;
        if (ok) {
            take_snapshot(&base, bt, ai);
            snprintf(sn.queue, sizeof sn.queue, "%s", base.queue);
            for (int b = 0; b < 4; b++) dbl_applied_str(sn.applied[b], LINE_LEN, bt, ai, b);
            const int slots[2] = {TRAINER_SLOT_TRAINER, TRAINER_SLOT_TRAINER_PARTNER};
            for (int s = 0; s < 2; s++) {
                sn.ts[s].n = pr_ai_party_matchup_scores(bt, ai, slots[s], true, sn.ts[s].slot, sn.ts[s].score);
                sn.ts[s].next = ai->trainer ? pr_ai_next_summon_index(bt, ai, slots[s]) : -1;
            }
            bool seen_match[2] = {false, false};
            for (int i = at; i < js.n && ok; i++) {
                if (!strncmp(js.s[i], "MATCHUP ", 8)) seen_match[(int)inum(js.s[i], "ts") == TRAINER_SLOT_TRAINER_PARTNER] = true;
                if (!dbl_check_line(js.s[i], bt, ai, &sn, &base, c_out, sizeof c_out)) ok = false, bad = i;
            }
            if (ok && !seen_match[0] && ai->trainer)
                ok = false, snprintf(c_out, sizeof c_out, "MATCHUP (upstream has none)");
        }

        if (ok) {
            pass++;
            for (int fi = 0; fi < 2; fi++) {
                if (!dbl_seen_step[fi]) continue;
                const struct pr_turn_command *c = &bt->commands[2 + fi];
                if (c->command == COMMAND_POKEMON) { n_switch++; continue; }
                n_fight++;
                int self = 2 + fi, ally = self ^ 1;
                bool both = c->n_targets >= 2;
                for (int t = 0; t < c->n_targets; t++) x_ally += c->targets[t] == ally;
                x_multi += both;
                int opp = 0;
                for (int t = 0; t < c->n_targets; t++) opp += c->targets[t] == 0 || c->targets[t] == 1;
                x_both += opp == 2;
            }
            n_scores += dbl_seen_scores[0] || dbl_seen_scores[1];
            n_queue += queued_any;
            for (int b = 0; b < 4; b++) {
                const pr_pokemon *p = bt->field[b];
                if (!p) continue;
                x_commanded += pr_get_battler_tag(p, BATTLER_TAG_TYPE_COMMANDED) != NULL;
                x_fainted += p->hp <= 0;
            }
            if (ai->trainer && ai->trainer->is_double) {
                x_double_trainer++;
                x_partner += sn.ts[1].n > 0;
            }
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

    if (said_written < 0 || said_threw < 0) {
        fprintf(stderr, "the trace has no AI_DOUBLES_COMPLETE footer: it is truncated\n");
        return 2;
    }
    if (said_written != total || said_written + said_threw != want) {
        fprintf(stderr, "the trace is incomplete: %d cases read, footer says %d written and %d threw of %d\n",
                total, said_written, said_threw, want);
        return 2;
    }

    printf("cases %d (skipped %d: records not read yet), stats mismatches %d\n", total, skipped, stat_fail);
    printf("covered %d: pass %d, fail %d; not covered %d\n", covered, pass, fail, total - skipped - covered);
    printf("passing cases: %d switches, %d moves (%d had a queued move, %d scored)\n",
           n_switch, n_fight, n_queue, n_scores);
    printf("doubles in the passing cases: %d commands aimed at the ally, %d with two or more targets,"
           " %d at both opponents\n", x_ally, x_multi, x_both);
    printf("  %d Pokemon commanding (COMMANDED), %d fainted in a field slot,"
           " %d double trainers (%d with partner-slot switch-in scores)\n",
           x_commanded, x_fainted, x_double_trainer, x_partner);
    int order[PR_AI_FEAT_COUNT_], n = 0;
    for (int i = 0; i < PR_AI_FEAT_COUNT_; i++)
        if (uncovered_by[i]) order[n++] = i;
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && uncovered_by[order[j - 1]] < uncovered_by[order[j]]; j--) {
            int t = order[j]; order[j] = order[j - 1]; order[j - 1] = t;
        }
    if (n) printf("not covered, by reason (a case can have several):\n");
    for (int i = 0; i < n && i < 40; i++) printf("  %6d  %s\n", uncovered_by[order[i]], pr_ai_unported_name((uint16_t)order[i]));

    /* the card's exercise gates: a doubles suite that reached none of these proves nothing */
    struct { int v; const char *what; } gates[] = {
        {x_ally, "a command aimed at the user's ally"},
        {x_multi, "a command with two or more targets"},
        {x_both, "a command aimed at both opponents"},
        {x_commanded, "a Pokemon commanding its ally (COMMANDED)"},
        {x_fainted, "a fainted Pokemon in a field slot"},
        {x_partner, "switch-in scores for a double trainer's partner slot"},
    };
    int missing = 0;
    for (unsigned i = 0; i < sizeof gates / sizeof *gates; i++)
        if (!gates[i].v) { printf("NOT EXERCISED: %s\n", gates[i].what); missing++; }
    return fail || missing ? 1 : 0;
}
