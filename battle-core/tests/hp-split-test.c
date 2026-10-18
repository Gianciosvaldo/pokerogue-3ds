/* Actual HpSplitAttr dispatch, direct HP events and boss/deferred phase payloads. */
#define main gas_replay_main
#include "wide-substitute-frame.inc"
#undef main
static bool boss(pr_pokemon *p,const char *prefix,int actor,bool set)
{
    double v[5];read_vector(prefix,v,5);if(v[0]!=actor)bad();
    int b=integer(v[1],0,1),segments=integer(v[2],0,8),index=integer(v[3],0,7),trainer=integer(v[4],0,1);
    if(set){p->is_boss=b;p->boss_segments=segments;p->boss_segment_index=index;p->has_trainer=trainer;}
    return p->is_boss==b&&p->boss_segments==segments&&p->boss_segment_index==index&&p->has_trainer==trainer;
}
int main(int argc,char **argv)
{
    if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
    pr_closures_init();double h[3];read_vector("HP_SPLIT ",h,3);if(h[0]!=1&&h[0]!=2&&h[0]!=3&&h[0]!=4)bad();
    bool deferred_context=false;
    int n=integer(h[1],h[0]==4?8448:8320,100000),failures=0,denied=0,heals=0,hits=0,shields=0,faints=0;integer(h[2],0,INT32_MAX);
    for(int i=0;i<n;i++){
        static pr_battle bt;static pr_engine e;static pr_pokemon actors[4];
        memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;
        bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.ignoring_effect_source=-1;e.n_levels=1;e.current=-1;e.deferred_active=deferred_context;
        double c[3];read_vector("C ",c,3);if(c[0]!=i)bad();int u=integer(c[1],0,3),t=integer(c[2],0,3);bool ok=true;
        rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
        if(h[0]>=2){double qin[3];read_vector("QIN ",qin,3);if(qin[0]!=1||qin[2]!=0)bad();e.deferred_active=integer(qin[1],0,1);}
        for(int k=0;k<4;k++){
            double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&actors[k],v+1,k);pr_reset_turn_data(&actors[k]);
            bt.field[k]=bt.party[k/2][k%2]=&actors[k];bt.n_party[k/2]=2;ok&=compare(&actors[k],v+1,i,k,'I');ok&=boss(&actors[k],"IB ",k,true);
        }
        shields+=actors[t].is_boss;
        int ph=pr_phase_new(&e,PH_MOVE_EFFECT,u);e.pool[ph].move_id=MOVE_ID_PAIN_SPLIT;
        const pr_move *mv=&pr_moves[MOVE_ID_PAIN_SPLIT];int attr=-1;
        for(int k=0;k<mv->nattr;k++)if(pr_attrs[mv->attr0+k].cls==PR_AC_HpSplitAttr)attr=mv->attr0+k;if(attr<0)bad();
        if(h[0]>=3){double cond[1];read_vector("COND ",cond,1);ok&=integer(cond[0],0,1)==pr_move_apply_conditions(&bt,MOVE_ID_PAIN_SPLIT,&actors[u],&actors[t],4);}
        pr_apply_move_effect(&bt,&e,ph,attr,&actors[u],&actors[t]);
        double ret[1];read_vector("RET ",ret,1);denied+=!integer(ret[0],0,1);
        for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&actors[k],v+1,i,k,'O');ok&=boss(&actors[k],"OB ",k,false);}
        double ec[1];read_vector("ECOUNT ",ec,1);int count=integer(ec[0],0,2);ok&=e.n_events==count;
        for(int j=0;j<count;j++){
            double v[6];read_vector("EV ",v,6);int b=integer(v[0],0,3);integer(v[1],INT32_MIN,INT32_MAX);heals+=v[1]>0;hits+=v[1]<0;
            for(int k=2;k<6;k++)integer(v[k],0,INT32_MAX);
            if(j>=e.n_events){ok=false;continue;}const pr_event *ev=&e.events[j];ok&=ev->type==PR_EV_HP&&ev->battler==b&&ev->target==-1&&ev->id==0&&ev->value==v[1];
            for(int k=0;k<4;k++)ok&=ev->field[k].hp==v[2+k];
        }
        double q[3];read_vector("QCOUNT ",q,3);count=integer(q[0],0,32);int seen=0;ok&=integer(q[1],1,PR_MAX_LEVELS)==e.n_levels&&integer(q[2],0,1)==e.deferred_active;deferred_context=q[2];
        for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){
            const pr_phase *actual=&e.pool[e.levels[l][k]];if(seen++>=count){ok=false;continue;}
            char *row=line_read("Q "),kind,extra;int level,order,b,prevent,source;
            if(sscanf(row,"%d %d %c %d %d %d %c",&level,&order,&kind,&b,&prevent,&source,&extra)!=6||level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||b< -1||b>3||prevent<0||prevent>1||source< -1||source>3)bad();
            ok&=level==l&&order==k;
            if(kind=='S'){if(b!=-1||prevent||source!=-1)bad();ok&=actual->type==PH_MARKER&&actual->marker_of==PH_STAT_STAGE_CHANGE;}
            else if(kind=='F'){if(b<0)bad();faints++;ok&=actual->type==PH_FAINT&&actual->battler==b&&actual->prevent_instant_revive==prevent&&actual->source==source;}
            else bad();
        }
        while(seen<count){line_read("Q ");seen++;ok=false;}
        double dc[1];read_vector("DCOUNT ",dc,1);int nd=integer(dc[0],0,2);for(int type=0;type<PH_COUNT_;type++)ok&=e.n_dyn[type]==(type==PH_STAT_STAGE_CHANGE?nd:0);
        for(int j=0;j<nd;j++){
            double v[9];read_vector("D ",v,9);if(v[0]!=j)bad();int b=integer(v[1],0,3),src=integer(v[2],0,3),self=integer(v[3],0,1),nc=integer(v[4],0,7);for(int k=5;k<9;k++)integer(v[k],0,1);
            const pr_phase *p=j<e.n_dyn[PH_STAT_STAGE_CHANGE]?&e.pool[e.dyn[PH_STAT_STAGE_CHANGE][j]]:NULL;
            if(!p)ok=false;else ok&=p->battler==b&&p->source_p==&actors[src]&&p->ssc_self_target==self&&p->n_ssc==nc&&p->ssc_ignore_abilities==v[5]&&p->ssc_processed==v[6]&&p->ssc_source_type==v[7]&&p->ssc_message==v[8];
            for(int k=0;k<nc;k++){double ch[2];read_vector("CHANGE ",ch,2);integer(ch[0],STAT_ATK,STAT_EVA);integer(ch[1],-6,12);if(p)ok&=p->ssc_stat[k]==ch[0]&&p->ssc_stages[k]==ch[1];}
        }
        pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);
        if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow)ok=false;
        if(!ok){if(failures<20)fprintf(stderr,"hp-split case%d u%d t%d failed\n",i,u,t);failures++;}
    }
    double footer[1];read_vector("HP_SPLIT_COMPLETE ",footer,1);if(footer[0]!=n||fgetc(input)!=EOF)bad();
    if(!failures&&(denied<100||heals<100||hits<100||shields<128||faints<1))bad();
    printf("HP Split: %d complete, %d exact, %d failures; denied=%d heals=%d hits=%d shieldControls=%d faints=%d\n",n,n-failures,failures,denied,heals,hits,shields,faints);return failures?1:0;
}
