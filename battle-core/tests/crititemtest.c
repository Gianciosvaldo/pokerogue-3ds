/* Additive contract guard; established turn comparator remains byte-for-byte unchanged. */
#include <stdarg.h>
#include <stdio.h>
static int guarded_printf(const char *format, ...);
static int message_cases, message_same, message_differ, message_unported;
static int reported_stats, reported_parse;
#define printf guarded_printf
#define main established_turn_main
#include "critical-item-replay.inc"
#undef main
#undef printf

static int guarded_printf(const char *format, ...)
{
    char output[65536];
    va_list args; va_start(args, format);
    int n = vsnprintf(output, sizeof output, format, args);
    va_end(args);
    if (!strncmp(output, "texts of covered turns ", 23))
        sscanf(output, "texts of covered turns %d: identical %d, differ %d, a message with no text yet %d",
               &message_cases, &message_same, &message_differ, &message_unported);
    if (!strncmp(output, "cases ", 6)) {
        int total, skipped;
        sscanf(output, "cases %d (skipped %d: records not read yet), parse mismatches %d, stats mismatches %d",
               &total, &skipped, &reported_parse, &reported_stats);
    }
    fputs(output, stdout);
    return n;
}


#include "crititem-guards.inc"

/* Full-turn admission stays separate from raw critical-stage projection. */
static int fusion_admission_checks(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) return 1;
    static lines_t input;
    input.n = 0;
    char line[LINE_LEN];
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line, "\n")] = 0;
        if (!strcmp(line, "RUN")) break;
        if (line[0] == '#' || !strncmp(line, "CASE ", 5)) continue;
        if (input.n >= MAX_LINES) { fclose(f); return 1; }
        snprintf(input.s[input.n++], LINE_LEN, "%s", line);
    }
    fclose(f);
    for (int mode = 0; mode < 3; mode++) {
        static case_state cs;
        static pr_engine engine;
        memset(&cs, 0, sizeof cs);
        memset(&engine, 0, sizeof engine);
        for (int k = 0; k < 4; k++) cs.bt.commands[k].command = cs.bt.pre_commands[k].command = -1;
        read_state(&input, 0, input.n, &cs);
        pr_battle *bt = &cs.bt;
        bt->field[0] = &cs.mon[0][0]; bt->field[2] = &cs.mon[1][0];
        pr_pokemon *p = bt->field[0];
        p->fusion_species = SPECIES_ID_RATTATA;
        p->species = SPECIES_ID_FARFETCHD;
        bt->n_mods[0] = 1; bt->n_mods[1] = 0;
        pr_modifier *m = &bt->mods[0][0];
        *m = (pr_modifier){.cls=mode?PR_MOD_SPECIES_CRIT_BOOSTER:PR_MOD_CRIT_BOOSTER,
            .pokemon_id=mode==2?-1:(int64_t)p->id,.stack_count=1,.num=mode?2:1};
        if (mode) { m->n_species=3; m->species[0]=SPECIES_ID_FARFETCHD;
            m->species[1]=SPECIES_ID_GALAR_FARFETCHD; m->species[2]=SPECIES_ID_SIRFETCHD; }
        int stage=0;
        pr_project_crit_booster_modifiers(bt,p,&stage);
        if (stage != (mode?2:1) || bt->n_unported) return 1;
        pr_run_turn(bt,&engine);
        bool flagged=false;
        for (int i=0;i<bt->n_unported;i++) if (bt->unported[i]==PR_FEAT_TURN_MODIFIERS) flagged=true;
        if (flagged != (mode!=0)) return 1;
    }
    printf("Critical full-turn fusion admission: 3 checks pass\n");
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) {
        fprintf(stderr, "usage: TURNTEST_LOCALES=<upstream>/locales/en crititemtest <trace>\n");
        return 2;
    }
    if (fusion_admission_checks(argv[1])) { fprintf(stderr, "Critical fusion admission checks failed\n"); return 1; }
    if (crititem_guard_checks()) { fprintf(stderr, "Critical item guard checks failed\n"); return 1; }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("trace"); return 2; }
    char line[LINE_LEN];
    char trailing;
    int version, wanted, cases = 0, ends = 0, footer = 0, inputs = 0, ran = 0;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# CRITITEM_CONTRACT %d %d %c", &version, &wanted, &trailing) != 2
        || version != 1 || wanted <= 0) { fclose(f); return 2; }
    while (fgets(line, sizeof line, f)) {
        if (footer) { fclose(f); fprintf(stderr, "records after completion\n"); return 2; }
        if (!strncmp(line, "CASE ", 5)) {
            int id;
            if (sscanf(line, "CASE %d %c", &id, &trailing) != 1 || id != cases || cases != ends) { fclose(f); return 2; }
            cases++; inputs = 0; ran = 0;
        } else if (!strncmp(line, "MOD ", 4) && !ran) {
            char cls[80]; int side; long long pid;
            if (sscanf(line, "MOD side=%d cls=%79s pid=%lld", &side, cls, &pid) != 3
                || side < 0 || side > 1 || pid < -1 || (strcmp(cls, "CritBoosterModifier") && strcmp(cls, "SpeciesCritBoosterModifier"))) {
                fclose(f); return 2;
            }
            inputs++;
        } else if (!strncmp(line, "RUN", 3)) {
            if (ran || (inputs < 1 || inputs > 2)) { fclose(f); return 2; }
            ran = 1;
        } else if (!strncmp(line, "END ", 4)) {
            if (!ran || cases != ends + 1) { fclose(f); return 2; }
            ends++;
        } else if (!strncmp(line, "# CRITITEM_COMPLETE ", 20)) {
            int count;
            if (sscanf(line, "# CRITITEM_COMPLETE %d %c", &count, &trailing) != 1 || count != wanted) { fclose(f); return 2; }
            footer = 1;
        }
    }
    fclose(f);
    if (!footer || cases != wanted || ends != wanted) {
        fprintf(stderr, "incomplete Critical item trace: requested %d cases %d ends %d footer %d\n", wanted, cases, ends, footer);
        return 2;
    }
    int status = established_turn_main(argc, argv);
    if (status) return status;
    int covered = 0;
    for (int i = 0; i < n_ran_phase; i++) if (!strcmp(ran_phase[i].name, "TurnStartPhase")) covered = ran_phase[i].n;
    if (covered != wanted) { fprintf(stderr, "Critical item coverage %d/%d\n", covered, wanted); return 1; }
    if (message_cases != wanted || message_same != wanted || message_differ || message_unported || reported_parse || reported_stats) {
        fprintf(stderr, "Critical item stats/state/message contract incomplete or different\n"); return 1;
    }
    return 0;
}
