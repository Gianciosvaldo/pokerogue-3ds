/* Native snapshot replay: acquisition, primary/secondary evolution queries and persistence. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run r,back;static unsigned char saved[GC_SAVE_MAX];static unsigned checks;
static int line;static FILE *f;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"evo tracker line%d fixture%d: %s\n",__LINE__,line,#x);exit(1);}}while(0)
static char buf[512];
static char *record(char c){CHECK(fgets(buf,sizeof buf,f));line++;CHECK(strchr(buf,'\n')&&buf[0]==c&&buf[1]==' ');return buf+2;}
static void setup(int rotation){
 gc_run_new(&r,GAME_MODES_CLASSIC,"native-tracker",777);gc_starter st[3]={0};const char *why;
 for(int j=0;j<3;j++){st[j].species=(int[]){SPECIES_ID_BULBASAUR,SPECIES_ID_CHARMANDER,SPECIES_ID_SQUIRTLE}[j];st[j].nature=0;st[j].tera_type=-1;memset(st[j].ivs,15,6);}
 CHECK(gc_run_set_starters(&r,st,3,&why));r.n_items=r.n_enemy_items=0;
 for(int j=0;j<3;j++)r.party_order[j]=(j+rotation)%3;
 gc_event ev;while(gc_run_next_event(&r,&ev)){}r.bt.n_unported=0;
}
static void inventory(char tag,bool compare){
 int n,end;char *b=record(tag);CHECK(sscanf(b,"%d %n",&n,&end)==1&&b[end]==0&&n>=0&&n<=24);
 if(compare)CHECK(r.n_items==n);else r.n_items=n;
 for(int j=0;j<n;j++){
  char key[64];int owner,stack,virt,locked; b=record('I');
  CHECK(sscanf(b,"%63s %d %d %d %d %n",key,&owner,&stack,&virt,&locked,&end)==5&&b[end]==0);
  int item=gc_reward_item_index(key);CHECK(item>=0&&owner>=-1&&owner<3&&stack>=1&&stack<=999&&virt==0&&(locked==0||locked==1));
  uint32_t id=owner<0?0:r.party[r.party_order[owner]].b.id;
  if(compare){gc_item *it=&r.items[j];CHECK(!strcmp(it->key,key)&&it->pokemon_id==id&&it->stack==stack&&it->virtual_stack==virt);
   if(!strcmp(key,"EVOLUTION_TRACKER_GIMMIGHOUL"))CHECK(it->non_transferable);
  }else r.items[j]=(gc_item){.key=pr_item_keys[item],.pokemon_id=id,.stack=stack,.virtual_stack=virt,.arg=0,.non_transferable=!strcmp(key,"EVOLUTION_TRACKER_GIMMIGHOUL")};
 }
}
static void roundtrip(void){const char *why;size_t n=gc_run_save(&r,saved,sizeof saved);CHECK(n&&gc_run_load(&back,saved,n,&why)&&!back.load_dropped);CHECK(back.money==r.money&&back.n_items==r.n_items);
 for(int j=0;j<r.n_items;j++)CHECK(!strcmp(back.items[j].key,r.items[j].key)&&back.items[j].stack==r.items[j].stack&&back.items[j].pokemon_id==r.items[j].pokemon_id&&back.items[j].non_transferable==r.items[j].non_transferable);
}
static int target(gc_run *run,gc_mon *m){bool fusion;int evo=gc_mon_get_evolution_target(run,m,&fusion);return evo<0?0:gc_evo_data[evo].species;}
int main(int argc,char **argv){CHECK(argc==2);f=fopen(argv[1],"r");CHECK(f);CHECK(fgets(buf,sizeof buf,f)&&!strcmp(buf,"EVOTRACKER 1 576 12\n"));line++;
 bool seen[3][6][8][4]={0};int source_fused_evolutions=0,primary_observations=0,fused_observations=0;
 for(int i=0;i<576;i++){
  int id,family,arrangement,count,treasure,wave,balls,end;double before,money;char *b=record('C');
  CHECK(sscanf(b,"%d %d %d %d %d %d %lf %lf %d %n",&id,&family,&arrangement,&count,&treasure,&wave,&before,&money,&balls,&end)==9&&b[end]==0);
  CHECK(id==i&&family>=0&&family<3&&arrangement>=0&&arrangement<6&&treasure>=0&&treasure<4);
  int counts[]={0,1,7,8,9,997,998,999},ci=-1;for(int q=0;q<8;q++)if(counts[q]==count)ci=q;
  CHECK(ci>=0&&!seen[family][arrangement][ci][treasure]);seen[family][arrangement][ci][treasure]=true;
  setup(i%3);r.wave=wave;r.money=before;r.pokeballs[0]=0;int expected[3][2],treasures[3];
  for(int j=0;j<3;j++){
   int species,fusion,form; b=record('M');CHECK(sscanf(b,"%d %d %d %d %d %d %n",&species,&fusion,&form,&treasures[j],&expected[j][0],&expected[j][1],&end)==6&&b[end]==0);
   CHECK((species==SPECIES_ID_GIMMIGHOUL||species==SPECIES_ID_MEW)&&(fusion==-1||fusion==SPECIES_ID_GIMMIGHOUL)&&form>=0&&form<2);
   gc_mon *m=&r.party[r.party_order[j]];m->b.species=species;m->b.fusion_species=fusion<0?0:fusion;m->b.form=form;m->b.level=5;m->ability_index=0;m->b.fusion_ability_index=0;m->b.fusion_form=treasure%2;
  }
  inventory('B',false);gc_items_to_battle(&r);for(int j=0;j<3;j++)gc_mon_recalc(&r,&r.party[j]);
  pr_reward_ctx *ctx=gc_reward_ctx(&r);for(int j=0;j<3;j++)CHECK(ctx->party[j].persistent_treasure==treasures[j]);
  for(int j=0;j<3;j++){gc_mon *m=&r.party[r.party_order[j]];CHECK(target(&r,m)==expected[j][0]);if(m->b.species==SPECIES_ID_GIMMIGHOUL||!m->b.fusion_species)primary_observations++;else fused_observations++;}
  pr_rng rng=r.run.rng;const char *why=NULL;const char *keys[]={"NUGGET","BIG_NUGGET","RELIC_GOLD"};gc_offer o={.key=keys[family],.target=GC_TARGET_NONE};
  CHECK(gc_reward_apply(&r,&o,-1,-1,&why)==GC_APPLY_DONE);CHECK(r.money==money&&r.pokeballs[0]==balls&&!memcmp(&rng,&r.run.rng,sizeof rng));inventory('A',true);
  CHECK(!r.stubs[GC_STUB_REWARD_APPLY]&&!r.stubs[GC_STUB_EVOLUTION]);
  for(int j=0;j<3;j++){gc_mon *m=&r.party[r.party_order[j]];CHECK(target(&r,m)==expected[j][1]);if(m->b.species==SPECIES_ID_GIMMIGHOUL||!m->b.fusion_species)primary_observations++;else {fused_observations++;source_fused_evolutions+=expected[j][1]!=0;}}
  gc_items_to_battle(&r);
  for(int j=0;j<r.bt.n_mods[0];j++)if(r.bt.mods[0][j].cls==PR_MOD_EVO_TRACKER){CHECK(pr_mod_is_ported_item(&r.bt.mods[0][j])&&!r.bt.mods[0][j].is_transferable);}
  roundtrip();
  for(int j=0;j<3;j++){gc_mon *m=&r.party[r.party_order[j]],*m2=&back.party[back.party_order[j]];CHECK(target(&r,m)==target(&back,m2));}
 }
 for(int i=0;i<12;i++){
  int id,form,copies,fused,species,fusion,end;char *b=record('D');CHECK(sscanf(b,"%d %d %d %d %d %d %n",&id,&form,&copies,&fused,&species,&fusion,&end)==6&&b[end]==0&&id==i);
  CHECK(form>=0&&form<2&&copies>=1&&copies<=3&&(fused==0||fused==1)&&species==SPECIES_ID_GHOLDENGO);
  setup(i%3);gc_mon *m=&r.party[r.party_order[0]];m->b.species=SPECIES_ID_GIMMIGHOUL;m->b.form=form;m->b.level=5;m->b.fusion_species=fused?SPECIES_ID_MEW:0;
  CHECK(fusion==(fused?SPECIES_ID_MEW:-1));inventory('B',false);gc_items_to_battle(&r);int evo=gc_mon_get_evolution(&r,m);CHECK(evo>=0);gc_mon_evolve(&r,m,evo);CHECK(m->b.species==species&&m->b.fusion_species==(fusion<0?0:fusion));inventory('A',true);roundtrip();
 }
 CHECK(fgets(buf,sizeof buf,f)&&!strcmp(buf,"END 576 12\n")&&fgetc(f)==EOF);fclose(f);
 printf("EvoTracker: 576 exact acquisition snapshots, 12 native evolution cleanups, %d primary evolution observations, %u checks. %d secondary observations (%d ready after reward).\n",primary_observations,checks,fused_observations,source_fused_evolutions);return 0;
}
