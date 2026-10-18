#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
static uint32_t uid(double v){if(v<0||v>UINT32_MAX||floor(v)!=v)bad();return (uint32_t)v;}
static int64_t source_id(double v){return v==-1?-1:(int64_t)uid(v);}
static void text_read(char *out,size_t cap){char *s=line_read("MSG ");s[strcspn(s,"\n")]=0;if(!strcmp(s,"-")){out[0]=0;return;}size_t n=strlen(s);if(n%2||strspn(s,"0123456789abcdef")!=n||n/2>=cap)bad();for(size_t i=0;i<n/2;i++){unsigned b;if(sscanf(s+2*i,"%2x",&b)!=1||!b)bad();out[i]=b;}out[n/2]=0;}
int main(int argc,char **argv){
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input||!getenv("TURNTEST_LOCALES"))return 2;pr_closures_init();double h[2];read_vector("HELPINGHAND ",h,2);if(h[0]!=1)bad();int n=integer(h[1],512,100000),fail=0,steps=0;
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;bt.double_battle=true;bt.wave_index=bt.turn=1;bt.ignoring_effect_source=-1;e.current=-1;e.n_levels=1;pr_pokemon mons[5]={0};double c[7];read_vector("C ",c,7);if(c[0]!=i)bad();int target=integer(c[1],0,4),turn=integer(c[4],-2,3),move=integer(c[5],0,PR_MOVE_COUNT-1),count=integer(c[6],1,8);int64_t source=source_id(c[2]),other=source_id(c[3]);
  char *seed=line_read("W ");seed[strcspn(seed,"\n")]=0;if(!*seed||strlen(seed)>64||strchr(seed,' '))bad();pr_seed_from_ascii(&bt.wave_seed,seed);
  for(int k=0;k<5;k++){
   double v[21];read_vector("MON ",v,21);pr_pokemon *p=&mons[k];p->id=uid(v[0]);p->species=integer(v[1],1,65535);p->form=integer(v[2],0,255);pr_pokemon probe={0};probe.species=p->species;const pr_species *sp=pr_pokemon_species(&probe);if(!sp||p->form>=sp->nforms)bad();p->level=integer(v[3],1,255);p->nature=integer(v[4],0,24);p->hp=integer(v[5],0,100000);p->ability=p->base_ability=p->custom_ability=integer(v[6],0,PR_ABILITY_COUNT-1);p->ability_override=-1;p->custom_passive=0;p->is_player=integer(v[7],0,1);p->on_field=integer(v[8],0,1);if(p->is_player!=(k<3))bad();p->added_type=-1;for(int j=0;j<6;j++){p->ivs[j]=integer(v[9+j],0,31);p->stats[j]=integer(v[15+j],1,100000);}bt.party[k<3?0:1][k<3?k:k-3]=p;
  }bt.n_party[0]=3;bt.n_party[1]=2;bt.field[0]=&mons[0];bt.field[1]=&mons[1];bt.field[2]=&mons[3];bt.field[3]=&mons[4];rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;bool exact=true;pr_pokemon *p=&mons[target];
  for(int opi=0;opi<count;opi++){
   double o[3];read_vector("OP ",o,3);int op=integer(o[0],0,8),result=integer(o[1],-1,1),nt=integer(o[2],0,1),got=-1;steps++;
   switch(op){case 0:case 1:got=pr_add_tag(&bt,&e,p,BATTLER_TAG_TYPE_HELPING_HAND,turn,move,op?other:source);break;case 2:got=pr_lapse_tag(&bt,&e,p,BATTLER_TAG_TYPE_HELPING_HAND,BATTLER_TAG_LAPSE_TYPE_CUSTOM);break;case 3:pr_lapse_tags(&bt,&e,p,BATTLER_TAG_LAPSE_TYPE_TURN_END);break;case 4:pr_lapse_tags(&bt,&e,p,BATTLER_TAG_LAPSE_TYPE_PRE_MOVE);break;case 5:pr_lapse_tags(&bt,&e,p,BATTLER_TAG_LAPSE_TYPE_FAINT);break;case 6:pr_remove_tag(&bt,&e,p,BATTLER_TAG_TYPE_HELPING_HAND);break;case 7:pr_remove_tags_by_source_id(&bt,&e,p,source);break;case 8:pr_transfer_tags_by_source_id(p,source,other);break;}
   exact&=result==got&&p->n_tags==nt&&!bt.n_unported&&!e.overflow;
   for(int j=0;j<nt;j++){double t[6];read_vector("T ",t,6);int turns=integer(t[0],1,1),smove=integer(t[1],0,PR_MOVE_COUNT-1),baton=integer(t[3],0,1),nlapse=integer(t[4],1,1),lt=integer(t[5],0,BATTLER_TAG_LAPSE_TYPE_CUSTOM);int64_t sid=source_id(t[2]);(void)nlapse;if(j>=p->n_tags){exact=false;continue;}pr_battler_tag *tag=&p->tags[j];exact&=tag->type==BATTLER_TAG_TYPE_HELPING_HAND&&tag->turn_count==turns&&tag->source_move==smove&&tag->source_id==sid&&pr_battler_tags_info[tag->type].baton==baton&&pr_battler_tags_info[tag->type].lapse_mask==(1u<<lt);}
   double lev[1];read_vector("LEVELS ",lev,1);int nl=integer(lev[0],1,PR_MAX_LEVELS);exact&=nl==e.n_levels;for(int l=0;l<nl;l++){double q[2];read_vector("L ",q,2);if(q[0]!=l)bad();int nq=integer(q[1],0,PR_LEVEL_CAP);exact&=l<e.n_levels&&nq==e.level_len[l];for(int j=0;j<nq;j++){char expected[2048],actual[2048];text_read(expected,sizeof expected);if(l>=e.n_levels||j>=e.level_len[l]){exact=false;continue;}pr_phase *ph=&e.pool[e.levels[l][j]];pr_text_source src={locale_get,NULL,"en"};pr_msg_format(&ph->msg,&src,actual,sizeof actual);exact&=ph->type==PH_MESSAGE&&!strcmp(expected,actual);}}
   double d[1];read_vector("DYNAMIC ",d,1);int nd=integer(d[0],0,PR_MAX_PHASES),actual=0;for(int k=0;k<PH_COUNT_;k++)actual+=e.n_dyn[k];exact&=nd==actual;
  }
  pr_rng rng;rng_read("RO ",&rng);exact&=rng_equal(bt.rng,rng);rng_read("BO ",&rng);exact&=rng_equal(bt.battle_rng,rng);if(strcmp(line_read("E"),"\n"))bad();if(!exact){if(fail<12)fprintf(stderr,"helping%d target%d source%lld steps%d tags%d unported%d\n",i,target,(long long)source,count,p->n_tags,bt.n_unported);fail++;}
 }
 double end[1];read_vector("HELPINGHAND_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Helping Hand: %d complete, %d API operations, %d failures\n",n,steps,fail);return fail?1:0;
}
