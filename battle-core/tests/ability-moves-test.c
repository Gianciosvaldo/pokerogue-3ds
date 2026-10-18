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
    pr_closures_init();double h[3];read_vector("ABILITY_MOVES ",h,3);if(h[0]!=2&&h[0]!=3&&h[0]!=4)bad();
    int n=integer(h[1],h[0]==4?12544:h[0]==3?12416:12288,100000),failures=0,modes[6]={0},denied=0,condition_denied=0,form_queues=0;integer(h[2],0,INT32_MAX);
    const int moves[]={MOVE_ID_WORRY_SEED,MOVE_ID_SIMPLE_BEAM,MOVE_ID_ROLE_PLAY,MOVE_ID_DOODLE,MOVE_ID_ENTRAINMENT,MOVE_ID_SKILL_SWAP};
    const int classes[]={PR_AC_AbilityChangeAttr,PR_AC_AbilityChangeAttr,PR_AC_AbilityCopyAttr,PR_AC_AbilityCopyAttr,PR_AC_AbilityGiveAttr,PR_AC_SwitchAbilitiesAttr};
    for(int i=0;i<n;i++){
        static pr_battle bt;static pr_engine e;static pr_pokemon actors[4];
        memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;
        bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.ignoring_effect_source=-1;e.n_levels=1;e.current=-1;
        double c[4];read_vector("C ",c,4);if(c[0]!=i)bad();int mode=integer(c[1],0,5),u=integer(c[2],0,3),t=integer(c[3],0,3),move=moves[mode];bool ok=true;modes[mode]++;
        char wave[512],extra;char *row=line_read("W ");if(sscanf(row,"%511s %c",wave,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,wave);
        if(h[0]>=4){double world[3];read_vector("WI ",world,3);bt.weather=integer(world[0],0,9);bt.weather_turns_left=integer(world[1],0,100);bt.weather_max_duration=integer(world[2],0,100);}
        rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;
        for(int k=0;k<4;k++){
            double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&actors[k],v+1,k);pr_reset_turn_data(&actors[k]);
            bt.field[k]=bt.party[k/2][k%2]=&actors[k];bt.n_party[k/2]=2;ok&=compare(&actors[k],v+1,i,k,'I');
        }
        double cond[1];read_vector("COND ",cond,1);int condition=integer(cond[0],0,1);condition_denied+=!condition;ok&=condition==pr_move_apply_conditions(&bt,move,&actors[u],&actors[t],4);
        const pr_move *mv=&pr_moves[move];int attr=-1;for(int k=0;k<mv->nattr;k++)if(pr_attrs[mv->attr0+k].cls==classes[mode])attr=mv->attr0+k;if(attr<0)bad();
        int ph=pr_phase_new(&e,PH_MOVE_EFFECT,u);e.pool[ph].move_id=move;pr_apply_move_effect(&bt,&e,ph,attr,&actors[u],&actors[t]);
        double ret[1];read_vector("RET ",ret,1);denied+=!integer(ret[0],0,1);
        for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&actors[k],v+1,i,k,'O');}
        if(h[0]>=4){double world[3];read_vector("WO ",world,3);for(int k=0;k<3;k++)integer(world[k],0,100);ok&=bt.weather==world[0]&&bt.weather_turns_left==world[1]&&bt.weather_max_duration==world[2];}
        double q[3];read_vector("QCOUNT ",q,3);int count=integer(q[0],0,32),seen=0;ok&=integer(q[1],1,PR_MAX_LEVELS)==e.n_levels&&integer(q[2],0,1)==e.deferred_active;
        for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){
            int ix=e.levels[l][k];if(seen++>=count){ok=false;continue;}
            char *row=line_read("Q "),expected[2048],actual[2048],kind='M',extra;int level,order,b=-1,form=-1;
            if(h[0]==2){if(sscanf(row,"%d %d %2047s %c",&level,&order,expected,&extra)!=3)bad();}
            else if(sscanf(row,"%d %d %c %2047s %d %d %c",&level,&order,&kind,expected,&b,&form,&extra)!=6)bad();
            if(level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES)bad();ok&=level==l&&order==k;
            if(kind=='M'){
                if(b!=-1||form!=-1)bad();
                if(strcmp(expected,"-")){size_t len=strlen(expected);if(len%2)bad();for(size_t ch=0;ch<len;ch++)if(!isxdigit((unsigned char)expected[ch]))bad();}
                if(e.pool[ix].type!=PH_MESSAGE){ok=false;continue;}
                text_hex(&e.pool[ix].msg,actual);ok&=!strcmp(expected,actual);
            }else if(kind=='F'){
                form_queues++;
                if(strcmp(expected,"-")||b<0||b>3||form<0||form>3)bad();
                ok&=e.pool[ix].type==PH_QUIET_FORM_CHANGE&&e.pool[ix].battler==b&&e.pool[ix].pokemon==&actors[b]&&e.pool[ix].move_id==form;
            }else if(kind=='K'){
                if(strcmp(expected,"CommonAnimPhase")||b!=-1||form!=-1)bad();ok&=e.pool[ix].type==PH_MARKER&&e.pool[ix].marker_of==PH_COMMON_ANIM;
            }else if(kind=='A'){
                if(strcmp(expected,"-")||b<0||b>3||form<0||form>=PR_ABILITY_COUNT*2)bad();
                ok&=e.pool[ix].type==PH_SHOW_ABILITY&&e.pool[ix].battler==b&&e.pool[ix].anim==form/2&&e.pool[ix].anim_result==form%2;
            }else if(kind=='H'){
                if(strcmp(expected,"-")||b!=-1||form!=-1)bad();ok&=e.pool[ix].type==PH_HIDE_ABILITY&&e.pool[ix].battler==-1;
            }else if(kind=='C'){
                int target;char ex;if(sscanf(expected,"T%d%c",&target,&ex)!=1||target< -1||target>3||b<0||b>3||form<0||form>UINT16_MAX)bad();
                ok&=e.pool[ix].type==PH_COMMON_ANIM&&e.pool[ix].battler==b&&e.pool[ix].anim_target==target&&e.pool[ix].anim==form;
            }else bad();
        }
        while(seen<count){line_read("Q ");seen++;ok=false;}
        int nd=0;
        if(h[0]>=4){double dc[1];read_vector("DCOUNT ",dc,1);nd=integer(dc[0],0,16);for(int j=0;j<nd;j++){
            char *row=line_read("D "),kind,extra;int index,target,b,anim;
            if(sscanf(row,"%d %c T%d %d %d %c",&index,&kind,&target,&b,&anim,&extra)!=5||index!=j||kind!='C'||target< -1||target>3||b<0||b>3||anim<0||anim>UINT16_MAX)bad();
            if(j>=e.n_dyn[PH_COMMON_ANIM]){ok=false;continue;}const pr_phase *p=&e.pool[e.dyn[PH_COMMON_ANIM][j]];
            ok&=p->type==PH_COMMON_ANIM&&p->battler==b&&p->anim_target==target&&p->anim==anim;
        }}
        for(int type=0;type<PH_COUNT_;type++)ok&=e.n_dyn[type]==(type==PH_COMMON_ANIM?nd:0);
        for(int k=0;k<4;k++)pr_reset_summon_data(&actors[k]);
        for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("Z ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&actors[k],v+1,i,k,'Z');}
        pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);
        if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow)ok=false;
        if(!ok){if(failures<20)fprintf(stderr,"ability-moves case%d mode%d u%d t%d failed; unported=%d\n",i,mode,u,t,bt.n_unported);failures++;}
    }
    double footer[1];read_vector("ABILITY_MOVES_COMPLETE ",footer,1);if(footer[0]!=n||fgetc(input)!=EOF)bad();
    if(!failures){for(int mode=0;mode<6;mode++)if(modes[mode]<100)bad();if(denied<100||condition_denied<100||(h[0]>=3&&form_queues<100))bad();}
    printf("Ability moves: %d complete, %d exact, %d failures; denied=%d conditionDenied=%d\n",n,n-failures,failures,denied,condition_denied);return failures?1:0;
}
