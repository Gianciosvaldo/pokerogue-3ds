/* Additive contract guard; established turn comparator remains byte-for-byte unchanged. */
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
static int guarded_printf(const char *format, ...);
static int message_cases, message_same, message_differ, message_unported;
static int reported_stats, reported_parse, reported_turn_fail;
#define printf guarded_printf
#define main established_turn_main
#include "speed-boost-replay.inc"
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
    if (!strncmp(output,"covered ",8)) {
        int cases,pass,uncovered;
        sscanf(output,"covered %d: pass %d, fail %d; not covered %d",&cases,&pass,&reported_turn_fail,&uncovered);
    }
    fputs(output, stdout);
    return n;
}


/* Reject fractional, trailing and overflowed request/identity counts rather than
 * relying on scanf's narrowing conversion for the new owning contract. */
static int count_line(const char *line, const char *prefix, int minimum, int *count)
{
    size_t n=strlen(prefix);
    if(strncmp(line,prefix,n))return 0;
    const char *start=line+n;
    if(*start<'0'||*start>'9')return 0;
    char *end;errno=0;long value=strtol(start,&end,10);
    if(errno||end==start||value<minimum||value>INT_MAX)return 0;
    if(*end && !(*end=='\n' && !end[1]))return 0;
    *count=(int)value;return 1;
}

/* Source apply always queues a stat phase, even for a simulated call. A
 * queue-less API must retain its explicit unsupported boundary. */
static int queue_boundary_checks(void)
{
    pr_battle bt={0};pr_pokemon p={0};
    p.is_player=true;p.on_field=true;p.hp=100;p.stats[STAT_HP]=100;
    p.ability=ABILITY_ID_SPEED_BOOST;bt.field[0]=&p;
    pr_ab_params params={.pokemon=&p,.move=-1};
    pr_apply_ab_attrs(&bt,PR_AC_PostTurnAbAttr,&params);
    if(bt.n_unported!=1||bt.unported[0]!=PR_AC_SpeedBoostAbAttr)return 1;
    bt.n_unported=0;params.simulated=true;
    pr_apply_ab_attrs(&bt,PR_AC_PostTurnAbAttr,&params);
    if(bt.n_unported!=1||bt.unported[0]!=PR_AC_SpeedBoostAbAttr)return 1;
    bt.n_unported=0;params.simulated=false;p.switched_in_this_turn=true;
    pr_apply_ab_attrs(&bt,PR_AC_PostTurnAbAttr,&params);if(bt.n_unported)return 1;
    p.switched_in_this_turn=false;p.failed_run_away=true;
    pr_apply_ab_attrs(&bt,PR_AC_PostTurnAbAttr,&params);if(bt.n_unported)return 1;
    printf("Speed Boost queue boundary: 4 checks pass\n");return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) {
        fprintf(stderr, "usage: TURNTEST_LOCALES=<upstream>/locales/en speedboosttest <trace>\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("trace"); return 2; }
    char line[LINE_LEN];
    int wanted, cases = 0, ends = 0, footer = 0, inputs = 0, ran = 0;
    if (!fgets(line, sizeof line, f) || !count_line(line,"# SPEEDBOOST_CONTRACT 1 ",20,&wanted)) { fclose(f); return 2; }
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line,"\n")]=0;
        if (footer) { fclose(f); fprintf(stderr, "records after completion\n"); return 2; }
        if (!strncmp(line, "CASE ", 5)) {
            int id;
            if (!count_line(line,"CASE ",0,&id) || id != cases || cases != ends) { fclose(f); return 2; }
            cases++; inputs = 0; ran = 0;
        } else if (!strncmp(line,"MOD ",4)) {fclose(f);return 2;
        } else if(!strncmp(line,"CMD ",4)&&!ran){
            int move=atoi(need(line,"move"));
            inputs++;
        } else if (!strcmp(line, "RUN")) {
            if (ran || inputs != 2) { fclose(f); return 2; }
            ran = 1;
        } else if (!strncmp(line, "END ", 4)) {
            if (!ran || cases != ends + 1) { fclose(f); return 2; }
            ends++;
        } else if (!strncmp(line, "# SPEEDBOOST_COMPLETE ", 22)) {
            int count;
            if (!count_line(line,"# SPEEDBOOST_COMPLETE ",20,&count) || count != wanted) { fclose(f); return 2; }
            footer = 1;
        }
    }
    fclose(f);
    if (!footer || cases != wanted || ends != wanted) {
        fprintf(stderr, "incomplete Speed Boost trace: requested %d cases %d ends %d footer %d\n", wanted, cases, ends, footer);
        return 2;
    }
    int status = established_turn_main(argc, argv);
    if (status) return status;
    int covered = 0;
    for (int i = 0; i < n_ran_phase; i++) if (!strcmp(ran_phase[i].name, "TurnStartPhase")) covered = ran_phase[i].n;
    if (covered != wanted) { fprintf(stderr, "Speed Boost coverage %d/%d\n", covered, wanted); return 1; }
    if (message_cases != wanted || message_same != wanted || message_differ || message_unported || reported_parse || reported_stats || reported_turn_fail) {
        fprintf(stderr, "Speed Boost stats/state/message contract incomplete or different\n"); return 1;
    }
    return queue_boundary_checks();
}
