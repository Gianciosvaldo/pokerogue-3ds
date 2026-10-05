/* Task 73: the doubles turn suite. A wrapper over the established turn comparator, which stays
 * byte-for-byte unchanged: this file adds the trace's contract check, puts the second Pokemon of
 * each side on the field (party slot 1, as upstream's getPlayerField / getEnemyField in a double
 * battle), and checks the turns that were compared exercised what the card asks for: a move aimed
 * at the ally, a move hitting both opponents, and two Pokemon fainting in the same turn.
 *
 * usage: TURNTEST_LOCALES=<upstream>/locales/en doublestest <trace>   (tools/doublesdiff.sh) */
#define _POSIX_C_SOURCE 200809L   /* setenv */
#include <stdarg.h>
#include <stdio.h>
#include "pr/engine.h"
static int guarded_printf(const char *format, ...);
static void doubles_run_turn(pr_battle *bt, pr_engine *e);
static int message_cases, message_same, message_differ, message_unported;
static int reported_stats, reported_parse, reported_pass, reported_fail, reported_covered;
#define printf guarded_printf
#define main established_turn_main
#define pr_run_turn doubles_run_turn
#include "turntest.c"
#undef pr_run_turn
#undef main
#undef printf

#define MAX_CASES 100000
static unsigned char *covered_case;   /* by case number: the comparator compared it */

static int guarded_printf(const char *format, ...)
{
    char output[65536];
    va_list args; va_start(args, format);
    int n = vsnprintf(output, sizeof output, format, args);
    va_end(args);
    int id;
    if (sscanf(output, "COVERED CASE %d", &id) == 1) {
        if (id >= 0 && id < MAX_CASES) covered_case[id] = 1;
        if (getenv("DOUBLES_COVERED")) fputs(output, stdout);   /* opt-in: list them */
        return n;                     /* the list is for the counters below, not the report */
    }
    if (!strncmp(output, "texts of covered turns ", 23))
        sscanf(output, "texts of covered turns %d: identical %d, differ %d, a message with no text yet %d",
               &message_cases, &message_same, &message_differ, &message_unported);
    if (!strncmp(output, "cases ", 6)) {
        int total, skipped;
        sscanf(output, "cases %d (skipped %d: records not read yet), parse mismatches %d, stats mismatches %d",
               &total, &skipped, &reported_parse, &reported_stats);
    }
    if (!strncmp(output, "covered ", 8))
        sscanf(output, "covered %d: pass %d, fail %d", &reported_covered, &reported_pass, &reported_fail);
    fputs(output, stdout);
    return n;
}

/* upstream's field in a double battle: party.slice(0, 2) on each side */
static void doubles_run_turn(pr_battle *bt, pr_engine *e)
{
    if (bt->double_battle) {
        bt->field[1] = bt->n_party[0] > 1 ? bt->party[0][1] : NULL;
        bt->field[3] = bt->n_party[1] > 1 ? bt->party[1][1] : NULL;
    }
    pr_run_turn(bt, e);
}

/* what a case exercised, read from its records */
typedef struct {
    int ally_cmd;        /* a FIGHT command aimed at the user's ally alone */
    int both_cmd;        /* a FIGHT command aimed at both opponents */
    int ally_hit;        /* a Pokemon was hit by its ally's move (recv source on the same side) */
    int both_hit;        /* one Pokemon's move hit both opponents this turn */
    int faints;          /* FaintPhase in the trace */
    int faint_sides[2];
    int one_move_faints; /* two or more FaintPhases between one MoveEffectPhase and its MoveEndPhase */
} exercise;

static int battler_side(int b) { return b >= 2; }

static void scan_case(char lines[][LINE_LEN], int n, exercise *x)
{
    memset(x, 0, sizeof *x);
    int run = -1;
    for (int i = 0; i < n; i++) if (!strcmp(lines[i], "RUN")) { run = i; break; }
    for (int i = 0; i < n; i++) {
        const char *l = lines[i];
        if (i < run && !strncmp(l, "CMD ", 4) && inum(l, "cmd") == 0) {
            int b = (int)inum(l, "b");
            char tg[4][64];
            int nt = split(need(l, "targets"), ',', tg, 4);
            if (nt == 1 && atoi(tg[0]) == (b ^ 1)) x->ally_cmd = 1;
            if (nt == 2 && battler_side(atoi(tg[0])) != battler_side(b) && battler_side(atoi(tg[1])) != battler_side(b))
                x->both_cmd = 1;
        }
        if (i > run && !strncmp(l, "TRACE ", 6)) {
            const char *p = need(l, "list");
            for (const char *f = strstr(p, "FaintPhase:"); f; f = strstr(f + 1, "FaintPhase:")) {
                x->faints++;
                x->faint_sides[battler_side(atoi(f + 11))] = 1;
            }
            int in_move = 0, n_faint = 0;
            for (const char *t = p; *t; t += strcspn(t, ",") + (t[strcspn(t, ",")] == ',')) {
                if (!strncmp(t, "MoveEffectPhase:", 16)) in_move = 1, n_faint = 0;
                else if (!strncmp(t, "MoveEndPhase:", 13)) in_move = 0;
                else if (in_move && !strncmp(t, "FaintPhase:", 11) && ++n_faint == 2) x->one_move_faints = 1;
            }
        }
        /* after the turn: who hit whom, from turnData.attacksReceived (move:result:damage:crit:id:battler) */
        if (i > run && !strncmp(l, "TURN ", 5)) {
            const char *b = need(l, "b");
            int side = b[0] == 'e' || b[0] == '2' ? 1 : 0;
            static char recs[32][64];
            int nr = split(need(l, "recv"), ',', recs, 32);
            for (int j = 0; j < nr; j++) {
                char f[6][64];
                if (split(recs[j], ':', f, 6) != 6) continue;
                int src = atoi(f[5]);
                if (src >= 0 && battler_side(src) == side && strcmp(b, src ? (src == 1 ? "p1" : src == 2 ? "2" : "e1") : "0"))
                    x->ally_hit = 1;
            }
        }
    }
    /* both opponents hit by one move: the same source battler and move in both opponents' records */
    for (int s = 0; s < 4 && !x->both_hit; s++) {
        int hit = 0;
        for (int i = run + 1; i < n; i++) {
            const char *l = lines[i];
            if (strncmp(l, "TURN ", 5)) continue;
            const char *b = need(l, "b");
            int side = b[0] == 'e' || b[0] == '2' ? 1 : 0;
            if (side == battler_side(s) || !(b[0] == '0' || b[0] == '2' || !strcmp(b, "p1") || !strcmp(b, "e1"))) continue;
            static char recs[32][64];
            int nr = split(need(l, "recv"), ',', recs, 32);
            for (int j = 0; j < nr; j++) {
                char f[6][64];
                if (split(recs[j], ':', f, 6) == 6 && atoi(f[5]) == s) { hit++; break; }
            }
        }
        if (hit >= 2) x->both_hit = 1;
    }
}

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) {
        fprintf(stderr, "usage: TURNTEST_LOCALES=<upstream>/locales/en doublestest <trace>\n");
        return 2;
    }
    covered_case = calloc(MAX_CASES, 1);
    FILE *f = fopen(argv[1], "r");
    if (!f || !covered_case) { perror("trace"); return 2; }
    static char line[LINE_LEN * 2];
    int version, wanted, cases = 0, ends = 0, footer = 0, threw = 0;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# DOUBLES_CONTRACT %d %d", &version, &wanted) != 2
        || version != 1 || wanted <= 0 || wanted > MAX_CASES) {
        fclose(f); fprintf(stderr, "no doubles contract line\n"); return 2;
    }
    while (fgets(line, sizeof line, f)) {
        if (footer) { fclose(f); fprintf(stderr, "records after completion\n"); return 2; }
        if (!strncmp(line, "CASE ", 5)) {
            int id;
            if (sscanf(line, "CASE %d", &id) != 1 || cases != ends) { fclose(f); return 2; }
            cases++;
        } else if (!strncmp(line, "END ", 4)) {
            if (cases != ends + 1) { fclose(f); return 2; }
            ends++;
        } else if (!strncmp(line, "# DOUBLES_COMPLETE ", 19)) {
            int written;
            if (sscanf(line, "# DOUBLES_COMPLETE %d %d", &written, &threw) != 2 || written != cases
                || written + threw != wanted) { fclose(f); fprintf(stderr, "bad completion line\n"); return 2; }
            footer = 1;
        }
    }
    fclose(f);
    if (!footer || cases != ends) {
        fprintf(stderr, "incomplete doubles trace: requested %d cases %d ends %d footer %d\n", wanted, cases, ends, footer);
        return 2;
    }

    setenv("TURNTEST_COVERED", "1", 1);
    int status = established_turn_main(argc, argv);

    /* what the compared turns exercised */
    f = fopen(argv[1], "r");
    if (!f) { perror("trace"); return 2; }
    static char lines[MAX_LINES][LINE_LEN];
    int n = 0, id = -1, n_covered = 0;
    int ally_cmd = 0, both_cmd = 0, ally_hit = 0, both_hit = 0, faint2 = 0, faint2_same = 0, faint2_both = 0;
    int faint_one_move = 0;
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        if (!strncmp(line, "CASE ", 5)) { id = atoi(line + 5); n = 0; continue; }
        if (line[0] == '#' || id < 0) continue;
        if (n < MAX_LINES) snprintf(lines[n++], LINE_LEN, "%s", line);
        if (strncmp(line, "END ", 4) || !covered_case[id]) continue;
        exercise x;
        scan_case(lines, n, &x);
        n_covered++;
        ally_cmd += x.ally_cmd;
        both_cmd += x.both_cmd;
        ally_hit += x.ally_hit;
        both_hit += x.both_hit;
        faint_one_move += x.one_move_faints;
        if (x.faints >= 2) {
            faint2++;
            if (x.faint_sides[0] != x.faint_sides[1]) faint2_same++;
            else faint2_both++;
        }
        id = -1;
    }
    fclose(f);
    printf("doubles cases %d of %d requested (upstream threw on %d)\n", cases, wanted, threw);
    printf("doubles exercised in compared turns %d: a move aimed at the ally %d (ally hit %d), "
           "aimed at both opponents %d (both hit %d), two or more faints %d (one side %d, both sides %d; "
           "from one move %d)\n",
           n_covered, ally_cmd, ally_hit, both_cmd, both_hit, faint2, faint2_same, faint2_both, faint_one_move);
    fflush(stdout);
    if (status) return status;
    if (n_covered != reported_covered) { fprintf(stderr, "covered list %d, report %d\n", n_covered, reported_covered); return 1; }
    if (reported_fail || reported_parse || reported_stats || message_differ || message_unported
        || reported_pass != reported_covered) {
        fprintf(stderr, "doubles comparison has a difference\n");
        return 1;
    }
    if (!ally_cmd || !ally_hit || !both_cmd || !both_hit || !faint2 || !faint_one_move) {
        fprintf(stderr, "doubles suite did not exercise ally targeting, both opponents and simultaneous faints\n");
        return 1;
    }
    return 0;
}
