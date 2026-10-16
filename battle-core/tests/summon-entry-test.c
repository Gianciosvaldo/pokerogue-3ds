#define main gas_replay_main
#include "gas-lifecycle-test.c"
#undef main
static int phase_type(const char *name){for(int j=0;j<PH_COUNT_;j++)if(pr_phase_name[j]&&!strcmp(name,pr_phase_name[j]))return j;bad();return -1;}
static bool compare_phase(const pr_phase *p,const pr_battle *bt){
 char name[80],marker[80],extra;int b,field,party,player,dbl,use;
 if(sscanf(line_read("Q "),"%79s %d %d %d %d %d %d %79s %c",name,&b,&field,&party,&player,&dbl,&use,marker,&extra)!=8)bad();
 phase_type(name);if(b<-1||b>3||field<-1||field>1||party<-1||party>1||player<-1||player>1||dbl<-1||dbl>1||use<-1||use>1)bad();
 if(strcmp(marker,"-"))phase_type(marker);if(!p)return false;
 bool member=p->type==PH_SUMMON||p->type==PH_RETURN,pokemon=p->type==PH_SHINY_SPARKLE;
 int gotb=member?-1:p->battler;
 int gotfield=member?(p->sw_field_index<(bt->double_battle?2:1)?p->sw_field_index:-1):p->type==PH_CHECK_SWITCH?p->sw_field_index:pokemon?p->battler%2:-1;
 return !strcmp(name,pr_phase_name[p->type])&&b==gotb&&field==gotfield&&party==(member?p->sw_field_index:-1)
  &&player==(member?p->sw_player:pokemon?p->battler<2:-1)&&dbl==(p->type==PH_TOGGLE_DOUBLE_POSITION?p->position_double:-1)
  &&use==(p->type==PH_CHECK_SWITCH?p->sw_use_name:-1)&&!strcmp(marker,p->type==PH_MARKER?pr_phase_name[p->marker_of]:"-");
}
static uint32_t uid(double v){if(v<0||v>UINT32_MAX||floor(v)!=v)bad();return (uint32_t)v;}
static void read_actor(const char *key,pr_pokemon *p,bool check,bool *exact){
 double a[26];read_vector(key,a,26);uint32_t id=uid(a[0]);int species=integer(a[1],1,65535),form=integer(a[2],0,255),level=integer(a[3],1,255),nature=integer(a[4],0,24),hp=integer(a[5],0,100000),ability=integer(a[6],0,PR_ABILITY_COUNT-1),on=integer(a[7],0,1),sw=integer(a[8],0,1),turn=integer(a[9],0,100),wave=integer(a[10],0,100),summoned=integer(a[11],0,1),shiny=integer(a[12],0,1),hits=integer(a[13],0,10);
 pr_pokemon probe={0};probe.species=species;const pr_species *sp=pr_pokemon_species(&probe);if(!sp||form>=sp->nforms)bad();
 if(check)*exact&=p->id==id&&p->species==species&&p->form==form&&p->level==level&&p->nature==nature&&p->hp==hp&&p->ability==ability&&(p->on_field&&!p->switch_out_status)==on&&p->switch_out_status==sw&&p->temp_turn_count==turn&&p->temp_wave_turn_count==wave&&p->summoned_this_turn==summoned&&p->shiny==shiny&&p->hit_count==hits;
 else{p->id=id;p->species=species;p->form=form;p->level=level;p->nature=nature;p->hp=hp;p->ability=p->base_ability=ability;p->custom_ability=ability;p->ability_override=-1;p->custom_passive=0;p->on_field=on;p->switch_out_status=sw;p->temp_turn_count=turn;p->temp_wave_turn_count=wave;p->summoned_this_turn=summoned;p->shiny=shiny;p->hit_count=hits;p->added_type=-1;}
 for(int k=0;k<6;k++){int iv=integer(a[14+k],0,31),stat=integer(a[20+k],1,100000);if(check)*exact&=p->ivs[k]==iv&&p->stats[k]==stat;else{p->ivs[k]=iv;p->stats[k]=stat;}}
}
static void read_hex(const char *key,char *out,size_t size){char *s=line_read(key);s[strcspn(s,"\n")]=0;if(!strcmp(s,"-")){*out=0;return;}size_t n=strlen(s);if(n%2||strspn(s,"0123456789abcdef")!=n||n/2>=size)bad();for(size_t i=0;i<n/2;i++){unsigned v;if(sscanf(s+2*i,"%2x",&v)!=1||!v)bad();out[i]=v;}out[n/2]=0;}
int main(int argc,char **argv){
 if(argc!=2)return 2;input=fopen(argv[1],"r");if(!input||!getenv("TURNTEST_LOCALES"))return 2;pr_closures_init();double h[2];read_vector("SUMMONENTRY ",h,2);if(h[0]!=3)bad();int n=integer(h[1],512,100000),fail=0,doubles=0;
 for(int i=0;i<n;i++){
  static pr_battle bt;static pr_engine e;memset(&bt,0,sizeof bt);memset(&e,0,sizeof e);bt.engine=&e;e.current=-1;e.n_levels=1;bt.battle_type=BATTLE_TYPE_TRAINER;bt.wave_index=1;bt.ignoring_effect_source=-1;pr_pokemon actors[6]={0};char names[2][256];bool exact=true;
  double c[4];read_vector("C ",c,4);if(c[0]!=i)bad();bt.double_battle=integer(c[1],0,1);int side=integer(c[2],0,1),field=integer(c[3],0,1);if(field&&!bt.double_battle)bad();doubles+=bt.double_battle;
  for(int k=0;k<2;k++){read_hex("NAME ",names[k],sizeof names[k]);bt.trainer_names[k]=names[k];}
  for(int k=0;k<6;k++){read_actor("MON ",&actors[k],false,&exact);actors[k].is_player=k<3;bt.party[k/3][k%3]=&actors[k];if(k%3<(bt.double_battle?2:1))bt.field[(k/3)*2+k%3]=&actors[k];}bt.n_party[0]=bt.n_party[1]=3;
  rng_read("R ",&bt.rng);rng_read("B ",&bt.battle_rng);bt.rng_live=bt.battle_rng_live=true;int ph=pr_phase_new(&e,PH_SUMMON,side*2+field);e.current=ph;e.pool[ph].sw_field_index=field;e.pool[ph].sw_player=side==0;pr_phase_run(&bt,&e,ph);exact&=!bt.n_unported&&!e.overflow;
  for(int k=0;k<6;k++)read_actor("OUT ",&actors[k],true,&exact);
  for(int s=0;s<2;s++){double p[4];read_vector("PARTY ",p,4);if(p[0]!=s)bad();for(int k=0;k<3;k++)exact&=bt.party[s][k]->id==uid(p[1+k]);}
  double f[4];read_vector("FIELD ",f,4);for(int k=0;k<4;k++)if(f[k]==-1)exact&=bt.field[k]==NULL;else exact&=bt.field[k]&&bt.field[k]->id==uid(f[k]);
  double texts[1];read_vector("TEXTS ",texts,1);int nt=integer(texts[0],0,100),actual=0;for(int k=0;k<e.n_events;k++)actual+=e.events[k].type==PR_EV_TEXT;exact&=nt==actual;int ev=0;
  for(int k=0;k<nt;k++){char expected[2048],got[2048];read_hex("TEXT ",expected,sizeof expected);while(ev<e.n_events&&e.events[ev].type!=PR_EV_TEXT)ev++;if(ev==e.n_events){exact=false;continue;}pr_text_source src={locale_get,NULL,"en"};pr_msg_format(&e.events[ev++].msg,&src,got,sizeof got);exact&=!strcmp(expected,got);}
  double levels[1];read_vector("LEVELS ",levels,1);int nl=integer(levels[0],1,PR_MAX_LEVELS);exact&=nl==e.n_levels;
  for(int l=0;l<nl;l++){double v[2];read_vector("L ",v,2);if(v[0]!=l)bad();int count=integer(v[1],0,PR_LEVEL_CAP);exact&=l<e.n_levels&&count==e.level_len[l];for(int k=0;k<count;k++)exact&=compare_phase(l<e.n_levels&&k<e.level_len[l]?&e.pool[e.levels[l][k]]:NULL,&bt);}
  double dyn[1];read_vector("DYNTYPES ",dyn,1);int nd=integer(dyn[0],0,PH_COUNT_),actual_dyn=0;bool seen[PH_COUNT_]={false};for(int t=0;t<PH_COUNT_;t++)actual_dyn+=e.n_dyn[t]>0;exact&=nd==actual_dyn;
  for(int j=0;j<nd;j++){char name[80],extra;int count;if(sscanf(line_read("D "),"%79s %d %c",name,&count,&extra)!=2||count<1||count>PR_DYN_CAP)bad();int t=phase_type(name);if(seen[t])bad();seen[t]=true;exact&=count==e.n_dyn[t];for(int k=0;k<count;k++)exact&=compare_phase(k<e.n_dyn[t]?&e.pool[e.dyn[t][k]]:NULL,&bt);}
  pr_rng rng;rng_read("RO ",&rng);exact&=rng_equal(bt.rng,rng);rng_read("BO ",&rng);exact&=rng_equal(bt.battle_rng,rng);if(strcmp(line_read("E"),"\n"))bad();if(!exact){if(fail<12){fprintf(stderr,"summon%d double%d side%d field%d unported%d events%d/%d dyn%d/%d\n",i,bt.double_battle,side,field,bt.n_unported,actual,nt,actual_dyn,nd);for(int k=0;k<bt.n_unported;k++)fprintf(stderr,"feature %s\n",pr_unported_name(bt.unported[k]));}fail++;}
 }
 double end[1];read_vector("SUMMONENTRY_COMPLETE ",end,1);if(end[0]!=n||fgetc(input)!=EOF)bad();printf("Summon entry: %d complete (%d doubles), %d failures\n",n,doubles,fail);return fail?1:0;
}
