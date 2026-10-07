/* Mint public reward flow, exact nature metadata, profile unlock guards, and
 * legal save/resume boundaries. Upstream stat answers are in mint_diff_test. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run r,loaded;
static gc_system s,back;
static unsigned checks;
static unsigned char buf[GC_SAVE_MAX],profile[1024*1024];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"mint host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void setup(void){const char *why;gc_system_init_defaults(&s);gc_run_new(&r,GAME_MODES_CLASSIC,"mint-host",777);
 gc_starter starter={.species=SPECIES_ID_BULBASAUR,.nature=17,.tera_type=-1};memset(starter.ivs,15,6);
 CHECK(gc_run_set_starters(&r,&starter,1,&why));r.sys=&s;gc_event e;while(gc_run_next_event(&r,&e)){} }
static gc_offer mint(int nature,int cost){return(gc_offer){.key="MINT",.arg=nature,.cost=cost,.target=GC_TARGET_POKEMON};}
static void screen(gc_offer o){gc_rqueue *q=&gc_int(&r)->rq;gc_rq_reset(q);q->current=gc_rp(RP_SELECT_MODIFIER,0,0,0);q->waiting=true;
 gc_rq_push(&r,gc_rp(RP_COMMAND,0,0,0));memset(&r.screen,0,sizeof r.screen);r.screen.kind=GC_SCREEN_REWARD;
 r.screen.n_options=r.screen.n_shop=1;r.screen.options[0]=r.screen.shop[0]=o;r.money=1000; }
int main(int argc,char **argv){const char *why;setup();gc_offer o=mint(1,-1);gc_mon *m=&r.party[0];
 m->b.status=STATUS_EFFECT_TOXIC;m->b.status_toxic_turns=7;m->b.status_sleep_turns=3;m->b.status_freeze_turns=5;
 m->b.pp_up[0]=2;m->b.pp_used[0]=3;m->b.max_pp_override[0]=1;pr_pokemon before=m->b;back=s;pr_rng rng=r.run.rng;
 CHECK(gc_port_apply_reward(&r,&o,0,-1,&why));CHECK(m->has_custom_nature&&m->base_nature==17&&m->b.nature==1);
 before.nature=m->b.nature;before.hp=m->b.hp;memcpy(before.stats,m->b.stats,sizeof before.stats);
 CHECK(!memcmp(&before,&m->b,sizeof before));CHECK(!memcmp(&rng,&r.run.rng,sizeof rng));CHECK(r.n_items==0&&r.stubs[GC_STUB_REWARD_APPLY]==0);
 CHECK(s.dex[SPECIES_ID_BULBASAUR].nature_attr==(back.dex[SPECIES_ID_BULBASAUR].nature_attr|(1U<<2)));
 back.dex[SPECIES_ID_BULBASAUR].nature_attr=s.dex[SPECIES_ID_BULBASAUR].nature_attr;CHECK(!memcmp(&back,&s,sizeof s));
 o.arg=2;CHECK(gc_port_apply_reward(&r,&o,0,-1,&why));CHECK(m->base_nature==17&&m->b.nature==2);
 size_t n=gc_run_save(&r,buf,sizeof buf);CHECK(n>0);CHECK(gc_run_load(&loaded,buf,n,&why));
 CHECK(loaded.party[0].has_custom_nature&&loaded.party[0].base_nature==17&&loaded.party[0].b.nature==2);
 CHECK(!memcmp(loaded.party[0].b.stats,m->b.stats,sizeof m->b.stats));
 size_t pn=gc_system_save(&s,profile,sizeof profile);CHECK(pn>0&&gc_system_load(&back,profile,pn,&why));
 CHECK(back.dex[SPECIES_ID_BULBASAUR].nature_attr==s.dex[SPECIES_ID_BULBASAUR].nature_attr);
 m->base_nature=25;n=gc_run_save(&r,buf,sizeof buf);CHECK(n>0&&!gc_run_load(&loaded,buf,n,&why));
 setup();m=&r.party[0];m->b.species=SPECIES_ID_IVYSAUR;gc_mon_recalc(&r,m);s.dex[SPECIES_ID_BULBASAUR].caught_attr=0;
 back=s;o=mint(1,-1);CHECK(gc_port_apply_reward(&r,&o,0,-1,&why));CHECK(!memcmp(&back,&s,sizeof s));
 CHECK(gc_system_set_pokemon_caught(&s,m,true,false));CHECK(s.dex[SPECIES_ID_IVYSAUR].nature_attr==(1U<<18));
 CHECK(!(s.dex[SPECIES_ID_BULBASAUR].nature_attr&(1U<<2)));
 s.dex[SPECIES_ID_IVYSAUR].nature_attr=s.dex[SPECIES_ID_BULBASAUR].nature_attr=0;
 gc_system_unlock_species_nature(&s,SPECIES_ID_IVYSAUR,1);CHECK(s.dex[SPECIES_ID_IVYSAUR].nature_attr==(1U<<2));CHECK(s.dex[SPECIES_ID_BULBASAUR].nature_attr==(1U<<2));
 back=s;gc_system_unlock_species_nature(&s,0,1);gc_system_unlock_species_nature(&s,65535,1);gc_system_unlock_species_nature(&s,1,25);CHECK(!memcmp(&back,&s,sizeof s));
 setup();o=mint(17,-1);CHECK(!gc_port_apply_reward(&r,&o,0,-1,&why));CHECK(!strcmp(why,"partyUiHandler:anyEffect"));
 o.arg=1;CHECK(!gc_port_apply_reward(&r,&o,-1,-1,&why));CHECK(!strcmp(why,"pick a Pokemon"));
 const char *unsupported[]={"BASE_STAT_BOOSTER","SOUL_DEW","MYSTERY_ENCOUNTER_SHUCKLE_JUICE","MYSTERY_ENCOUNTER_OLD_GATEAU","MYSTERY_ENCOUNTER_MACHO_BRACE"};
 for(unsigned i=0;i<sizeof unsupported/sizeof *unsupported;i++)for(int wildcard=0;wildcard<2;wildcard++){
  setup();o=mint(1,-1);gc_item_add(&r,unsupported[i],0,wildcard?0:r.party[0].b.id,1);gc_mon old=r.party[0];
  CHECK(gc_reward_apply(&r,&o,0,-1,&why)==GC_APPLY_UNHANDLED);CHECK(!memcmp(&old,&r.party[0],sizeof old));
  CHECK(gc_port_apply_reward(&r,&o,0,-1,&why));CHECK(r.stubs[GC_STUB_REWARD_APPLY]==1);CHECK(r.party[0].b.nature==17);
 }
 setup();r.run.has_challenges=true;o=mint(1,-1);CHECK(gc_reward_apply(&r,&o,0,-1,&why)==GC_APPLY_UNHANDLED);
 setup();r.party[0].b.hp=0;o=mint(1,-1);CHECK(gc_port_apply_reward(&r,&o,0,-1,&why));CHECK(r.party[0].b.hp==0);
 setup();screen(mint(1,300));gc_choice buy={GC_CH_SHOP,0,0,-1};CHECK(gc_run_choose(&r,&buy,&why));
 CHECK(r.money==700&&r.screen.kind==GC_SCREEN_REWARD&&r.party[0].b.nature==1);CHECK(!gc_run_choose(&r,&buy,&why));CHECK(r.money==700);
 r.screen.shop[0].arg=2;r.money=100;CHECK(!gc_run_choose(&r,&buy,&why));CHECK(r.party[0].b.nature==1&&r.money==100);
 setup();screen(mint(1,-1));gc_choice free_pick={GC_CH_REWARD,0,0,-1};CHECK(gc_run_choose(&r,&free_pick,&why));CHECK(r.party[0].b.nature==1&&r.money==1000);
 CHECK(r.screen.kind==GC_SCREEN_COMMAND&&gc_run_can_save(&r));
 if(argc==2){FILE *f=fopen(argv[1],"rb");CHECK(f!=NULL);n=fread(buf,1,sizeof buf,f);CHECK(!ferror(f));fclose(f);
  CHECK(gc_run_load(&loaded,buf,n,&why));CHECK(loaded.party[0].b.nature==17&&!loaded.party[0].has_custom_nature);
  o=mint(1,-1);CHECK(gc_port_apply_reward(&loaded,&o,0,-1,&why));CHECK(loaded.party[0].base_nature==17&&loaded.party[0].has_custom_nature);}
 printf("mint host: %u checks passed\n",checks);return 0;}
