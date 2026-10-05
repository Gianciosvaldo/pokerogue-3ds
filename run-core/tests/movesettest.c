#define _POSIX_C_SOURCE 200809L
#include "pr/moveset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static char *next(char **s) { char *t=strtok_r(NULL," \r\n",s); if(!t){fprintf(stderr,"short trace\n");exit(2);}return t; }
static int integer(char **s) { return atoi(next(s)); }
static void state(char **s,pr_rng *r) {
    memset(r,0,sizeof *r);r->c=strtod(next(s),NULL);r->s0=strtod(next(s),NULL);
    r->s1=strtod(next(s),NULL);r->s2=strtod(next(s),NULL);
}
static bool equal_rng(const pr_rng *a,const pr_rng *b) {
    return a->c==b->c && a->s0==b->s0 && a->s1==b->s1 && a->s2==b->s2;
}
static int unsupported_contract(void) {
    int failures=0;
    for(int reason=1;reason<PR_MOVESET_REASON_COUNT;reason++) {
        pr_moveset_ctx c={.species=SPECIES_ID_MAGIKARP,.level=10,.ability=ABILITY_ID_SWIFT_SWIM,.attack=20,.special_attack=15};
        switch(reason) {
        case PR_MOVESET_SPECIES:c.species=SPECIES_ID_BULBASAUR;break;
        case PR_MOVESET_FORM:c.form=1;break;
        case PR_MOVESET_LEVEL_POOL:c.level=25;break;
        case PR_MOVESET_TM_EGG_POOL:c.level=25;c.trainer=true;break;
        case PR_MOVESET_ABILITY_STATE:c.passive=true;break;
        case PR_MOVESET_FUSION:c.fused=true;break;
        case PR_MOVESET_TERA:c.will_tera=true;break;
        case PR_MOVESET_CHALLENGE:c.other_challenges=true;break;
        case PR_MOVESET_INVALID:c.level=0;break;
        }
        pr_rng rng={.c=1,.s0=.2,.s1=.3,.s2=.4},before=rng;
        pr_generated_moveset out,sentinel;memset(&out,0x5a,sizeof out);sentinel=out;
        pr_moveset_result rc=pr_run_enemy_moveset(&rng,&c,&out);
        bool ok=rc==(pr_moveset_result)reason && equal_rng(&rng,&before) && !memcmp(&out,&sentinel,sizeof out);
        failures+=!ok;
        printf("unported guard %s: cases 1 reported %d failures %d (host only)\n",pr_moveset_reason(reason),rc==(pr_moveset_result)reason,!ok);
    }
    return failures;
}
int main(int argc,char **argv) {
    if(argc!=4){fprintf(stderr,"usage: movesettest trace N seed\n");return 2;}
    int expected=atoi(argv[2]);unsigned expected_seed=(unsigned)strtoul(argv[3],NULL,10);
    FILE *f=fopen(argv[1],"r");if(!f){perror("trace");return 2;}
    char line[2048],pin[64];int version,header_n;unsigned header_seed;
    if(!fgets(line,sizeof line,f) || sscanf(line,"# MOVESET %d %d %u %63s",&version,&header_n,&header_seed,pin)!=4
        || version!=1 || header_n!=expected || header_seed!=expected_seed
        || strcmp(pin,"8555c08c823b856cbec4eb99ca84ea52a955836d")) {fprintf(stderr,"invalid header\n");return 2;}
    int cases=0,identical=0,mismatches=0,wild=0,trainer=0,boss=0,level=0,fixed=0,stab=0,rejected=0,inverse=0,forms[2]={0};
    bool complete=false;
    while(fgets(line,sizeof line,f)) {
        char *s,*tag=strtok_r(line," \r\n",&s);
        if(!tag || complete){fprintf(stderr,"unexpected content\n");return 2;}
        if(!strcmp(tag,"END")) {
            int n=integer(&s);unsigned seed=(unsigned)strtoul(next(&s),NULL,10);
            if(n!=cases || n!=expected || seed!=expected_seed || strtok_r(NULL," \r\n",&s)){fprintf(stderr,"invalid footer\n");return 2;}
            complete=true;continue;
        }
        if(strcmp(tag,"M")){fprintf(stderr,"unknown record\n");return 2;}
        pr_moveset_ctx c={0};c.species=integer(&s);c.form=integer(&s);c.level=integer(&s);c.ability=integer(&s);
        c.attack=integer(&s);c.special_attack=integer(&s);c.trainer=integer(&s);c.boss=integer(&s);c.inverse=integer(&s);
        int wave=integer(&s);pr_rng rng,after;state(&s,&rng);
        int n=integer(&s),ids[4],pp[4],up[4],used[4];
        if(n<0 || n>4){fprintf(stderr,"invalid moves\n");return 2;}
        for(int i=0;i<n;i++){ids[i]=integer(&s);pp[i]=integer(&s);up[i]=integer(&s);used[i]=integer(&s);}
        state(&s,&after);if(strtok_r(NULL," \r\n",&s)){fprintf(stderr,"extra fields\n");return 2;}
        pr_generated_moveset result={0};pr_moveset_result rc=pr_run_enemy_moveset(&rng,&c,&result);
        bool ok=rc==PR_MOVESET_OK && result.count==n && equal_rng(&rng,&after);
        for(int i=0;i<n;i++)ok&=result.moves[i]==ids[i] && result.pp[i]==pp[i] && result.pp_up[i]==up[i] && used[i]==0;
        cases++;if(ok)identical++;else {mismatches++;if(mismatches<=10)printf("MISMATCH case %d species %d level %d wave %d reason %s rng %d\n",cases,c.species,c.level,wave,pr_moveset_reason(rc),equal_rng(&rng,&after));}
        wild+=!c.trainer;trainer+=c.trainer;boss+=c.boss;inverse+=c.inverse;
        bool is_fixed=c.species==SPECIES_ID_SMEARGLE || c.species==SPECIES_ID_ETERNATUS;
        fixed+=is_fixed;level+=!is_fixed;stab+=result.forced_stab;rejected+=result.soft_rejection;
        if(c.species==SPECIES_ID_ETERNATUS)forms[c.form]++;
    }
    fclose(f);if(!complete || cases!=expected){fprintf(stderr,"missing completion footer\n");return 2;}
    printf("moveset seed %u: cases %d identical %d mismatches %d\n",expected_seed,cases,identical,mismatches);
    printf("paths: wild %d trainer %d boss %d level %d fixed %d STAB %d rejection %d Eternatus0 %d Eternatus1 %d inverse %d\n",wild,trainer,boss,level,fixed,stab,rejected,forms[0],forms[1],inverse);
    if(!wild || !trainer || !boss || !level || !fixed || !stab || !rejected || !forms[0] || !forms[1] || !inverse){fprintf(stderr,"missing path coverage\n");return 2;}
    int guards=unsupported_contract();return mismatches || guards ? 1 : 0;
}
