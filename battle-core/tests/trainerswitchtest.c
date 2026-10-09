/* Additive contract guard; established turn comparator remains byte-for-byte unchanged. */
#include <stdarg.h>
#include <stdio.h>
static int guarded_printf(const char *format, ...);
static int message_cases, message_same, message_differ, message_unported;
static int reported_stats, reported_parse, reported_turn_fail;
#define printf guarded_printf
#define main established_turn_main
#include "trainer-switch-replay.inc"
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


/* The caller-owned selection boundary stays explicit when no callback is supplied. */
static int missing_callback_check(const char *path)
{
    FILE *f=fopen(path,"r");if(!f)return 1;
    static lines_t input;input.n=0;char line[LINE_LEN];
    while(fgets(line,sizeof line,f)){
        line[strcspn(line,"\n")]=0;if(!strcmp(line,"RUN"))break;
        if(line[0]=='#'||!strncmp(line,"CASE ",5)||!strncmp(line,"EPICK ",6))continue;
        if(input.n>=MAX_LINES){fclose(f);return 1;}
        snprintf(input.s[input.n++],LINE_LEN,"%s",line);
    }
    fclose(f);static case_state cs;static pr_engine engine;
    memset(&cs,0,sizeof cs);memset(&engine,0,sizeof engine);
    for(int k=0;k<4;k++)cs.bt.commands[k].command=cs.bt.pre_commands[k].command=-1;
    read_state(&input,0,input.n,&cs);pr_battle *bt=&cs.bt;
    bt->field[0]=&cs.mon[0][0];bt->field[2]=&cs.mon[1][0];
    bt->next_summon_index=NULL;
    /* Call the actual switch effect on the recorded first eligible trainer holder. */
    pr_force_switch_out(bt,&engine,true,SWITCH_TYPE_SWITCH,bt->field[2],bt->field[0],MOVE_ID_U_TURN);
    bool flagged=false;
    for(int k=0;k<bt->n_unported;k++)if(bt->unported[k]==PR_FEAT_TRAINER)flagged=true;
    if(!flagged)return 1;
    printf("Trainer missing callback boundary: 1 check pass\n");return 0;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) {
        fprintf(stderr, "usage: TURNTEST_LOCALES=<upstream>/locales/en trainerswitchtest <trace>\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("trace"); return 2; }
    char line[LINE_LEN];
    char trailing;
    int version, wanted, cases = 0, ends = 0, footer = 0, inputs = 0, ran = 0;
    if (!fgets(line, sizeof line, f) || sscanf(line, "# TRAINERSW_CONTRACT %d %d %c", &version, &wanted, &trailing) != 2
        || version != 1 || wanted < 20) { fclose(f); return 2; }
    while (fgets(line, sizeof line, f)) {
        line[strcspn(line,"\n")]=0;
        if (footer) { fclose(f); fprintf(stderr, "records after completion\n"); return 2; }
        if (!strncmp(line, "CASE ", 5)) {
            int id;
            if (sscanf(line, "CASE %d %c", &id, &trailing) != 1 || id != cases || cases != ends) { fclose(f); return 2; }
            cases++; inputs = 0; ran = 0;
        } else if (!strncmp(line,"MOD ",4)) {fclose(f);return 2;
        } else if(!strncmp(line,"EPICK ",6)){
            if(ran||inputs){fclose(f);return 2;}inputs++;
            char entries[17][64];int n=split(need(line,"list"),',',entries,17);
            if(n>16){fclose(f);return 2;}
            for(int k=0;k<n;k++){
                int owner,pick;char rest;
                if(sscanf(entries[k],"%d:%d%c",&owner,&pick,&rest)!=2||owner<1||owner>2||pick<1||pick>=PR_MAX_PARTY){fclose(f);return 2;}
            }
        } else if (!strncmp(line, "RUN", 3)) {
            if (ran || inputs != 1) { fclose(f); return 2; }
            ran = 1;
        } else if (!strncmp(line, "END ", 4)) {
            if (!ran || cases != ends + 1) { fclose(f); return 2; }
            ends++;
        } else if (!strncmp(line, "# TRAINERSW_COMPLETE ", 21)) {
            int count;
            if (sscanf(line, "# TRAINERSW_COMPLETE %d %c", &count, &trailing) != 1 || count != wanted) { fclose(f); return 2; }
            footer = 1;
        }
    }
    fclose(f);
    if (!footer || cases != wanted || ends != wanted) {
        fprintf(stderr, "incomplete Trainer switch trace: requested %d cases %d ends %d footer %d\n", wanted, cases, ends, footer);
        return 2;
    }
    if(missing_callback_check(argv[1]))return 1;
    int status = established_turn_main(argc, argv);
    if (status) return status;
    int covered = 0;
    for (int i = 0; i < n_ran_phase; i++) if (!strcmp(ran_phase[i].name, "TurnStartPhase")) covered = ran_phase[i].n;
    if (covered != wanted) { fprintf(stderr, "Trainer switch coverage %d/%d\n", covered, wanted); return 1; }
    if (message_cases != wanted || message_same != wanted || message_differ || message_unported || reported_parse || reported_stats || reported_turn_fail) {
        fprintf(stderr, "Trainer switch stats/state/message contract incomplete or different\n"); return 1;
    }
    return 0;
}
