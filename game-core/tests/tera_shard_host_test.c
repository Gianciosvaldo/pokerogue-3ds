#include "../src/internal.h"
#include "gc/reward.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run;
static int count,checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Tera shard case%d line%d: %s\n",count,__LINE__,#x);exit(1);}}while(0)
int main(int argc,char**argv){if(argc!=2)return 2;FILE*f=fopen(argv[1],"r");CHECK(f);char line[256];CHECK(fgets(line,sizeof line,f)&&!strcmp(line,"TERA_SHARDS 1 1520\n"));
 const int primary[]={SPECIES_ID_MEW,SPECIES_ID_SHEDINJA,SPECIES_ID_OGERPON,SPECIES_ID_TERAPAGOS},fusion[]={0,SPECIES_ID_MEW,SPECIES_ID_SHEDINJA,SPECIES_ID_OGERPON,SPECIES_ID_TERAPAGOS};
 for(int p=0;p<4;p++)for(int q=0;q<5;q++)for(int faint=0;faint<2;faint++)for(int type=0;type<=18;type++)for(int same=0;same<2;same++){
  int v[14];char extra;CHECK(fgets(line,sizeof line,f));CHECK(sscanf(line,"C %d %d %d %d %d %d %d %d %d %d %d %d %d %c",&v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7],&v[8],&v[9],&v[10],&v[11],&v[12],&extra)==13);
  CHECK(v[0]==count&&v[1]==primary[p]&&v[2]==fusion[q]&&v[3]==faint&&v[4]==type&&v[5]==same&&v[6]==(same?type:(type+1)%19));
  for(int i=7;i<=9;i++)CHECK(v[i]==0||v[i]==1);CHECK(v[10]>=0&&v[10]<=18&&v[11]==2&&v[12]==0);
  gc_run_new(&run,GAME_MODES_CLASSIC,"tera-shards",1);run.n_party=2;run.party_order[0]=1;run.party_order[1]=0;
  gc_mon*m=&run.party[1];m->b.species=primary[p];m->b.level=100;m->b.fusion_species=fusion[q];m->b.tera_type=v[6];m->b.hp=faint?0:10;m->b.stats[0]=10;m->b.id=2;run.party[0].b.species=SPECIES_ID_MEW;run.party[0].b.level=100;run.party[0].b.hp=run.party[0].b.stats[0]=10;run.party[0].b.id=1;run.party[0].b.tera_type=2;
  CHECK(gc_mon_can_use_tera_shard(m)==v[7]);CHECK(v[7]==v[8]);pr_rng before=run.run.rng;gc_offer offer={.key="TERA_SHARD",.arg=type,.target=GC_TARGET_POKEMON};const char*why=NULL;
  CHECK(gc_port_apply_reward(&run,&offer,1,-1,&why)==v[9]);CHECK(m->b.tera_type==v[10]);CHECK(run.party[0].b.tera_type==v[11]);CHECK(!run.n_items&&!run.n_enemy_items);CHECK(!run.stubs[GC_STUB_REWARD_APPLY]);CHECK(!memcmp(&run.run.rng,&before,sizeof before));CHECK(!m->b.terastallized);count++;
 }
 CHECK(fgets(line,sizeof line,f)&&!strcmp(line,"TERA_SHARDS_COMPLETE 1520\n")&&fgetc(f)==EOF);fclose(f);printf("Tera shard consumable: %d native cases, %d checks passed\n",count,checks);return 0;
}
