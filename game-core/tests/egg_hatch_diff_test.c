#define main gacha_replay_main
#include "egg_gacha_diff_test.c"
#undef main
#include "../src/internal.h"
static gc_run run;
int main(int argc,char **argv){
 if(argc!=2 || !(fp=fopen(argv[1],"r")))return 2;
 if(!fgets(line,sizeof line,fp)||strcmp(line,"EGGHATCH 2 2284\n"))return 2;
 int fail=0,seen[571]={0},modes[571]={0};
 if(gc_egg_pool_count!=571)bad("pool size");
 run.run.mode=GAME_MODES_CLASSIC;run.wave=1;run.biome=BIOME_ID_TOWN;run.trainer_type=-1;
 for(int i=0;i<2284;i++){
  double v[23];vec('C',v,23);
  if(v[0]!=i)bad("case order");
  gc_egg egg={.id=(uint32_t)v[1],.species=integer(v[2],1,GC_MAX_SPECIES_ID),.source_type=integer(v[3],0,4),
   .is_shiny=integer(v[4],0,1),.variant_tier=integer(v[5],0,2),.egg_move_index=integer(v[6],0,3),.override_hidden_ability=integer(v[7],0,1)};
  if(v[1]<0||v[1]>UINT32_MAX||v[1]!=floor(v[1])||v[8]<0||v[8]>UINT32_MAX||v[8]!=floor(v[8]))bad("id");
  int pool=-1;
  for(int k=0;k<gc_egg_pool_count;k++)if(gc_egg_pool[k].species==egg.species)pool=k;
  if(pool<0||egg.source_type!=i%4||++seen[pool]>4||(modes[pool]&(1<<egg.source_type)))bad("pool manifest");
  modes[pool]|=1<<egg.source_type;
  gc_mon mon;
  pr_rng prior=run.run.rng,battle=run.bt.battle_rng;
  if(!gc_egg_generate(&run,&egg,&mon))bad("generation");
  bool exact=mon.b.id==(uint32_t)v[8]&&mon.b.species==integer(v[9],1,GC_MAX_SPECIES_ID)
    &&mon.b.form==integer(v[10],0,255)&&mon.b.gender==integer(v[11],-1,1)
    &&mon.ability_index==integer(v[12],0,2)&&mon.b.nature==integer(v[13],0,24)
    &&mon.b.shiny==integer(v[14],0,1)&&mon.variant==integer(v[15],0,2)&&mon.b.tera_type==integer(v[16],0,17)
    &&rng_eq(&prior,&run.run.rng)&&rng_eq(&battle,&run.bt.battle_rng);
  for(int s=0;s<6;s++)exact&=mon.b.ivs[s]==integer(v[17+s],0,31);
  if(!exact){if(fail<12)fprintf(stderr,"egg%d species%d: id%u/%u form%d/%.0f ability%d/%.0f nature%d/%.0f IV%d/%.0f\n",i,egg.species,mon.b.id,(uint32_t)v[8],mon.b.form,v[10],mon.ability_index,v[12],mon.b.nature,v[13],mon.b.ivs[0],v[17]);fail++;}
 }
 for(int k=0;k<571;k++)if(seen[k]!=4||modes[k]!=15)bad("missing species");
 if(!fgets(line,sizeof line,fp)||strcmp(line,"EGGHATCH_COMPLETE 2284\n")||fgetc(fp)!=EOF)bad("footer");
 printf("Egg hatch generation: 2284 compared, %d failures\n",fail);return fail?1:0;
}
