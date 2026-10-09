#define _POSIX_C_SOURCE 200809L
/* Additive Card 108 replay. Established state reader/comparator stay unchanged. */
#define main established_ai_main
#include "aitest.c"
#undef main
#include "../src/ai_internal.h"
#include <errno.h>

static long strict_int(const char *line, const char *key, long lo, long hi)
{
    const char *v = need(line, key); char *end; errno = 0;
    long x = strtol(v, &end, 10);
    if (errno || !*v || *end || x < lo || x > hi) { fprintf(stderr,"invalid %s\n",key); exit(2); }
    return x;
}
static double strict_double(const char *line, const char *key)
{
    const char *v=need(line,key); char *end; errno=0;
    double x=strtod(v,&end);
    if(errno || !*v || *end || !isfinite(x)) exit(2);
    return x;
}
static void validate_records(const lines_t *js, int run)
{
    static const char *const before[]={"RNG ","BATTLE ","ARENA ","MON ","TURN ","MOVES ","HIST ","MQ ","AIMON ","XMOD ","PARTY ","TRAINER ","AI ","SESS ","CALLSEL "};
    static const char *const after[]={"PROBE ","CMD ","AFTER ","RNG ","MQ ","SCORES ","CALLSEL ","QUEUE ","APPLIED ","MATCHUP ","NEXT "};
    int bc[15]={0}, ac[11]={0};
    unsigned mask[6]={0}; int party_mask=0, applied_mask=0;
    for(int i=0;i<js->n;i++) {
        const char *l=js->s[i]; bool found=false;
        const char *const *keys=i<run?before:after;
        int count=i<run?15:11;
        for(int k=0;k<count;k++) if(!strncmp(l,keys[k],strlen(keys[k]))) {
            (i<run?bc:ac)[k]++;found=true;break;
        }
        if(!found) exit(2);
        if(i<run) {
            const char *const keyed[]={"MON ","TURN ","MOVES ","HIST ","MQ ","AIMON "};
            for(int k=0;k<6;k++) if(!strncmp(l,keyed[k],strlen(keyed[k]))) {
                const char *b=need(l,"b"); int slot=-1;
                if(!strcmp(b,"0"))slot=0;
                else if(!strcmp(b,"2"))slot=6;
                else if(strlen(b)==2 && b[1]>='1' && b[1]<='5' && (b[0]=='p' || b[0]=='e'))slot=(b[0]=='e'?6:0)+b[1]-'0';
                if(slot<0 || (mask[k]&(1u<<slot)))exit(2);
                mask[k]|=1u<<slot;
            }
            if(!strncmp(l,"PARTY ",6)) {
                int side=(int)strict_int(l,"side",0,1);
                if(party_mask&(1<<side))exit(2);
                party_mask|=1<<side;
            }
        } else if(!strncmp(l,"APPLIED ",8)) {
            int b=(int)strict_int(l,"b",0,2);
            if((b!=0 && b!=2) || (applied_mask&(1<<b)))exit(2);
            applied_mask|=1<<b;
        }
        if(!strncmp(l,"MON ",4)) {
            strict_int(l,"id",1,UINT32_MAX); strict_int(l,"lv",1,100);
            strict_int(l,"sp",1,65535); strict_int(l,"hp",0,INT32_MAX);
        }
    }
    if(party_mask!=3 || applied_mask!=5)exit(2);
    for(int k=1;k<6;k++)if(mask[k]!=mask[0])exit(2);
    const int singleton[]={0,1,2,11,12,13,14};
    for(int k=0;k<7;k++)if(bc[singleton[k]]!=1)exit(2);
    if(bc[10]!=2 || !bc[3] || bc[3]!=bc[4] || bc[3]!=bc[5] || bc[3]!=bc[6] || bc[3]!=bc[7] || bc[3]!=bc[8])exit(2);
    for(int k=0;k<11;k++)if(k!=5 && k!=8 && ac[k]!=1)exit(2);
    if(ac[5]>1 || ac[8]!=2)exit(2);
}
static void read_extra(const lines_t *js, int run, case_state *cs)
{
    for (int i=0;i<run;i++) {
        const char *l=js->s[i];
        if (!strncmp(l,"AIMON ",6)) {
            pr_pokemon *p=mon_for(cs,need(l,"b"),NULL);
            p->trainer_slot=(int8_t)strict_int(l,"slot",0,2);
        } else if (!strncmp(l,"XMOD ",5)) {
            int side=(int)strict_int(l,"side",0,1);
            if (strcmp(need(l,"cls"),"AttackTypeBoosterModifier")) {fprintf(stderr,"invalid modifier\n");exit(2);}
            if (cs->bt.n_mods[side]>=PR_MAX_MODIFIERS) exit(2);
            pr_modifier *m=&cs->bt.mods[side][cs->bt.n_mods[side]++];
            memset(m,0,sizeof *m); m->cls=PR_MOD_ATTACK_TYPE_BOOSTER; m->is_transferable=true;
            m->pokemon_id=strict_int(l,"pid",1,UINT32_MAX);
            m->stack_count=(int32_t)strict_int(l,"stack",1,99);
            m->virtual_stack_count=(int32_t)strict_int(l,"virtual",0,0);
            m->move_type=(int8_t)strict_int(l,"type",0,18);
            if (strcmp(need(l,"mult"),"0.2")) {fprintf(stderr,"invalid multiplier\n");exit(2);}
            m->mult=.2;
        }
    }
}
int main(int argc,char **argv)
{
    if (argc!=3) {fprintf(stderr,"usage: trainerheldtest trace requested-count\n");return 2;}
    char *end; long want=strtol(argv[2],&end,10); if (*end || want<=0) return 2;
    FILE *f=fopen(argv[1],"r"); if(!f){perror("trace");return 2;}
    char line[LINE_LEN]; int version,n,used;
    if(!fgets(line,sizeof line,f) || sscanf(line,"# AI108_CONTRACT %d %d %n",&version,&n,&used)!=2 || version!=1 || n!=want || line[used]) return 2;
    static lines_t js; static case_state cs; int total=0,pass=0,fail=0,run=-1,profile=-1,state=-1,complete=0;
    int profiles[8]={0}, states[8]={0}, switches=0, scored=0, held=0;
    while(fgets(line,sizeof line,f)) {
        if(!strchr(line,'\n')) return 2;
        line[strcspn(line,"\n")]=0;
        if(!strncmp(line,"# AI108_COMPLETE ",17)) {
            int threw; if(sscanf(line,"# AI108_COMPLETE %d %d %n",&n,&threw,&used)!=2 || line[used] || n!=want || threw || total!=want || run!=-1) return 2;
            complete=1; if(fgets(line,sizeof line,f)) return 2; break;
        }
        if(!strncmp(line,"CASE ",5)) {
            if(run!=-1 || sscanf(line,"CASE %d %n",&n,&used)!=1 || line[used] || n!=total) return 2;
            js.n=0; run=-2; profile=state=-1; continue;
        }
        if(run==-1) return 2;
        if(!strncmp(line,"PROFILE ",8)) {
            if(profile!=-1 || run!=-2) return 2;
            profile=(int)strict_int(line,"kind",0,7); state=(int)strict_int(line,"state",0,7); continue;
        }
        if(!strcmp(line,"RUN")) {if(run!=-2 || profile<0 || js.n==0)return 2;run=js.n;continue;}
        if(strncmp(line,"END ",4)) {
            if(js.n>=MAX_LINES)return 2;
            snprintf(js.s[js.n++],LINE_LEN,"%s",line);continue;
        }
        if(run<0 || sscanf(line,"END %d %n",&n,&used)!=1 || line[used] || n!=total) return 2;
        validate_records(&js,run);
        if(profile!=total%8 || state!=(total/8)%8) return 2;
        memset(&cs,0,sizeof cs);
        for(int k=0;k<4;k++)cs.bt.commands[k].command=cs.bt.pre_commands[k].command=-1;
        pr_status_healed_clear();pr_called_selected_reset();pr_ai_session_reset();
        /* New records are omitted from the established reader, without editing it. */
        static lines_t base; base.n=0;
        for(int i=0;i<run;i++) if(strncmp(js.s[i],"XMOD ",5)) memcpy(base.s[base.n++],js.s[i],LINE_LEN);
        read_state(&base,0,base.n,&cs);read_extra(&js,run,&cs);
        if(cs.skip || cs.btag)return 2;
        pr_battle *bt=&cs.bt;pr_ai *ai=&cs.ai;
        bt->field[0]=&cs.mon[0][0];bt->field[2]=&cs.mon[1][0];
        bool ok=true;
        for(int side=0;side<2;side++)for(int k=0;k<bt->n_party[side];k++) {
            pr_pokemon *p=bt->party[side][k];int32_t stats[6],hp=p->hp;memcpy(stats,p->stats,sizeof stats);
            pr_update_passive(bt,p);pr_calculate_stats(bt,p);p->hp=hp;
            if(memcmp(stats,p->stats,sizeof stats)){fprintf(stderr,"stats mismatch case%d\n",total);ok=false;}
        }
        static pr_engine eng;memset(&eng,0,sizeof eng);eng.n_levels=1;eng.current=-1;
        for(int k=0;k<4;k++) eng.command_targets[k]=-1;
        bt->engine=&eng;
        bt->n_unported=bt->n_deferred=0;pr_ai_enemy_command(bt,ai,0);
        static const int moves[]={MOVE_ID_U_TURN,MOVE_ID_BATON_PASS,MOVE_ID_PARTING_SHOT,MOVE_ID_ROAR,MOVE_ID_THIEF,MOVE_ID_KNOCK_OFF,MOVE_ID_TACKLE,MOVE_ID_QUICK_ATTACK};
        double user=pr_ai_user_benefit_score(bt,ai,bt->field[2],bt->field[0],moves[profile]);
        double target=pr_ai_target_benefit_score(bt,ai,bt->field[2],bt->field[0],moves[profile]);
        pr_damage_result damage=ai_attack_damage(bt,bt->field[0],bt->field[2],MOVE_ID_TACKLE,false,true,false);
        static snapshot snap;take_snapshot(&snap,bt,ai);
        snap.n_match=pr_ai_party_matchup_scores(bt,ai,TRAINER_SLOT_TRAINER,true,snap.match_slot,snap.match_score);
        int next=pr_ai_next_summon_index(bt,ai,TRAINER_SLOT_TRAINER);
        if(bt->n_unported || bt->n_deferred){fprintf(stderr,"uncovered new case%d\n",total);ok=false;}
        bool seen_pre=false,seen_scores=false,seen_next=false,seen_probe=false;
        char got[LINE_LEN];
        for(int i=run;i<js.n;i++) {
            const char *l=js.s[i];
            if(!strncmp(l,"PROBE ",6)) {
                if(seen_probe)return 2;
                seen_probe=true;
                strict_int(l,"damage",0,INT32_MAX);
                if(user!=strict_double(l,"user") || target!=strict_double(l,"target") || damage.damage!=strict_double(l,"damage")){
                    snprintf(got,sizeof got,"PROBE user=%.17g target=%.17g damage=%.17g",user,target,damage.damage);ok=false;
                    if(fail<5)fprintf(stderr,"case%d JS %s C %s\n",total,l,got);
                }
            } else if(!check_line(l,bt,ai,&snap,next,&seen_pre,&seen_scores,&seen_next,got,sizeof got)) {
                ok=false;if(fail<5)fprintf(stderr,"case%d JS %s C %s\n",total,l,got);
            }
        }
        if(!seen_probe || !seen_next)return 2;
        if(!seen_pre && bt->pre_commands[2].command>=0)ok=false;
        if(!seen_scores && ai->n_move_scores)ok=false;
        total++;profiles[profile]++;states[state]++;switches+=bt->commands[2].command==COMMAND_POKEMON;scored+=ai->n_move_scores>0;held+=bt->n_mods[0]+bt->n_mods[1]>0;
        if(ok)pass++;else fail++;run=-1;
    }
    fclose(f);if(!complete)return 2;
    for(int i=0;i<8;i++)if(!profiles[i] || !states[i])return 2;
    if(!switches || !scored || !held)return 1;
    printf("cases %d: covered %d identical %d mismatches %d; switches %d scored %d held %d\n",total,total,pass,fail,switches,scored,held);
    return fail?1:0;
}
