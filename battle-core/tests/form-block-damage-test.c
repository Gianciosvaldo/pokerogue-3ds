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
/* A formatter-only template exposes the existing Pokemon argument renderer. */
static const char *display_locale(void *ctx,const char *file,int fallback,size_t *len)
{
    if(!strcmp(file,"fixture")){static const char value[]="{\"name\":\"{{x}}\"}";*len=sizeof value-1;return value;}
    return locale_get(ctx,file,fallback,len);
}
static void text_hex(const pr_msg *m,char *out)
{
    char t[1024];pr_text_source src={display_locale,NULL,"en"};size_t n=pr_msg_format(m,&src,t,sizeof t);if(n>=sizeof t)bad();if(!n){strcpy(out,"-");return;}for(size_t i=0;i<n;i++)sprintf(out+2*i,"%02x",(unsigned char)t[i]);
}
int main(int argc,char **argv)
{
    if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
    pr_closures_init();double h[3];read_vector("FORM_BLOCK ",h,3);if(h[0]!=1&&h[0]!=2)bad();
    int n=integer(h[1],h[0]==2?8704:8576,100000),failures=0,changed=0,simulations=0,forms=0,anims=0,shields=0;integer(h[2],0,INT32_MAX);
    for(int i=0;i<n;i++){
        static pr_battle bt;static pr_engine e;static pr_pokemon actors[4];memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.ignoring_effect_source=-1;e.n_levels=1;e.current=-1;bool ok=true;
        double c[6];read_vector("C ",c,6);if(c[0]!=i)bad();int u=integer(c[1],0,3),t=integer(c[2],0,3),move=integer(c[3],1,PR_MOVE_COUNT-1),sim=integer(c[4],0,1);double damage=c[5];if(damage<0||damage>2147483648.0||floor(damage)!=damage)bad();simulations+=sim;
        char wave[512],extra;if(sscanf(line_read("W "),"%511s %c",wave,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,wave);
        double ignore[2];read_vector("IGNORE ",ignore,2);bt.ignore_abilities=integer(ignore[0],0,1);bt.ignoring_effect_source=integer(ignore[1],-1,3);
        rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;double qin[3];read_vector("QIN ",qin,3);if(qin[0]!=1||qin[2]!=0)bad();e.deferred_active=integer(qin[1],0,1);
        for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&actors[k],v+1,k);pr_reset_turn_data(&actors[k]);bt.field[k]=bt.party[k/2][k%2]=&actors[k];bt.n_party[k/2]=2;ok&=compare(&actors[k],v+1,i,k,'I');ok&=boss(&actors[k],"IB ",k,true);}
        shields+=actors[t].is_boss;
        pr_ab_params pp={.pokemon=&actors[t],.opponent=&actors[u],.move=move,.simulated=sim,.value=&damage};pr_apply_ab_attrs(&bt,PR_AC_ReceivedMoveDamageMultiplierAbAttr,&pp);
        double result[1];read_vector("RESULT ",result,1);if(result[0]<0||result[0]>2147483648.0||floor(result[0])!=result[0])bad();ok&=damage==result[0];changed+=c[5]!=result[0];
        for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&actors[k],v+1,i,k,'O');ok&=boss(&actors[k],"OB ",k,false);}
        double q[3];read_vector("QCOUNT ",q,3);int count=integer(q[0],0,32),seen=0;ok&=integer(q[1],1,PR_MAX_LEVELS)==e.n_levels&&integer(q[2],0,1)==e.deferred_active;
        for(int l=0;l<e.n_levels;l++)for(int k=0;k<e.level_len[l];k++){
            const pr_phase *p=&e.pool[e.levels[l][k]];if(seen++>=count){ok=false;continue;}char kind,text[2048],actual[2048],extra;int level,order,b;double amount,result;
            if(sscanf(line_read("Q "),"%d %d %c %2047s %d %lf %lf %c",&level,&order,&kind,text,&b,&amount,&result,&extra)!=7||level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||b< -1||b>3||floor(amount)!=amount||amount< -1||amount>INT32_MAX||floor(result)!=result||result< -1||result>100)bad();ok&=level==l&&order==k;
            if(kind=='M'){if(b!=-1||amount!=-1||result!=-1||strcmp(text,"-")&&(strlen(text)%2||strspn(text,"0123456789abcdef")!=strlen(text)))bad();if(p->type!=PH_MESSAGE){ok=false;continue;}text_hex(&p->msg,actual);ok&=!strcmp(actual,text);}
            else if(kind=='F'){forms++;if(strcmp(text,"-")||b<0||amount<0||amount>1||result!=-1)bad();ok&=p->type==PH_QUIET_FORM_CHANGE&&p->battler==b&&p->pokemon==&actors[b]&&p->move_id==amount;}
            else if(kind=='A'){anims++;if(strcmp(text,"-")||b<0||amount<0||result<0)bad();ok&=p->type==PH_DAMAGE_ANIM&&p->battler==b&&p->anim_amount==amount&&p->anim_result==(int)result/2&&p->anim_critical==((int)result%2);}
            else if(kind=='S'){if(strcmp(text,"-")||b!=-1||amount!=-1||result!=-1)bad();ok&=p->type==PH_MARKER&&p->marker_of==PH_STAT_STAGE_CHANGE;}
            else if(kind=='B'){if(b<0||amount<0||amount>=PR_ABILITY_COUNT||result<0||result>3||strcmp(text,"-")&&(strlen(text)%2||strspn(text,"0123456789abcdef")!=strlen(text)))bad();ok&=p->type==PH_SHOW_ABILITY&&p->battler==b&&p->anim==amount&&p->anim_result==(int)result/2;pr_msg name=pr_msg1("fixture:name",pr_arg_pokemon(&bt,"x",&actors[b]));text_hex(&name,actual);ok&=!strcmp(text,actual)&&((int)result%2)==actors[b].on_field;}
            else if(kind=='H'){if(strcmp(text,"-")||b!=-1||amount!=-1||result!=-1)bad();ok&=p->type==PH_HIDE_ABILITY&&p->battler==-1;}
            else bad();
        }
        while(seen<count){line_read("Q ");seen++;ok=false;}
        double dc[1];read_vector("DCOUNT ",dc,1);int nd=integer(dc[0],0,16);for(int type=0;type<PH_COUNT_;type++)ok&=e.n_dyn[type]==(type==PH_STAT_STAGE_CHANGE?nd:0);
        for(int j=0;j<nd;j++){
            double v[9];read_vector("D ",v,9);if(v[0]!=j)bad();int b=integer(v[1],0,3),src=integer(v[2],0,3),self=integer(v[3],0,1),nc=integer(v[4],0,7);for(int k=5;k<9;k++)integer(v[k],0,1);
            const pr_phase *p=j<e.n_dyn[PH_STAT_STAGE_CHANGE]?&e.pool[e.dyn[PH_STAT_STAGE_CHANGE][j]]:NULL;
            if(!p)ok=false;else ok&=p->battler==b&&p->source_p==&actors[src]&&p->ssc_self_target==self&&p->n_ssc==nc&&p->ssc_ignore_abilities==v[5]&&p->ssc_processed==v[6]&&p->ssc_source_type==v[7]&&p->ssc_message==v[8];
            for(int k=0;k<nc;k++){double ch[2];read_vector("CHANGE ",ch,2);integer(ch[0],STAT_ATK,STAT_EVA);integer(ch[1],-6,12);if(p)ok&=p->ssc_stat[k]==ch[0]&&p->ssc_stages[k]==ch[1];}
        }
        pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow)ok=false;
        if(!ok){if(failures<20)fprintf(stderr,"form-block case%d u%d t%d sim%d unported%d deferred%d failed\n",i,u,t,sim,bt.n_unported,bt.n_deferred);failures++;}
    }
    double footer[1];read_vector("FORM_BLOCK_COMPLETE ",footer,1);if(footer[0]!=n||fgetc(input)!=EOF)bad();if(!failures&&(changed<100||simulations<100||forms<100||anims<100||shields<256))bad();printf("Form block: %d complete, %d exact, %d failures; changed=%d simulated=%d forms=%d animations=%d shields=%d\n",n,n-failures,failures,changed,simulations,forms,anims,shields);return failures?1:0;
}
