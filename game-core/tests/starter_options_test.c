/* Pinned native starter pools/prices/grades, plus purchase and save guards. */
#include "../src/internal.h"
#include "../../game/include/luck_meter.h"
#include <stdio.h>
#include <stdlib.h>
static gc_system sys,loaded;
static unsigned char profile[1024*1024];
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"starter options line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void purchases(void) {
 for(int i=0;i<gc_starter_cost_count;i++)for(int hidden=0;hidden<2;hidden++) {
  int id=gc_starter_costs[i].id,cost=gc_system_starter_unlock_cost(id);
  CHECK(cost>0);gc_system_init_defaults(&sys);
  gc_mon mon={0};mon.b.species=id;mon.b.gender=0;mon.b.nature=0;
  gc_system_set_pokemon_caught(&sys,&mon,true,false);
  gc_starter_entry *s=&sys.starters[id];s->ability_attr&=~4u;s->passive_attr=0;s->candy_count=cost-1;
  gc_starter_entry before=*s;
  CHECK(!gc_system_buy_starter_unlock(&sys,id,hidden)&&!memcmp(s,&before,sizeof before));
  s->candy_count=cost;before=*s;
  const pr_species *sp=gc_species(id);const pr_form *f=&pr_forms[sp->form0];
  bool available=hidden?f->ability_hidden!=0:f->passive!=0;
  CHECK(gc_system_buy_starter_unlock(&sys,id,hidden)==available);
  if(available) {
   CHECK(s->candy_count==0);
   if(hidden)CHECK((s->ability_attr&4)&&s->ability_preference==2&&(s->preference_flags&4));
   else CHECK(s->passive_attr==3);
   s->candy_count=cost;before=*s;
   CHECK(!gc_system_buy_starter_unlock(&sys,id,hidden)&&!memcmp(s,&before,sizeof before));
  } else CHECK(!memcmp(s,&before,sizeof before));
 }
 gc_system_init_defaults(&sys);CHECK(!gc_system_buy_starter_unlock(&sys,0,false));
 CHECK(!gc_system_buy_starter_unlock(NULL,1,false));
 sys.starters[1].candy_count=100;
 CHECK(gc_system_buy_starter_unlock(&sys,1,false));CHECK(gc_system_buy_starter_unlock(&sys,1,true));
 sys.starters[1].preference_flags|=3;sys.starters[1].nature_preference=3;sys.starters[1].appearance_preference=2;
 sys.starters[1].moveset[0]=MOVE_ID_TACKLE;
 const char *why=NULL;size_t n=gc_system_save(&sys,profile,sizeof profile);
 CHECK(n&&gc_system_load(&loaded,profile,n,&why));
 CHECK(!memcmp(&sys.starters[1],&loaded.starters[1],sizeof(gc_starter_entry)));
 sys.starters[1].nature_preference=25;n=gc_system_save(&sys,profile,sizeof profile);
 CHECK(n&&!gc_system_load(&loaded,profile,n,&why));
 uint16_t moves[64];CHECK(gc_starter_moves(&sys,0,0,moves,64)==-1);
 CHECK(gc_starter_moves(&sys,1,-1,moves,64)==-1);
 CHECK(gc_starter_moves(&sys,1,0,moves,0)==-1);
}
static void native(const char* path) {
 FILE *f=fopen(path,"r");CHECK(f);char word[40];int version,expected;
 CHECK(fscanf(f,"%39s%d%d",word,&version,&expected)==3&&!strcmp(word,"STARTER_OPTIONS")&&version==1&&expected==gc_starter_cost_count);
 int species=0,forms=0,next_form=0,next_mask=0,seen=0,pools=0,grades=0;bool complete=false;
 while(fscanf(f,"%39s",word)==1) {
  if(!strcmp(word,"S")) {
   CHECK(!species || (next_form==forms&&next_mask==0));int id,cost,price,count;
   CHECK(fscanf(f,"%d%d%d%d",&id,&cost,&price,&count)==4);CHECK(id>species&&id<=GC_MAX_SPECIES_ID);
   const pr_species *sp=gc_species(id);CHECK(sp&&count==sp->nforms);
   CHECK(gc_starter_cost(id)==cost&&gc_system_starter_unlock_cost(id)==price);
   species=id;forms=count;next_form=next_mask=0;seen++;
  } else if(!strcmp(word,"P")) {
   int id,form,mask,n;CHECK(fscanf(f,"%d%d%d%d",&id,&form,&mask,&n)==4);
   CHECK(id==species&&form==next_form&&mask==next_mask&&n>=0&&n<=64);
   uint16_t actual[64];sys.starters[id].egg_moves=mask;
   CHECK(gc_starter_moves(&sys,id,form,actual,64)==n);
   for(int k=0;k<n;k++){int move;CHECK(fscanf(f,"%d",&move)==1);if(actual[k]!=move)fprintf(stderr,"pool mismatch species%d form%d mask%d slot%d C%d native%d\n",id,form,mask,k,actual[k],move);CHECK(actual[k]==move);}
   pools++;if(++next_mask==16){next_mask=0;next_form++;}
  } else if(!strcmp(word,"L")) {
   int luck;char grade[8];CHECK(fscanf(f,"%d%7s",&luck,grade)==2&&luck==grades);
   CHECK(!strcmp(luck_grade(luck),grade));grades++;
  } else if(!strcmp(word,"COMPLETE")) {
   int n,p;CHECK(fscanf(f,"%d%d",&n,&p)==2&&n==seen&&p==pools);
   CHECK(seen==572&&pools==12672&&grades==15&&next_form==forms&&next_mask==0);
   complete=true;break;
  } else CHECK(false);
 }
 CHECK(complete&&fscanf(f,"%39s",word)==EOF);fclose(f);
 printf("Native starter options: %d species, %d form/egg pools, %d luck grades exact\n",seen,pools,grades);
}
int main(int argc,char**argv){purchases();if(argc==2)native(argv[1]);else CHECK(argc==1);printf("Starter options: %d checks passed\n",checks);return 0;}
