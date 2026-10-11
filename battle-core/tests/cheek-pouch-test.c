/* Additive Cheek Pouch contract guard over a build-time extension of the unchanged
 * Growth comparator. The extension observes zero temporary overrides too. */
#include <stdarg.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <math.h>
static int guarded_printf(const char *format, ...);
static int message_cases, message_same, message_differ, message_unported;
static int reported_stats, reported_parse, reported_turn_fail;
#define printf guarded_printf
#define main established_turn_main
#include "cheek-pouch-replay.inc"
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

static int number_list(const char *line, const char *key, int count, double lo, double hi, int integers)
{
    const char *v=field(line,key);
    if(!v) return 0;
    for(int i=0;i<count;i++) {
        char *end; errno=0; double n=strtod(v,&end);
        if(end==v || errno || !isfinite(n) || n<lo || n>hi || (integers && n!=floor(n))) return 0;
        if(i==count-1) return !*end;
        if(*end!=',') return 0;
        v=end+1;
    }
    return 0;
}

static int type_list(const char *line){
 const char *v=field(line,"types");if(!v)return 0;if(!strcmp(v,"-"))return 1;
 int count=1;for(const char *p=v;*p;p++)if(*p==',')count++;
 return count<=PR_MAX_SUMMON_TYPES&&number_list(line,"types",count,POKEMON_TYPE_UNKNOWN,POKEMON_TYPE_STELLAR,1);
}

static int owner(const char *line)
{
    const char *v=field(line,"b");
    if(!v) return -1;
    if(!strcmp(v,"0")) return 0;
    if(!strcmp(v,"2")) return 6;
    if((v[0]=='p'||v[0]=='e') && v[1]>='1' && v[1]<='5' && !v[2])
        return (v[0]=='e'?6:0)+(v[1]-'0');
    return -1;
}

static int snapshot_line(const char *line, unsigned masks[12], unsigned *globals)
{
    const char *const permon[]={"MON ","TURN ","MOVES ","HIST ","MQ ","EFFECTIVE "};
    for(int k=0;k<6;k++) if(!strncmp(line,permon[k],strlen(permon[k]))) {
        int b=owner(line);unsigned bit=1u<<k;
        if(b<0 || masks[b]&bit) return 0;
        masks[b]|=bit;
        if(k==0) {
            if(!type_list(line)||!scalar(line,"added",0,POKEMON_TYPE_STELLAR,0))return 0;
            if(!scalar(line,"id",0,4294967295LL,0) || !scalar(line,"sp",1,PR_SPECIES_COUNT-1,0)
                || !scalar(line,"form",0,255,0) || !scalar(line,"lv",1,100,0)
                || !scalar(line,"abil",0,PR_ABILITY_COUNT-1,0) || !scalar(line,"babil",0,PR_ABILITY_COUNT-1,0)
                || !scalar(line,"st",0,STATUS_EFFECT_FAINT,0)
                || !number_list(line,"stats",6,1,2147483647,1)
                || !number_list(line,"stages",7,-6,6,1)
                || !number_list(line,"ivs",6,0,31,1)
                || !number_list(line,"ss",6,0,2147483647,0)) return 0;
        }
        if(k==5 && !number_list(line,"stats",5,0,2147483647,0)) return 0;
        return 1;
    }
    const char *const global[]={"RNG ","BATTLE ","ARENA ","PICK "};
    for(int k=0;k<4;k++) if(!strncmp(line,global[k],strlen(global[k]))) {
        if(k==2 && (!scalar(line,"terrain",0,TERRAIN_PSYCHIC,0)||!scalar(line,"biome",0,50,0)))return 0;
        unsigned bit=1u<<k;if(*globals&bit)return 0;*globals|=bit;return 1;
    }
    if(!strncmp(line,"PARTY ",6)) {
        if(!scalar(line,"side",0,1,0))return 0;
        unsigned bit=1u<<(4+atoi(need(line,"side")));if(*globals&bit)return 0;*globals|=bit;return 1;
    }
    if(!strncmp(line,"BTAG ",5)) return owner(line)>=0;
    return 0;
}

static int snapshot_complete(unsigned masks[12], unsigned globals)
{
    if(globals!=63 || masks[0]!=63 || masks[6]!=63) return 0;
    for(int i=0;i<12;i++) if(masks[i] && masks[i]!=63) return 0;
    return 1;
}

int main(int argc, char **argv)
{
    if (argc != 2 || !getenv("TURNTEST_LOCALES")) {
        fprintf(stderr, "usage: TURNTEST_LOCALES=<upstream>/locales/en cheek-pouch-test <trace>\n");
        return 2;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) { perror("trace"); return 2; }
    char line[LINE_LEN];
    int wanted, cases = 0, ends = 0, footer = 0, inputs = 0, ran = 0, probe = 0;
    int trace = 0, text = 0, queue = 0, commands = 0, command_sides = 0, after_command_sides = 0;
    unsigned masks[2][12]={{0}},globals[2]={0};
    if (!fgets(line,sizeof line,f)) {fclose(f);return 2;}
    const char *header="# CHEEK_POUCH_CONTRACT 1 ";
    const char *complete="# CHEEK_POUCH_COMPLETE ";
    if(!count_line(line,header,20,&wanted)) {fclose(f);return 2;}
    while (fgets(line, sizeof line, f)) {
        if (!strchr(line, '\n')) { fclose(f); fprintf(stderr, "truncated/oversized record\n"); return 2; }
        line[strcspn(line,"\n")]=0;
        if (footer) { fclose(f); fprintf(stderr, "records after completion\n"); return 2; }
        if (line[0] == '#' && strncmp(line, complete, strlen(complete))) {
            fclose(f); fprintf(stderr, "unexpected comment record\n"); return 2;
        } else if (!strncmp(line, "CASE ", 5)) {
            int id;
            if (!count_line(line,"CASE ",0,&id) || id != cases || cases != ends) { fclose(f); return 2; }
            cases++; inputs = 0; ran = 0; probe = 0;
            trace = text = queue = commands = command_sides = 0, after_command_sides = 0;
            memset(masks,0,sizeof masks);globals[0]=globals[1]=0;
        } else if (!strncmp(line,"MOD ",4)) {
            if(cases!=ends+1 || strcmp(need(line,"cls"),"BerryModifier") || !scalar(line,"side",0,1,0) || !scalar(line,"pid",0,4294967295LL,0) || !scalar(line,"stack",1,3,0) || !scalar(line,"btype",BERRY_TYPE_SITRUS,BERRY_TYPE_LEPPA,0)){fclose(f);return 2;}
        } else if(!strncmp(line,"CMD ",4)&&!ran){
            if (cases != ends + 1 || !command_valid(line)) { fclose(f); return 2; }
            int side = !strcmp(need(line,"b"),"0") ? 1 : 2;
            if (command_sides & side) { fclose(f); return 2; }
            command_sides |= side; commands++;
            int move=atoi(need(line,"move"));
            if((move==MOVE_ID_SPLASH))inputs++;
        } else if (!strcmp(line, "RUN")) {
            if (ran || inputs != 2 || commands != 2 || command_sides != 3) { fclose(f); return 2; }
            if(!snapshot_complete(masks[0],globals[0])) {fclose(f);return 2;}
            ran = 1;
        } else if (!strncmp(line,"TRACE ",6)) {
            if (!ran || trace++) { fclose(f); return 2; }
        } else if (!strncmp(line,"TEXTS ",6)) {
            if (!ran || text++) { fclose(f); return 2; }
        } else if (!strncmp(line,"QUEUE ",6)) {
            if (!ran || queue++) { fclose(f); return 2; }
        } else if (!strncmp(line,"CMD ",4)) {
            if(!ran||!command_valid(line)){fclose(f);return 2;}
            int side=!strcmp(need(line,"b"),"0")?1:2;
            if(after_command_sides&side){fclose(f);return 2;}after_command_sides|=side;

        } else if (!strncmp(line, "END ", 4)) {
            if(strcmp(line,"END stop=TurnInitPhase") && strcmp(line,"END stop=BattleEndPhase") && strcmp(line,"END stop=GameOverPhase")){fclose(f);return 2;}
            if (!ran || probe != 0 || trace != 1 || text != 1 || queue != 1 || cases != ends + 1) { fclose(f); return 2; }
            if(!snapshot_complete(masks[1],globals[1])) {fclose(f);return 2;}
            ends++;
        } else if (!strncmp(line, complete, strlen(complete))) {
            int count;
            if (!count_line(line,complete,20,&count) || count != wanted || cases != ends) { fclose(f); return 2; }
            footer = 1;
        } else if(cases!=ends+1 || probe || !snapshot_line(line,masks[ran?1:0],&globals[ran?1:0])) {
            fclose(f);fprintf(stderr,"invalid/missing/duplicate state record\n");return 2;
        }
    }
    fclose(f);
    if (!footer || cases != wanted || ends != wanted) {
        fprintf(stderr, "incomplete Cheek Pouch trace: requested %d cases %d ends %d footer %d\n", wanted, cases, ends, footer);
        return 2;
    }
    int status = established_turn_main(argc, argv);
    if (status) return status;
    int covered = 0;
    for (int i = 0; i < n_ran_phase; i++) if (!strcmp(ran_phase[i].name, "TurnStartPhase")) covered = ran_phase[i].n;
    if (covered != wanted) { fprintf(stderr, "Cheek Pouch coverage %d/%d\n", covered, wanted); return 1; }
    if (message_cases != wanted || message_same != wanted || message_differ || message_unported || reported_parse || reported_stats || reported_turn_fail) {
        fprintf(stderr, "Cheek Pouch stats/state/message contract incomplete or different\n"); return 1;
    }
    return 0;
}
