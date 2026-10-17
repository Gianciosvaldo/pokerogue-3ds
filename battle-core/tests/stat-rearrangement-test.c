/* Replay actual owning attributes, including repeated calls and rejected gates. */
#define main gas_replay_main
#include "wide-substitute-frame.inc"
#undef main
static void text_hex(const pr_msg *m,char *out)
{
    char text[1024];pr_text_source src={locale_get,NULL,"en"};
    size_t n=pr_msg_format(m,&src,text,sizeof text);if(n>=sizeof text)bad();
    if(!n){strcpy(out,"-");return;}
    for(size_t i=0;i<n;i++)sprintf(out+2*i,"%02x",(unsigned char)text[i]);
}
int main(int argc,char **argv)
{
    if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
    pr_closures_init();double h[3];read_vector("STAT_REARRANGE ",h,3);
    if(h[0]!=1)bad();int n=integer(h[1],12288,100000),failures=0;
    integer(h[2],0,INT32_MAX);
    const int moves[]={MOVE_ID_SPEED_SWAP,MOVE_ID_POWER_SHIFT,MOVE_ID_TOPSY_TURVY};
    const int classes[]={PR_AC_SwapStatAttr,PR_AC_ShiftStatAttr,PR_AC_InvertStatsAttr};
    int outcomes[3][2]={{0}},relations[3][3]={{0}},repeated[3]={0};
    for(int i=0;i<n;i++){
        static pr_battle bt;static pr_engine e;static pr_pokemon actors[4];
        memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;
        bt.double_battle=true;bt.game_mode=GAME_MODES_ENDLESS;bt.turn=1;
        bt.wave_index=201;bt.last_used_pokeball=-1;bt.ignoring_effect_source=-1;
        e.n_levels=1;e.current=-1;
        double c[5];read_vector("C ",c,5);if(c[0]!=i)bad();
        int mode=integer(c[1],0,2),u=integer(c[2],0,3),t=integer(c[3],0,3);
        int repeats=integer(c[4],1,3);bool ok=true;
        rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);
        bt.rng_live=bt.battle_rng_live=true;
        for(int k=0;k<4;k++){
            double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();
            load(&actors[k],v+1,k);pr_reset_turn_data(&actors[k]);
            bt.field[k]=bt.party[k/2][k%2]=&actors[k];bt.n_party[k/2]=2;
            ok&=compare(&actors[k],v+1,i,k,'I');
        }
        int attr=-1;const pr_move *move=&pr_moves[moves[mode]];
        for(int j=0;j<move->nattr;j++)if(pr_attrs[move->attr0+j].cls==classes[mode]){
            if(attr>=0)bad();attr=move->attr0+j;
        }
        if(attr<0)bad();
        int ph=pr_phase_new(&e,PH_MOVE_EFFECT,u);e.current=ph;
        e.pool[ph].move_id=moves[mode];e.pool[ph].move.id=moves[mode];
        outcomes[mode][actors[t].hp>0]++;relations[mode][u==t?0:u/2==t/2?1:2]++;
        repeated[mode]+=repeats>1;
        for(int j=0;j<repeats;j++)pr_apply_move_effect(&bt,&e,ph,attr,&actors[u],&actors[t]);
        for(int k=0;k<4;k++){
            double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();
            ok&=compare(&actors[k],v+1,i,k,'O');
        }
        double q[3];read_vector("QCOUNT ",q,3);int count=integer(q[0],0,32),seen=0;
        if(integer(q[1],1,PR_MAX_LEVELS)!=e.n_levels||integer(q[2],0,1)!=e.deferred_active)ok=false;
        for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){
            int ix=e.levels[l][k];if(e.pool[ix].type!=PH_MESSAGE)bad();
            if(seen++>=count){ok=false;continue;}
            char *row=line_read("Q "),text[2048],expected[2048],extra;int level,order;
            if(sscanf(row,"%d %d %2047s %c",&level,&order,expected,&extra)!=3
                ||level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES)bad();
            text_hex(&e.pool[ix].msg,text);
            if(level!=l||order!=k||strcmp(expected,text))ok=false;
        }
        while(seen<count){line_read("Q ");seen++;ok=false;}
        pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);
        rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);
        if(strcmp(line_read("E"),"\n"))bad();
        if(bt.n_unported||bt.n_deferred||e.overflow)ok=false;
        if(!ok){if(failures<20)fprintf(stderr,"stat case%d mode%d u%d t%d failed\n",i,mode,u,t);failures++;}
    }
    double end[1];read_vector("STAT_REARRANGE_COMPLETE ",end,1);
    if(end[0]!=n||fgetc(input)!=EOF)bad();
    if(!failures)for(int m=0;m<3;m++){
        for(int j=0;j<2;j++)if(outcomes[m][j]<100)bad();
        for(int j=0;j<3;j++)if(relations[m][j]<100)bad();
        if(repeated[m]<100)bad();
    }
    printf("Stat rearrangement: %d complete, %d exact, %d failures\n",n,n-failures,failures);
    return failures?1:0;
}
