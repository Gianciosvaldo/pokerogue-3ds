/* Actual command/encounter owners against pinned methods and rendered text. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static char docs[4][4096],files[4][32];
static void unhex(const char *in,char *out,size_t size){
 size_t n=strlen(in);CHECK(n%2==0&&n/2<size);
 for(size_t i=0;i<n/2;i++){unsigned byte;CHECK(sscanf(in+2*i,"%2x",&byte)==1&&byte);out[i]=(char)byte;}out[n/2]=0;
}
static const char *locale(void *ctx,const char *file,int fallback,size_t *len){
 (void)ctx;if(fallback)return NULL;
 for(int i=0;i<4;i++)if(!strcmp(files[i],file)){*len=strlen(docs[i]);return docs[i];}
 return NULL;
}
int main(int argc,char **argv){
 CHECK(argc==2);FILE *f=fopen(argv[1],"r");CHECK(f);char tag[32],hex[8192];int ver,ns,nt,species,form;
 CHECK(fscanf(f,"%31s%d%d%d",tag,&ver,&ns,&nt)==4&&!strcmp(tag,"POKERUS_TRAP")&&ver==1&&ns==384&&nt==4);
 CHECK(fscanf(f,"%31s%d%d",tag,&species,&form)==3&&!strcmp(tag,"E"));
 for(int i=0;i<4;i++){CHECK(fscanf(f,"%31s%31s%8191s",tag,files[i],hex)==3&&!strcmp(tag,"L"));unhex(hex,docs[i],sizeof docs[i]);}
 int reordered_spread=0;
 for(int c=0;c<ns;c++){
  char seed[64],order[8];int wave,before,after;
  CHECK(fscanf(f,"%31s%63s%d%7s%d%d",tag,seed,&wave,order,&before,&after)==6&&!strcmp(tag,"S")&&strlen(order)==3);
  setup(1);pr_seed_from_ascii(&r.run.seed,seed);r.wave=wave-1;
  for(int i=0;i<3;i++){r.party_order[i]=order[i]-'0';r.party[i].pokerus=(before&(1<<i))!=0;}
  gc_new_battle(&r);int mask=0;for(int i=0;i<3;i++)mask|=(int)r.party[i].pokerus<<i;
  CHECK(r.wave==wave&&mask==after);
  if(strcmp(order,"012")&&mask!=before)reordered_spread++;
  /* New-encounter save point must retain every carrier. */
  const char *why=NULL;size_t size=gc_run_save(&r,save,sizeof save);
  CHECK(size&&gc_run_load(&back,save,size,&why));
  for(int i=0;i<3;i++)CHECK(back.party[i].pokerus==r.party[i].pokerus);
 }
 CHECK(reordered_spread>0);
 for(int c=0;c<nt;c++){
  int ability,ghost,expected;CHECK(fscanf(f,"%31s%d%d%d%8191s",tag,&ability,&ghost,&expected,hex)==5&&!strcmp(tag,"T"));
  setup(1);pr_pokemon *p=r.bt.field[0],*opp=r.bt.field[2];
  for(int k=0;k<4;k++){r.bt.field[k]->ability=ABILITY_ID_STENCH;r.bt.field[k]->passive=0;r.bt.field[k]->custom_ability=ABILITY_ID_NONE;}
  opp->species=species;opp->form=form;opp->ability=ability;
  int8_t type=ghost?POKEMON_TYPE_GHOST:POKEMON_TYPE_GRASS;pr_set_summon_types(&r.bt,p,&type,1);
  pr_rng before=r.bt.rng,battle=r.bt.battle_rng;const char *why=NULL;
  bool ok=gc_run_choose(&r,&(gc_choice){GC_CH_POKEMON,2,0,0},&why);CHECK(ok==expected);
  const pr_msg *message=gc_run_refusal_message(&r);
  if(!ok){
   CHECK(message&&!strcmp(message->key,"abilityTriggers:arenaTrap")&&message->n_args==2);
   char expected_text[4096],actual[4096];unhex(hex,expected_text,sizeof expected_text);
   pr_text_source src={locale,NULL,NULL};pr_msg_format(message,&src,actual,sizeof actual);
   if(strcmp(expected_text,actual))fprintf(stderr,"expected <%s> actual <%s>\n",expected_text,actual);
   CHECK(!strcmp(expected_text,actual));CHECK(rngsame(before,r.bt.rng)&&rngsame(battle,r.bt.battle_rng));
   CHECK(!gc_run_choose(&r,&(gc_choice){GC_CH_POKEMON,-1,0,0},&why)&&!gc_run_refusal_message(&r));
  }else CHECK(!message&&!strcmp(hex,"-"));
 }
 int a,b;CHECK(fscanf(f,"%31s%d%d",tag,&a,&b)==3&&!strcmp(tag,"COMPLETE")&&a==384&&b==4&&fscanf(f,"%31s",tag)==EOF);fclose(f);
 printf("Pokerus/trap owners: 384 reordered spread/save cases, %d changed reordered cases; four native switch/text cases; %d checks passed\n",reordered_spread,checks);return 0;
}
