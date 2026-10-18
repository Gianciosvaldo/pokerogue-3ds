/* Strict actual override queue/state replay, including zero-change phases. */
#define main gas_replay_main
#include "wide-substitute-frame.inc"
#undef main
static void text_hex(const pr_msg *m,char *out)
{
    char text[1024];pr_text_source src={locale_get,NULL,"en"};size_t n=pr_msg_format(m,&src,text,sizeof text);
    if(n>=sizeof text)bad();if(!n){strcpy(out,"-");return;}for(size_t i=0;i<n;i++)sprintf(out+2*i,"%02x",(unsigned char)text[i]);
}
int main(int argc,char **argv)
{
    if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
    pr_closures_init();double h[3];read_vector("SPECTRAL_THIEF ",h,3);if(h[0]!=1)bad();
    int n=integer(h[1],8192,100000),failures=0,empty=0,zero=0,queued=0;integer(h[2],0,INT32_MAX);
    for(int i=0;i<n;i++){
        static pr_battle bt;static pr_engine e;static pr_pokemon actors[4];
        memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;
        bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.ignoring_effect_source=-1;e.n_levels=1;e.current=-1;
        double c[3];read_vector("C ",c,3);if(c[0]!=i)bad();int u=integer(c[1],0,3),t=integer(c[2],0,3);bool ok=true;
        rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
        for(int k=0;k<4;k++){
            double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&actors[k],v+1,k);pr_reset_turn_data(&actors[k]);
            bt.field[k]=bt.party[k/2][k%2]=&actors[k];bt.n_party[k/2]=2;ok&=compare(&actors[k],v+1,i,k,'I');
        }
        pr_apply_pre_damage_stat_changes(&bt,&e,&actors[u],&actors[t],MOVE_ID_SPECTRAL_THIEF);
        for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&actors[k],v+1,i,k,'O');}
        double q[3];read_vector("QCOUNT ",q,3);int count=integer(q[0],0,32),seen=0;
        ok&=integer(q[1],1,PR_MAX_LEVELS)==e.n_levels&&integer(q[2],0,1)==e.deferred_active;
        for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){
            const pr_phase *actual=&e.pool[e.levels[l][k]];if(seen++>=count){ok=false;continue;}
            char *row=line_read("Q "),kind,expected[2048],text[2048],extra;int level,order;
            if(sscanf(row,"%d %d %c %2047s %c",&level,&order,&kind,expected,&extra)!=4||level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES)bad();
            ok&=level==l&&order==k;
            if(kind=='S')ok&=!strcmp(expected,"-")&&actual->type==PH_MARKER&&actual->marker_of==PH_STAT_STAGE_CHANGE;
            else if(kind=='M'){if(actual->type!=PH_MESSAGE)ok=false;else{text_hex(&actual->msg,text);ok&=!strcmp(text,expected);}}
            else bad();
        }
        while(seen<count){line_read("Q ");seen++;ok=false;}
        double dc[1];read_vector("DCOUNT ",dc,1);int nd=integer(dc[0],0,7);empty+=nd==0;queued+=nd;
        for(int type=0;type<PH_COUNT_;type++)ok&=e.n_dyn[type]==(type==PH_STAT_STAGE_CHANGE?nd:0);
        for(int j=0;j<nd;j++){
            double d[11];read_vector("D ",d,11);if(d[0]!=j)bad();
            int b=integer(d[1],0,3),source=integer(d[2],0,3),self=integer(d[3],0,1),changes=integer(d[4],0,1);
            int stat=integer(d[5],changes?STAT_ATK:0,changes?STAT_EVA:0),stages=integer(d[6],0,changes?6:0);
            for(int k=7;k<11;k++)integer(d[k],0,1);zero+=changes==0;
            if(j>=e.n_dyn[PH_STAT_STAGE_CHANGE]){ok=false;continue;}
            const pr_phase *p=&e.pool[e.dyn[PH_STAT_STAGE_CHANGE][j]];
            ok&=p->type==PH_STAT_STAGE_CHANGE&&p->battler==b&&p->source_p==&actors[source]&&p->ssc_self_target==self&&p->n_ssc==changes
                &&p->ssc_ignore_abilities==d[7]&&p->ssc_processed==d[8]&&p->ssc_source_type==d[9]&&p->ssc_message==d[10];
            if(changes)ok&=p->ssc_stat[0]==stat&&p->ssc_stages[0]==stages;
        }
        pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);
        if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow)ok=false;
        if(!ok){if(failures<20)fprintf(stderr,"spectral case%d u%d t%d failed\n",i,u,t);failures++;}
    }
    double footer[1];read_vector("SPECTRAL_THIEF_COMPLETE ",footer,1);if(footer[0]!=n||fgetc(input)!=EOF)bad();
    if(!failures&&(empty<100||zero<100||queued<100))bad();
    printf("Spectral Thief: %d complete, %d exact, %d failures, empty=%d zero=%d queued=%d\n",n,n-failures,failures,empty,zero,queued);return failures?1:0;
}
