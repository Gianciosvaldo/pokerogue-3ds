#include "../src/internal.h"
#include "gc/gacha.h"
#include <stdio.h>
#include <stdlib.h>
static gc_system sys,expected,loaded;
static unsigned char save[1024*1024];static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"species egg line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(int argc,char **argv){
 CHECK(argc==2);FILE *f=fopen(argv[1],"r");CHECK(f);char tag[24];int v,n;
 CHECK(fscanf(f,"%23s%d%d",tag,&v,&n)==3&&!strcmp(tag,"SPECIES_EGGS")&&v==1&&n==572&&n==gc_starter_cost_count);
 const int counts[]={0,7,8,9,10,14,15,16,19,20,29,30,31,32,39,40,59,60,79,80,100};
 int rows=0,shiny=0,variants=0,rare=0;
 for(int i=0;i<n;i++){
  int id=gc_starter_costs[i].id,spmeta,tiermeta,varmeta;
  CHECK(fscanf(f,"%23s%d%d%d",tag,&spmeta,&tiermeta,&varmeta)==4&&!strcmp(tag,"S")&&spmeta==id&&tiermeta>=0&&tiermeta<4&&varmeta>=0&&varmeta<2);
  for(unsigned k=0;k<sizeof counts/sizeof *counts;k++){
   int sp,h,c;CHECK(fscanf(f,"%23s%d%d%d",tag,&sp,&h,&c)==4&&!strcmp(tag,"P")&&sp==id&&h==counts[k]);
   sys.dex[id].hatched_count=h;CHECK(gc_system_species_egg_cost(&sys,id)==c);
  }
  for(int k=0;k<4;k++){
   int sp;double x[19];CHECK(fscanf(f,"%23s%d",tag,&sp)==2&&!strcmp(tag,"E")&&sp==id);
   for(int j=0;j<19;j++)CHECK(fscanf(f,"%lf",&x[j])==1&&isfinite(x[j]));
   gc_system_init_defaults(&sys);sys.dex[id].caught_attr=157;sys.starters[id].candy_count=9999;
   sys.n_eggs=1;sys.eggs[0]=(gc_egg){.id=17,.species=1,.hatch_waves=3};expected=sys;
   gc_gacha_ctx ctx={0};ctx.entropy=(pr_rng){x[0],x[1],x[2],x[3],0};ctx.timestamp_ms=x[13];
   gc_egg e={0};e.id=x[4];e.hatch_waves=x[5];e.tier=x[6];e.species=x[7];e.source_type=x[8];e.variant_tier=x[9];
   e.is_shiny=x[10];e.egg_move_index=x[11];e.override_hidden_ability=x[12];e.timestamp_ms=x[13];
   expected.starters[id].candy_count-=gc_system_species_egg_cost(&sys,id);expected.eggs[expected.n_eggs++]=e;
   const char *why=NULL;
   if(!gc_system_buy_species_egg(&sys,id,&ctx,&why)){fprintf(stderr,"species%d: %s\n",id,why);CHECK(false);}
   if(memcmp(&sys,&expected,sizeof sys)){fprintf(stderr,"species%d sample%d egg actual %u/%d/%d/%d/%d/%d/%d expected %u/%d/%d/%d/%d/%d/%d\n",id,k,sys.eggs[1].id,sys.eggs[1].hatch_waves,sys.eggs[1].tier,sys.eggs[1].is_shiny,sys.eggs[1].variant_tier,sys.eggs[1].egg_move_index,sys.eggs[1].species,e.id,e.hatch_waves,e.tier,e.is_shiny,e.variant_tier,e.egg_move_index,e.species);CHECK(false);}
   CHECK(ctx.entropy.c==x[14]&&ctx.entropy.s0==x[15]&&ctx.entropy.s1==x[16]&&ctx.entropy.s2==x[17]&&ctx.entropy_draws==x[18]);
   CHECK(!ctx.scene_rng.c&&!ctx.scene_rng.s0&&!ctx.scene_rng.s1&&!ctx.scene_rng.s2);
   size_t bytes=gc_system_save(&sys,save,sizeof save);CHECK(bytes&&gc_system_load(&loaded,save,bytes,&why));
   CHECK(!memcmp(&sys,&loaded,sizeof sys));
   /* Refusals leave every profile byte and RNG state untouched. */
   for(int reason=0;reason<3;reason++){
    sys=expected;if(reason==0)sys.n_eggs=99;else if(reason==1)sys.starters[id].candy_count=0;else sys.dex[id].caught_attr=0;
    loaded=sys;gc_gacha_ctx old=ctx;CHECK(!gc_system_buy_species_egg(&sys,id,&ctx,&why));
    CHECK(!memcmp(&sys,&loaded,sizeof sys)&&!memcmp(&ctx,&old,sizeof ctx));
   }
   rows++;shiny+=e.is_shiny;variants+=e.variant_tier>0;rare+=e.egg_move_index==3;
  }
 }
 int sn,rn,ss,vn,en;CHECK(fscanf(f,"%23s%d%d%d%d%d",tag,&sn,&rn,&ss,&vn,&en)==6&&!strcmp(tag,"COMPLETE"));
 CHECK(sn==572&&rn==rows&&rows==2288&&ss==shiny&&vn==variants&&en==rare&&shiny>0&&variants>0&&rare>0);
 CHECK(fscanf(f,"%23s",tag)==EOF);fclose(f);
 printf("Species egg native: %d species, %d constructors, %d shiny, %d variants, %d rare moves; %u checks passed\n",sn,rows,shiny,variants,rare,checks);
 return 0;
}
