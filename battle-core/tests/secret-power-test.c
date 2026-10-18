/* Actual Secret Power children, status/tag/arena state and queued payloads. */
#define main gas_replay_main
#include "wide-substitute-frame.inc"
#undef main
static int roll_calls,roll_range[8],roll_min[8];
int __real_pr_battle_rand_seed_int(pr_battle *,int,int);
int __wrap_pr_battle_rand_seed_int(pr_battle *bt,int range,int min)
{
    if(roll_calls<8){roll_range[roll_calls]=range;roll_min[roll_calls]=min;}roll_calls++;
    return __real_pr_battle_rand_seed_int(bt,range,min);
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
static bool status_read(pr_pokemon *p,const char *prefix,int actor,bool set)
{
    double v[6];read_vector(prefix,v,6);if(v[0]!=actor)bad();int s=integer(v[1],0,7),toxic=integer(v[2],0,INT16_MAX),sleep=integer(v[3],-1,INT16_MAX),freeze=integer(v[4],-1,INT16_MAX),pending=integer(v[5],0,7);
    if(set){p->status=s;p->status_toxic_turns=toxic;p->status_sleep_turns=sleep;p->status_freeze_turns=freeze;p->pending_status=pending;}
    return p->status==s&&p->status_toxic_turns==toxic&&p->status_sleep_turns==sleep&&p->status_freeze_turns==freeze&&p->pending_status==pending;
}
static bool tags_read(pr_pokemon *p,const char *prefix,int actor,bool set)
{
    int who,n,turn,move;long long source;char type[40],extra;
    if(sscanf(line_read(prefix),"%d %d %39s %d %d %lld %c",&who,&n,type,&turn,&move,&source,&extra)!=6||who!=actor||n<0||n>1||turn<0||turn>INT16_MAX||move<0||move>=PR_MOVE_COUNT||source< -1||source>UINT32_MAX)bad();
    if(n?strcmp(type,"FLINCHED")!=0:strcmp(type,"0")!=0)bad();if(!n&&(turn||move||source!=-1))bad();
    if(set){p->n_tags=n;if(n)p->tags[0]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_FLINCHED,.turn_count=turn,.source_move=move,.source_id=source};}
    if(p->n_tags!=n)return false;if(!n)return true;const pr_battler_tag *a=&p->tags[0];return a->type==BATTLER_TAG_TYPE_FLINCHED&&a->turn_count==turn&&a->source_move==move&&a->source_id==source;
}
static bool arena_read(pr_battle *bt,const char *prefix,bool set)
{
    int n,side,turn,max,move;long long source;char type[40],extra;
    if(sscanf(line_read(prefix),"%d %39s %d %d %d %d %lld %c",&n,type,&side,&turn,&max,&move,&source,&extra)!=7||n<0||n>1||side<0||side>2||turn<0||turn>INT16_MAX||max<0||max>INT16_MAX||move<0||move>=PR_MOVE_COUNT||source< -1||source>UINT32_MAX)bad();
    if(n?strcmp(type,"WATER_FIRE_PLEDGE")!=0:strcmp(type,"0")!=0)bad();if(!n&&(side||turn||max||move||source!=-1))bad();
    if(set){bt->n_arena_tags=n;if(n)bt->arena_tags[0]=(pr_arena_tag){.type=ARENA_TAG_TYPE_WATER_FIRE_PLEDGE,.side=side,.turn_count=turn,.max_duration=max,.source_move=move,.source_id=source,.layers=1};}
    if(bt->n_arena_tags!=n)return false;if(!n)return true;const pr_arena_tag *a=&bt->arena_tags[0];return a->type==ARENA_TAG_TYPE_WATER_FIRE_PLEDGE&&a->side==side&&a->turn_count==turn&&a->max_duration==max&&a->source_move==move&&a->source_id==source;
}
static int variant(void)
{
    char name[24],extra;if(sscanf(line_read("CHILD "),"%23s %c",name,&extra)!=1)bad();if(!strcmp(name,"N"))return -1;if(!strcmp(name,"F"))return 9;
    const int status[]={STATUS_EFFECT_PARALYSIS,STATUS_EFFECT_SLEEP,STATUS_EFFECT_FREEZE,STATUS_EFFECT_BURN},stats[]={STAT_ATK,STAT_DEF,STAT_SPATK,STAT_SPD,STAT_ACC};int v;char kind;if(sscanf(name,"%c%d%c",&kind,&v,&extra)!=2)bad();if(kind=='P')for(int j=0;j<4;j++)if(v==status[j])return j;if(kind=='S')for(int j=0;j<5;j++)if(v==stats[j])return j+4;bad();return -1;
}
int main(int argc,char **argv)
{
    if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;pr_closures_init();double h[3];read_vector("SECRET_POWER ",h,3);if(h[0]!=1&&h[0]!=2)bad();int n=integer(h[1],12288,100000),failures=0,denied=0,success=0,rainbows=0,faint_targets=0,positive[10]={0};integer(h[2],0,INT32_MAX);
    bool biomes[51]={0},terrains[5]={0},positive_biomes[51]={0},positive_terrains[5]={0};int attr=-1;const pr_move *mv=&pr_moves[MOVE_ID_SECRET_POWER];for(int j=0;j<mv->nattr;j++)if(pr_attrs[mv->attr0+j].cls==PR_AC_SecretPowerAttr)attr=mv->attr0+j;if(attr<0)bad();
    for(int i=0;i<n;i++){
        static pr_battle bt;static pr_engine e;static pr_pokemon actors[4];memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.ignoring_effect_source=-1;e.n_levels=1;e.current=-1;bool ok=true;
        double c[3];read_vector("C ",c,3);if(c[0]!=i)bad();int u=integer(c[1],0,3),t=integer(c[2],0,3);char wave[512],extra;if(sscanf(line_read("W "),"%511s %c",wave,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,wave);
        double w[4];read_vector("WORLD ",w,4);bt.biome=integer(w[0],0,50);bt.has_biome=true;bt.terrain=integer(w[1],0,4);bt.terrain_turns_left=integer(w[2],0,100);bt.terrain_max_duration=integer(w[3],0,100);biomes[bt.biome]=true;terrains[bt.terrain]=true;ok&=arena_read(&bt,"AI ",true);rainbows+=bt.n_arena_tags!=0;
        rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;double qin[3];read_vector("QIN ",qin,3);if(qin[0]!=1||qin[2]!=0)bad();e.deferred_active=integer(qin[1],0,1);
        for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&actors[k],v+1,k);pr_reset_turn_data(&actors[k]);bt.field[k]=bt.party[k/2][k%2]=&actors[k];bt.n_party[k/2]=2;ok&=compare(&actors[k],v+1,i,k,'I');ok&=status_read(&actors[k],"SI ",k,true);ok&=tags_read(&actors[k],"TI ",k,true);}faint_targets+=actors[t].hp==0;
        int ph=pr_phase_new(&e,PH_MOVE_EFFECT,u);e.pool[ph].move_id=MOVE_ID_SECRET_POWER;roll_calls=0;pr_apply_move_effect(&bt,&e,ph,attr,&actors[u],&actors[t]);int kind=variant();double ret[1];read_vector("RET ",ret,1);int result=integer(ret[0],0,1);denied+=!result;success+=result;if(h[0]>=2){double calls[1];read_vector("CALLS ",calls,1);int nc=integer(calls[0],0,8);ok&=nc==roll_calls;for(int j=0;j<nc;j++){double v[2];read_vector("CALL ",v,2);int range=integer(v[0],1,100),min=integer(v[1],0,0);if(j<roll_calls)ok&=roll_range[j]==range&&roll_min[j]==min;}}if(result){if(kind<0)bad();positive[kind]++;positive_terrains[bt.terrain]=true;if(bt.terrain==0)positive_biomes[bt.biome]=true;}
        for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&actors[k],v+1,i,k,'O');ok&=status_read(&actors[k],"SO ",k,false);ok&=tags_read(&actors[k],"TO ",k,false);}ok&=arena_read(&bt,"AO ",false);
        double q[3];read_vector("QCOUNT ",q,3);int count=integer(q[0],0,16),seen=0;ok&=integer(q[1],1,PR_MAX_LEVELS)==e.n_levels&&integer(q[2],0,1)==e.deferred_active;
        for(int l=0;l<e.n_levels;l++)for(int j=0;j<e.level_len[l];j++){
            const pr_phase *p=&e.pool[e.levels[l][j]];if(seen++>=count){ok=false;continue;}int level,order,b,a,src,sleep;char kind,text[2048],actual[2048],ex;
            if(sscanf(line_read("Q "),"%d %d %c %d %d %d %d %2047s %c",&level,&order,&kind,&b,&a,&src,&sleep,text,&ex)!=8||level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES||b< -1||b>3||a< -1||a>=PR_ABILITY_COUNT||src< -1||src>3||sleep< -1||sleep>3||strcmp(text,"-")&&(strlen(text)%2||strspn(text,"0123456789abcdef")!=strlen(text)))bad();ok&=level==l&&order==j;
            if(kind=='S'||kind=='T'){if(b!=-1||a!=-1||src!=-1||sleep!=-1||strcmp(text,"-"))bad();ok&=p->type==PH_MARKER&&p->marker_of==(kind=='S'?PH_STAT_STAGE_CHANGE:PH_OBTAIN_STATUS_EFFECT);}
            else if(kind=='M'){if(b!=-1||a!=-1||src!=-1||sleep!=-1)bad();ok&=p->type==PH_MESSAGE;if(p->type==PH_MESSAGE){text_hex(&p->msg,actual);ok&=!strcmp(text,actual);}}
            else if(kind=='H'){if(b!=-1||a!=-1||src!=-1||sleep!=-1||strcmp(text,"-"))bad();ok&=p->type==PH_HIDE_ABILITY&&p->battler==-1;}
            else if(kind=='A'){if(b<0||src<0||src>1||sleep<0||sleep>1)bad();ok&=p->type==PH_SHOW_ABILITY&&p->battler==b&&p->anim==a&&p->anim_result==src;pr_msg name=pr_msg1("fixture:name",pr_arg_pokemon(&bt,"x",&actors[b]));text_hex(&name,actual);ok&=!strcmp(text,actual)&&sleep==actors[b].on_field;}
            else if(kind=='O'){if(b<0||a<1||a>7)bad();ok&=p->type==PH_OBTAIN_STATUS_EFFECT&&p->battler==b&&p->ose_effect==a&&p->source_p==(src<0?NULL:&actors[src])&&p->ose_sleep_turns==sleep;text_hex(&p->msg,actual);ok&=!strcmp(text,actual);}
            else bad();
        }
        while(seen<count){line_read("Q ");seen++;ok=false;}
        double dc[1];read_vector("DCOUNT ",dc,1);int nd=integer(dc[0],0,4),observed[PH_COUNT_]={0},positions[PH_COUNT_]={0};
        for(int j=0;j<nd;j++){
            char row[2048],tag,ex;int index,b;char *raw=line_read("D ");if(sscanf(raw,"%d %c %d",&index,&tag,&b)!=3||index!=j||b<0||b>3)bad();int type=tag=='S'?PH_STAT_STAGE_CHANGE:tag=='O'?PH_OBTAIN_STATUS_EFFECT:-1;if(type<0)bad();int at=positions[type]++;observed[type]++;const pr_phase *p=at<e.n_dyn[type]?&e.pool[e.dyn[type][at]]:NULL;
            if(p)ok&=p->type==type;
            if(tag=='S'){int src,self,nc,ignore,processed,source,message;if(sscanf(raw,"%d %c %d %d %d %d %d %d %d %d %c",&index,&tag,&b,&src,&self,&nc,&ignore,&processed,&source,&message,&ex)!=10||src< -1||src>3||self<0||self>1||nc<0||nc>7||ignore<0||ignore>1||processed<0||processed>1||source<0||source>1||message<0||message>1)bad();if(!p)ok=false;else ok&=p->battler==b&&p->source_p==(src<0?NULL:&actors[src])&&p->ssc_self_target==self&&p->n_ssc==nc&&p->ssc_ignore_abilities==ignore&&p->ssc_processed==processed&&p->ssc_source_type==source&&p->ssc_message==message;for(int k=0;k<nc;k++){double v[2];read_vector("CHANGE ",v,2);int stat=integer(v[0],1,7),stages=integer(v[1],-12,12);if(p)ok&=p->ssc_stat[k]==stat&&p->ssc_stages[k]==stages;}}
            else{int effect,src,sleep;if(sscanf(raw,"%d %c %d %d %d %d %2047s %c",&index,&tag,&b,&effect,&src,&sleep,row,&ex)!=7||effect<1||effect>7||src< -1||src>3||sleep< -1||sleep>9||strcmp(row,"-")&&(strlen(row)%2||strspn(row,"0123456789abcdef")!=strlen(row)))bad();if(!p)ok=false;else{char actual[2048];text_hex(&p->msg,actual);ok&=p->battler==b&&p->ose_effect==effect&&p->source_p==(src<0?NULL:&actors[src])&&p->ose_sleep_turns==sleep&&!strcmp(row,actual);}}
        }
        for(int type=0;type<PH_COUNT_;type++)ok&=e.n_dyn[type]==observed[type];pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow||e.n_events)ok=false;
        if(!ok){if(failures<20)fprintf(stderr,"secret-power case%d u%d t%d child%d failed unported%d\n",i,u,t,kind,bt.n_unported);failures++;}
    }
    double end[1];read_vector("SECRET_POWER_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();if(!failures){int nb=0;for(int b=0;b<51;b++)nb+=biomes[b];if(nb!=35||denied<100||success<100||rainbows<100||faint_targets<100)bad();for(int b=0;b<51;b++)if(biomes[b]&&!positive_biomes[b])bad();for(int j=0;j<5;j++)if(!terrains[j]||!positive_terrains[j])bad();for(int j=0;j<10;j++)if(positive[j]<1)bad();}
    printf("Secret Power: %d complete, %d exact, %d failures; denied=%d success=%d rainbows=%d faintTargets=%d\n",n,n-failures,failures,denied,success,rainbows,faint_targets);return failures?1:0;
}
