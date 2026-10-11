/* Additive Psych Up contract guard over a build-time extension of the unchanged
 * Growth comparator. The extension adds a later pr_get_critical_hit_result probe. */
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
static int guarded_printf(const char *format, ...);
static int message_cases, message_same, message_differ, message_unported;
static int reported_stats, reported_parse, reported_turn_fail;
#define printf guarded_printf
#define main established_turn_main
#include "psych-up-replay.inc"
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

/* Strict scalar validation before the inherited reader's permissive atoi/strtol.
 * A malformed trace is a transport error (2), never a semantic mismatch (1). */
static int scalar(const char *line, const char *key, long long lo, long long hi, int undefined)
{
    const char *v = field(line, key);
    if (!v) return 0;
    if (undefined && !strcmp(v, "u")) return 1;
    const char *p = v;
    if (*p == '-') p++;
    if (*p < '0' || *p > '9') return 0;
    for (; *p; p++) if (*p < '0' || *p > '9') return 0;
    char *end; errno = 0;
    long long n = strtoll(v, &end, 10);
    return !errno && !*end && n >= lo && n <= hi;
}

static int command_valid(const char *line)
{
    const char *b = field(line, "b");
    if (!b || (strcmp(b, "0") && strcmp(b, "2"))) return 0;
    return scalar(line,"cmd",0,0,0) && scalar(line,"move",1,PR_MOVE_COUNT-1,0)
        && scalar(line,"mode",MOVE_USE_MODE_NORMAL,MOVE_USE_MODE_NORMAL,0) && scalar(line,"skip",0,1,0)
        && scalar(line,"cursor",0,3,1) && scalar(line,"baton",0,1,0);
}

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) {
        fprintf(stderr, "usage: TURNTEST_LOCALES=<upstream>/locales/en psych-up-test <trace>\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("trace"); return 2; }
    char line[LINE_LEN];
    int wanted, cases = 0, ends = 0, footer = 0, inputs = 0, ran = 0, probes_seen = 0, probe = 0;
    int trace = 0, text = 0, queue = 0, commands = 0, command_sides = 0;
    if (!fgets(line, sizeof line, f) || !count_line(line,"# PSYCH_UP_CONTRACT 1 ",20,&wanted)) { fclose(f); return 2; }
    while (fgets(line, sizeof line, f)) {
        if (!strchr(line, '\n')) { fclose(f); fprintf(stderr, "truncated/oversized record\n"); return 2; }
        line[strcspn(line,"\n")]=0;
        if (footer) { fclose(f); fprintf(stderr, "records after completion\n"); return 2; }
        if (line[0] == '#' && strncmp(line, "# PSYCH_UP_COMPLETE ", 20)) {
            fclose(f); fprintf(stderr, "unexpected comment record\n"); return 2;
        } else if (!strncmp(line, "CASE ", 5)) {
            int id;
            if (!count_line(line,"CASE ",0,&id) || id != cases || cases != ends) { fclose(f); return 2; }
            cases++; inputs = 0; ran = 0; probe = 0;
            trace = text = queue = commands = command_sides = 0;
        } else if (!strncmp(line,"MOD ",4)) {fclose(f);return 2;
        } else if(!strncmp(line,"CMD ",4)&&!ran){
            if (cases != ends + 1 || !command_valid(line)) { fclose(f); return 2; }
            int side = !strcmp(need(line,"b"),"0") ? 1 : 2;
            if (command_sides & side) { fclose(f); return 2; }
            command_sides |= side; commands++;
            int move=atoi(need(line,"move"));
            if(move==MOVE_ID_PSYCH_UP)inputs++;
        } else if (!strcmp(line, "RUN")) {
            if (ran || inputs != 1 || commands != 2 || command_sides != 3) { fclose(f); return 2; }
            ran = 1;
        } else if (!strncmp(line,"TRACE ",6)) {
            if (!ran || trace++) { fclose(f); return 2; }
        } else if (!strncmp(line,"TEXTS ",6)) {
            if (!ran || text++) { fclose(f); return 2; }
        } else if (!strncmp(line,"QUEUE ",6)) {
            if (!ran || queue++) { fclose(f); return 2; }
        } else if (!strncmp(line,"CMD ",4)) { fclose(f); return 2;
        } else if (!strncmp(line, "CRITPROBE ", 10)) {
            if (!scalar(line,"move",1,PR_MOVE_COUNT-1,0) || !scalar(line,"crit",0,1,0)) { fclose(f); return 2; }
            const char *v = need(line,"att");
            if (strcmp(v,"0") && strcmp(v,"2")) { fclose(f); return 2; }
            v = need(line,"def");
            if (strcmp(v,"0") && strcmp(v,"2")) { fclose(f); return 2; }
            if (!ran || probe++) { fclose(f); return 2; }
            probes_seen++;
        } else if (!strncmp(line, "END ", 4)) {
            if (!ran || probe != 1 || trace != 1 || text != 1 || queue != 1 || cases != ends + 1) { fclose(f); return 2; }
            ends++;
        } else if (!strncmp(line, "# PSYCH_UP_COMPLETE ", 20)) {
            int count;
            if (!count_line(line,"# PSYCH_UP_COMPLETE ",20,&count) || count != wanted || cases != ends) { fclose(f); return 2; }
            footer = 1;
        }
    }
    fclose(f);
    if (!footer || cases != wanted || ends != wanted || probes_seen != wanted) {
        fprintf(stderr, "incomplete Psych Up trace: requested %d cases %d ends %d footer %d\n", wanted, cases, ends, footer);
        return 2;
    }
    int status = established_turn_main(argc, argv);
    if (status) return status;
    int covered = 0;
    for (int i = 0; i < n_ran_phase; i++) if (!strcmp(ran_phase[i].name, "TurnStartPhase")) covered = ran_phase[i].n;
    if (covered != wanted) { fprintf(stderr, "Psych Up coverage %d/%d\n", covered, wanted); return 1; }
    if (message_cases != wanted || message_same != wanted || message_differ || message_unported || reported_parse || reported_stats || reported_turn_fail) {
        fprintf(stderr, "Psych Up stats/state/message contract incomplete or different\n"); return 1;
    }
    if (crit_probes.total != wanted || crit_probes.pass != wanted || crit_probes.fail) {
        fprintf(stderr, "Psych Up critical probe %d/%d (fail %d)\n", crit_probes.pass, wanted, crit_probes.fail); return 1;
    }
    return 0;
}
