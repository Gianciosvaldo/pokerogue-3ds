/* Actual self-cost boosts, HP gate ordering and queued animation/stat payloads. */
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

static bool tags(pr_pokemon *p,const char *prefix,int actor,bool set)
{
    char *row=line_read(prefix),name[40],extra;int b,n,turn,move;long long source;
    if(sscanf(row,"%d %d %39s %d %d %lld %c",&b,&n,name,&turn,&move,&source,&extra)!=6||b!=actor||n<0||n>1||turn<0||turn>INT16_MAX||move<0||move>=PR_MOVE_COUNT||source< -1||source>UINT32_MAX)bad();
    if(n?strcmp(name,"ENDURING")!=0:strcmp(name,"0")!=0)bad();int type=n?BATTLER_TAG_TYPE_ENDURING:0;
    if(!n&&(turn||move||source!=-1))bad();
    if(set){p->n_tags=n;if(n)p->tags[0]=(pr_battler_tag){.type=type,.turn_count=turn,.source_move=move,.source_id=source};}
    if(p->n_tags!=n)return false;if(!n)return true;
    const pr_battler_tag *t=&p->tags[0];return t->type==type&&t->turn_count==turn&&t->source_move==move&&t->source_id==source;
}
int main(int argc,char **argv)
{
    if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
    pr_closures_init();double h[3];read_vector("HP_COST ",h,3);if(h[0]!=1&&h[0]!=2&&h[0]!=3)bad();
    int n=integer(h[1],h[0]==3?12928:h[0]==2?12800:12672,100000),failures=0,denied=0,conditions=0,shields=0,enduring=0,overrides=0,one_hp=0,faints=0,animations=0,stat_phases=0,modes[3]={0};integer(h[2],0,INT32_MAX);
    const int moves[]={MOVE_ID_BELLY_DRUM,MOVE_ID_CLANGOROUS_SOUL,MOVE_ID_FILLET_AWAY};
    for(int i=0;i<n;i++){
        static pr_battle bt;static pr_engine e;static pr_pokemon actors[4];
        memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;
        bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.ignoring_effect_source=-1;e.n_levels=1;e.current=-1;
        double c[4];read_vector("C ",c,4);if(c[0]!=i)bad();int mode=integer(c[1],0,2),u=integer(c[2],0,3),t=integer(c[3],0,3),move=moves[mode];bool ok=true;modes[mode]++;
        char wave[512],extra;char *row=line_read("W ");if(sscanf(row,"%511s %c",wave,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,wave);
        rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
        double qin[3];read_vector("QIN ",qin,3);if(qin[0]!=1||qin[2]!=0)bad();e.deferred_active=integer(qin[1],0,1);
        for(int k=0;k<4;k++){
            double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&actors[k],v+1,k);pr_reset_turn_data(&actors[k]);
            bt.field[k]=bt.party[k/2][k%2]=&actors[k];bt.n_party[k/2]=2;ok&=compare(&actors[k],v+1,i,k,'I');ok&=boss(&actors[k],"IB ",k,true);ok&=tags(&actors[k],"IT ",k,true);
        }
        one_hp+=pr_get_stat(&actors[u],STAT_HP,true)==1&&actors[u].hp==1;
        shields+=actors[u].is_boss;enduring+=actors[u].n_tags!=0;overrides+=actors[u].summon_stats[STAT_HP]!=0;
        double cond[3];read_vector("COND ",cond,3);bool fails=false;for(int j=0;j<3;j++){int expected=integer(cond[j],0,1);fails|=!expected;ok&=expected==pr_move_apply_conditions(&bt,move,&actors[u],&actors[t],j+2);}conditions+=fails;
        int ph=pr_phase_new(&e,PH_MOVE_EFFECT,u);e.pool[ph].move_id=move;
        const pr_move *mv=&pr_moves[move];int attr=-1;for(int k=0;k<mv->nattr;k++)if(pr_attrs[mv->attr0+k].cls==PR_AC_CutHpStatStageBoostAttr)attr=mv->attr0+k;if(attr<0)bad();
        pr_apply_move_effect(&bt,&e,ph,attr,&actors[u],&actors[t]);
        double ret[1];read_vector("RET ",ret,1);denied+=!integer(ret[0],0,1);
        for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&actors[k],v+1,i,k,'O');ok&=boss(&actors[k],"OB ",k,false);ok&=tags(&actors[k],"OT ",k,false);}
        double q[3];read_vector("QCOUNT ",q,3);int count=integer(q[0],0,32),seen=0;ok&=integer(q[1],1,PR_MAX_LEVELS)==e.n_levels&&integer(q[2],0,1)==e.deferred_active;
        for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){
            const pr_phase *actual=&e.pool[e.levels[l][k]];if(seen++>=count){ok=false;continue;}
            char *row=line_read("Q "),kind,ex;int level,order,b;double first,second,third;
            if(sscanf(row,"%d %d %c %d %lf %lf %lf %c",&level,&order,&kind,&b,&first,&second,&third,&ex)!=7||level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||b< -1||b>3||!isfinite(first)||!isfinite(second)||!isfinite(third))bad();
            ok&=level==l&&order==k;
            if(kind=='D'){if(b<0)bad();integer(first,0,INT32_MAX);integer(second,0,255);integer(third,0,1);animations++;ok&=actual->type==PH_DAMAGE_ANIM&&actual->battler==b&&actual->anim_amount==first&&actual->anim_result==second&&actual->anim_critical==third;}
            else if(kind=='S'){if(b!=-1||first||second||third!=-1)bad();ok&=actual->type==PH_MARKER&&actual->marker_of==PH_STAT_STAGE_CHANGE;}
            else if(kind=='F'){if(b<0||third!=-1)bad();integer(first,0,1);integer(second,-1,3);faints++;ok&=actual->type==PH_FAINT&&actual->battler==b&&actual->prevent_instant_revive==first&&actual->source==second;}
            else bad();
        }
        while(seen<count){line_read("Q ");seen++;ok=false;}
        double dc[1];read_vector("DCOUNT ",dc,1);int nd=integer(dc[0],0,4);stat_phases+=nd;
        for(int type=0;type<PH_COUNT_;type++)ok&=e.n_dyn[type]==(type==PH_STAT_STAGE_CHANGE?nd:0);
        for(int j=0;j<nd;j++){
            double v[9];read_vector("D ",v,9);if(v[0]!=j)bad();int b=integer(v[1],0,3),src=integer(v[2],-1,3),self=integer(v[3],0,1),nc=integer(v[4],0,7);for(int k=5;k<9;k++)integer(v[k],0,1);
            const pr_phase *p=j<e.n_dyn[PH_STAT_STAGE_CHANGE]?&e.pool[e.dyn[PH_STAT_STAGE_CHANGE][j]]:NULL;
            if(!p)ok=false;else ok&=p->battler==b&&p->source_p==(src<0?NULL:&actors[src])&&p->ssc_self_target==self&&p->n_ssc==nc&&p->ssc_ignore_abilities==v[5]&&p->ssc_processed==v[6]&&p->ssc_source_type==v[7]&&p->ssc_message==v[8];
            if(p&&v[8])ok&=p->move_id==MOVE_ID_BELLY_DRUM;
            for(int k=0;k<nc;k++){double ch[2];read_vector("CHANGE ",ch,2);integer(ch[0],STAT_ATK,STAT_EVA);integer(ch[1],-12,12);if(p)ok&=p->ssc_stat[k]==ch[0]&&p->ssc_stages[k]==ch[1];}
        }
        pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);
        if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow||e.n_events)ok=false;
        if(!ok){if(failures<20)fprintf(stderr,"hp-cost case%d mode%d u%d t%d failed; unported%d\n",i,mode,u,t,bt.n_unported);failures++;}
    }
    double footer[1];read_vector("HP_COST_COMPLETE ",footer,1);if(footer[0]!=n||fgetc(input)!=EOF)bad();
    if(!failures){if(h[0]>=3&&one_hp<128)bad();if(denied<100||conditions<100||shields<128||enduring<128||overrides<128||faints<100||animations!=n||stat_phases<100)bad();for(int mode=0;mode<3;mode++)if(modes[mode]<100)bad();}
    printf("HP cost: %d complete, %d exact, %d failures; denied=%d conditions=%d shields=%d enduring=%d overrides=%d oneHP=%d faints=%d animations=%d statPhases=%d\n",n,n-failures,failures,denied,conditions,shields,enduring,overrides,one_hp,faints,animations,stat_phases);return failures?1:0;
}
