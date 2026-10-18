/* Actual Pokemon.addTag/overlap/remove and native highest-stat observations. */
#define main gas_replay_main
#include "wide-substitute-frame.inc"
#undef main
static void text_hex(const pr_msg *m,char *out)
{
    char text[1024];pr_text_source src={locale_get,NULL,"en"};size_t n=pr_msg_format(m,&src,text,sizeof text);
    if(n>=sizeof text)bad();if(!n){strcpy(out,"-");return;}
    for(size_t i=0;i<n;i++)sprintf(out+2*i,"%02x",(unsigned char)text[i]);
}
static bool queues(pr_engine *e)
{
    double q[3];read_vector("QCOUNT ",q,3);int count=integer(q[0],0,32),seen=0;bool ok=true;
    ok&=integer(q[1],1,PR_MAX_LEVELS)==e->n_levels&&integer(q[2],0,1)==e->deferred_active;
    for(int l=0;l<e->n_levels;l++)for(int k=0;k<e->level_len[l];k++){
        int ix=e->levels[l][k];if(e->pool[ix].type!=PH_MESSAGE)bad();
        if(seen++>=count){ok=false;continue;}
        char *row=line_read("Q "),expected[2048],actual[2048],extra;int level,order;
        if(sscanf(row,"%d %d %2047s %c",&level,&order,expected,&extra)!=3||level<0||level>=PR_MAX_LEVELS||order<0||order>=PR_MAX_PHASES)bad();
        text_hex(&e->pool[ix].msg,actual);ok&=level==l&&order==k&&!strcmp(expected,actual);
    }
    while(seen<count){line_read("Q ");seen++;ok=false;}
    return ok;
}
int main(int argc,char **argv)
{
    if(argc!=2||!getenv("TURNTEST_LOCALES")||!(input=fopen(argv[1],"r")))return 2;
    pr_closures_init();double h[3];read_vector("HIGHEST_STAT ",h,3);if(h[0]!=1)bad();
    int n=integer(h[1],8192,100000),failures=0,wins[5]={0},kinds[2]={0};integer(h[2],0,INT32_MAX);
    const int types[]={BATTLER_TAG_TYPE_PROTOSYNTHESIS,BATTLER_TAG_TYPE_QUARK_DRIVE};
    for(int i=0;i<n;i++){
        static pr_battle bt;static pr_engine e;static pr_pokemon actors[4];
        memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;
        bt.game_mode=GAME_MODES_ENDLESS;bt.wave_index=201;bt.turn=1;bt.ignoring_effect_source=-1;e.n_levels=1;e.current=-1;
        double c[4];read_vector("C ",c,4);if(c[0]!=i)bad();int owner=integer(c[1],0,3),mode=integer(c[2],0,1);integer(c[3],0,1);kinds[mode]++;
        char seed[512],extra;char *row=line_read("W ");if(sscanf(row,"%511s %c",seed,&extra)!=1)bad();pr_seed_from_ascii(&bt.wave_seed,seed);
        rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;bool ok=true;
        for(int k=0;k<4;k++){
            double v[WIDTH+1];read_vector("I ",v,WIDTH+1);if(v[0]!=k)bad();load(&actors[k],v+1,k);pr_reset_turn_data(&actors[k]);
            bt.field[k]=bt.party[k/2][k%2]=&actors[k];bt.n_party[k/2]=2;ok&=compare(&actors[k],v+1,i,k,'I');
        }
        for(int step=0;step<3;step++){
            double action[2];read_vector("STEP ",action,2);if(action[0]!=step)bad();
            if(step<2){integer(action[1],0,1);ok&=pr_add_tag(&bt,&e,&actors[owner],types[mode],0,MOVE_ID_NONE,actors[owner].id)==action[1];}
            else {if(action[1]!=-1)bad();pr_remove_tag(&bt,&e,&actors[owner],types[mode]);}
            for(int k=0;k<4;k++){
                double tag[6];read_vector("TAG ",tag,6);if(tag[0]!=k)bad();int present=integer(tag[1],0,1);integer(tag[5],0,PR_MAX_BATTLER_TAGS);
                const pr_battler_tag *actual=pr_get_battler_tag(&actors[k],types[mode]);
                ok&=present==!!actual&&actors[k].n_tags==tag[5];
                if(actual)ok&=actual->stat==tag[2]&&actual->multiplier==tag[3]&&actual->turn_count==tag[4];
                else if(tag[2]||tag[3]||tag[4])bad();
                if(step==0&&k==owner){int stat=integer(tag[2],STAT_ATK,STAT_SPD);wins[stat-1]++;}
            }
            for(int k=0;k<4;k++){double v[WIDTH+1];read_vector("O ",v,WIDTH+1);if(v[0]!=k)bad();ok&=compare(&actors[k],v+1,i,k,'O');}
            double stats[5];read_vector("EF ",stats,5);const pr_stat_params sp={.move=-1,.simulated=true};
            for(int stat=STAT_ATK;stat<=STAT_SPD;stat++)ok&=stats[stat-1]==pr_get_effective_stat_p(&bt,&actors[owner],stat,&sp);
            ok&=queues(&e);
        }
        pr_rng want;rng_read("RO ",&want);ok&=rng_equal(want,bt.rng);rng_read("BO ",&want);ok&=rng_equal(want,bt.battle_rng);
        if(strcmp(line_read("E"),"\n"))bad();if(bt.n_unported||bt.n_deferred||e.overflow)ok=false;
        if(!ok){if(failures<20)fprintf(stderr,"highest case%d owner%d mode%d failed\n",i,owner,mode);failures++;}
    }
    double footer[1];read_vector("HIGHEST_STAT_COMPLETE ",footer,1);if(footer[0]!=n||fgetc(input)!=EOF)bad();
    if(!failures){for(int s=0;s<5;s++)if(wins[s]<100)bad();for(int k=0;k<2;k++)if(kinds[k]<100)bad();}
    printf("Wide highest stat: %d complete, %d exact, %d failures\n",n,n-failures,failures);return failures?1:0;
}
