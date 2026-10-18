/* Registered type-changing callbacks, raw state, history, queue and both RNGs. */
#define main gas_replay_main
#include "wide-substitute-frame.inc"
#undef main
static int roll_calls,roll_range[16],roll_min[16];
int __real_pr_battle_rand_seed_int(pr_battle *,int,int);
int __wrap_pr_battle_rand_seed_int(pr_battle *bt,int range,int min)
{
    if(roll_calls<16){roll_range[roll_calls]=range;roll_min[roll_calls]=min;}roll_calls++;
    return __real_pr_battle_rand_seed_int(bt,range,min);
}
static void text_hex(const pr_msg *m,char *out)
{
    char text[1024];pr_text_source src={locale_get,NULL,"en"};size_t n=pr_msg_format(m,&src,text,sizeof text);
    if(n>=sizeof text)bad();if(!n){strcpy(out,"-");return;}for(size_t i=0;i<n;i++)sprintf(out+2*i,"%02x",(unsigned char)text[i]);
}
static bool history(pr_pokemon *p,const char *prefix,int actor,bool set)
{
    double h[2];read_vector(prefix,h,2);if(h[0]!=actor)bad();int n=integer(h[1],0,3);bool ok=true;
    if(set)p->n_move_history=n;else ok&=p->n_move_history==n;
    for(int j=0;j<n;j++){
        double v[5];read_vector("HENTRY ",v,5);int m=integer(v[0],0,PR_MOVE_COUNT-1),result=integer(v[1],0,4),mode=integer(v[2],1,10),to=integer(v[4],0,3);if(v[3]!=1)bad();
        if(set)p->move_history[j]=(pr_turn_move){.move=m,.result=result,.use_mode=mode,.n_targets=1,.targets={to}};
        else{const pr_turn_move *a=&p->move_history[j];ok&=a->move==m&&a->result==result&&a->use_mode==mode&&a->n_targets==1&&a->targets[0]==to;}
    }return ok;
}
int main(int argc,char **argv)
{
    if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
    pr_closures_init();double h[3];read_vector("TYPE_MOVES ",h,3);if(h[0]!=1&&h[0]!=2&&h[0]!=3)bad();int n=integer(h[1],h[0]==3?12672:h[0]==2?12544:12288,100000),failures=0,denied=0,condition_denied=0,messages=0,operations=0,no_choice=0,one_choice=0,modes[3]={0};integer(h[2],0,INT32_MAX);
    const int moves[]={MOVE_ID_REFLECT_TYPE,MOVE_ID_CONVERSION,MOVE_ID_CONVERSION_2},classes[]={PR_AC_CopyTypeAttr,PR_AC_FirstMoveTypeAttr,PR_AC_ResistLastMoveTypeAttr};
    for(int i=0;i<n;i++){
        static pr_battle bt;static pr_engine e;static pr_pokemon actors[4];memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.ignoring_effect_source=-1;e.n_levels=1;e.current=-1;
        double c[5];read_vector("C ",c,5);if(c[0]!=i)bad();int mode=integer(c[1],0,2),u=integer(c[2],0,3),t=integer(c[3],0,3),ops=integer(c[4],1,3),move=moves[mode];modes[mode]++;bool ok=true;
        rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
        double qin[3];read_vector("QIN ",qin,3);if(qin[0]!=1||qin[2]!=0)bad();e.deferred_active=integer(qin[1],0,1);
        for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&actors[k],v+1,k);pr_reset_turn_data(&actors[k]);bt.field[k]=bt.party[k/2][k%2]=&actors[k];bt.n_party[k/2]=2;ok&=compare(&actors[k],v+1,i,k,'I');ok&=history(&actors[k],"HI ",k,true);}
        int ph=pr_phase_new(&e,PH_MOVE_EFFECT,u);e.pool[ph].move_id=move;int attr=-1;const pr_move *mv=&pr_moves[move];for(int j=0;j<mv->nattr;j++)if(pr_attrs[mv->attr0+j].cls==classes[mode])attr=mv->attr0+j;if(attr<0)bad();
        for(int op=0;op<ops;op++){
            operations++;double cond[3];read_vector("COND ",cond,3);bool fails=false;for(int j=0;j<3;j++){int want=integer(cond[j],0,1);ok&=want==pr_move_apply_conditions(&bt,move,&actors[u],&actors[t],j+2);fails|=!want;}condition_denied+=fails;
            roll_calls=0;pr_apply_move_effect(&bt,&e,ph,attr,&actors[u],&actors[t]);double ret[1];read_vector("RET ",ret,1);denied+=!integer(ret[0],0,1);
            if(h[0]>=2){double calls[1];read_vector("CALLS ",calls,1);int nc=integer(calls[0],0,16);ok&=nc==roll_calls;for(int j=0;j<nc;j++){double r[2];read_vector("CALL ",r,2);int range=integer(r[0],1,PR_TYPE_COUNT),min=integer(r[1],0,0);if(j<roll_calls)ok&=range==roll_range[j]&&min==roll_min[j];}if(op==0&&i>=12288&&i<12416)no_choice+=!ret[0]&&nc==0;if(op==0&&i>=12416)one_choice+=ret[0]&&nc==1&&roll_range[0]==1;}

            for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&actors[k],v+1,i,k,'O');ok&=history(&actors[k],"HO ",k,false);}
            double q[3];read_vector("QCOUNT ",q,3);int count=integer(q[0],0,3),seen=0;messages+=count;ok&=integer(q[1],1,PR_MAX_LEVELS)==e.n_levels&&integer(q[2],0,1)==e.deferred_active;
            for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++){
                const pr_phase *p=&e.pool[e.levels[l][j]];if(seen++>=count){ok=false;continue;}char expected[2048],actual[2048],extra;int level,order;
                if(sscanf(line_read("Q "),"%d %d %2047s %c",&level,&order,expected,&extra)!=3||level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||strcmp(expected,"-")&&(strlen(expected)%2||strspn(expected,"0123456789abcdef")!=strlen(expected)))bad();
                ok&=level==l&&order==j&&p->type==PH_MESSAGE;if(p->type==PH_MESSAGE){text_hex(&p->msg,actual);ok&=!strcmp(expected,actual);}
            }
            while(seen<count){line_read("Q ");seen++;ok=false;}
            for(int type=0;type<PH_COUNT_;type++)ok&=e.n_dyn[type]==0;
        }
        pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow||e.n_events)ok=false;
        if(!ok){if(failures<20)fprintf(stderr,"type-move case%d mode%d u%d t%d failed unported%d\n",i,mode,u,t,bt.n_unported);failures++;}
    }
    double end[1];read_vector("TYPE_MOVES_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();if(!failures){if(h[0]>=2&&(no_choice<128||one_choice<128))bad();if(denied<100||condition_denied<100||messages<100)bad();for(int j=0;j<3;j++)if(modes[j]<100)bad();}
    printf("Type moves: %d complete, %d exact, %d failures, %d operations, denied=%d conditionDenied=%d messages=%d\n",n,n-failures,failures,operations,denied,condition_denied,messages);return failures?1:0;
}
