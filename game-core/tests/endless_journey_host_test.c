/* Chained high-wave save/resume regression. Player and wave are prepared API
 * inputs; generated opponents, damage, EXP, reward and next encounter are real. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run run,loaded;
static unsigned char bytes[GC_SAVE_MAX],again[GC_SAVE_MAX];
static int checks,mode,seed_index,exp_events,level_events;
#define CHECK(x) do { checks++; if(!(x)){fprintf(stderr,"Endless journey mode%d seed%d line%d: %s\n",mode,seed_index,__LINE__,#x);exit(1);} } while(0)
static void drain(gc_run *r,bool count){gc_event ev;while(gc_run_next_event(r,&ev)){
 if(count&&ev.kind==GC_EV_EXP){exp_events++;CHECK(ev.exp_amount>=0&&isfinite(ev.exp_amount));}
 if(count&&ev.kind==GC_EV_LEVEL_UP){level_events++;CHECK(ev.b>=65536&&ev.c==65535);CHECK(ev.hp>0);}
}CHECK(!r->ev_overflow);}
static void compare(void){
 CHECK(run.wave==loaded.wave&&run.screen.kind==loaded.screen.kind&&run.screen.slot==loaded.screen.slot);
 CHECK(run.finished==loaded.finished&&run.money==loaded.money&&run.n_enemy==loaded.n_enemy);
 CHECK(!memcmp(&run.run.rng,&loaded.run.rng,sizeof(pr_rng)));
 CHECK(!memcmp(&run.bt.rng,&loaded.bt.rng,sizeof(pr_rng)));
 CHECK(!memcmp(&run.bt.battle_rng,&loaded.bt.battle_rng,sizeof(pr_rng)));
 for(int side=0;side<2;side++)for(int j=0;j<(side?run.n_enemy:run.n_party);j++){
  const gc_mon *a=side?&run.enemy[j]:&run.party[j],*b=side?&loaded.enemy[j]:&loaded.party[j];
  CHECK(a->exp==b->exp&&a->b.level==b->b.level&&a->b.hp==b->b.hp&&a->b.status==b->b.status);
  CHECK(a->b.id==b->b.id&&a->b.species==b->b.species&&a->b.form==b->b.form&&a->b.fusion_species==b->b.fusion_species&&a->b.ability==b->b.ability);
  CHECK(!memcmp(a->b.stats,b->b.stats,sizeof a->b.stats));
  CHECK(a->b.n_moves==b->b.n_moves&&!memcmp(a->b.moves,b->b.moves,sizeof a->b.moves));
  CHECK(!memcmp(a->b.pp_used,b->b.pp_used,sizeof a->b.pp_used));
 }
 CHECK(!run.run.unported&&!loaded.run.unported&&!run.bt.n_unported&&!loaded.bt.n_unported);
 for(int j=0;j<GC_STUB_COUNT_;j++)CHECK(!run.stubs[j]&&!loaded.stubs[j]);
}
static void same_save(void){const char *why=NULL;CHECK(gc_run_can_save(&run)&&gc_run_can_save(&loaded));size_t n=gc_run_save(&run,bytes,sizeof bytes),m=gc_run_save(&loaded,again,sizeof again);CHECK(n&&n==m&&!memcmp(bytes,again,n));(void)why;}
int main(void){
 int wins[2]={0},losses[2]={0},doubles[2]={0},bosses[2]={0},locked=0;
 for(mode=0;mode<2;mode++)for(seed_index=0;seed_index<8;seed_index++){
  char seed[48];snprintf(seed,sizeof seed,"endless-integration-%d",seed_index);const char *why=NULL;
  gc_run_new(&run,mode?GAME_MODES_SPLICED_ENDLESS:GAME_MODES_ENDLESS,seed,777);
  gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SEED_FLARE,MOVE_ID_EARTH_POWER,MOVE_ID_FLAMETHROWER,MOVE_ID_ICE_BEAM}};memset(s.ivs,31,6);
  CHECK(gc_run_set_starters(&run,&s,1,&why));drain(&run,false);
  /* Controlled progression boundary, not a naturally played 5,700-wave history. */
  run.wave=5700;s.species=SPECIES_ID_MEWTWO;s.moves[0]=MOVE_ID_PSYSTRIKE;s.moves[1]=MOVE_ID_AURA_SPHERE;
  gc_mon_new_player(&run,&run.party[0],&s,65535);
  run.party[0].exp=gc_level_total_exp(65536,gc_species(run.party[0].b.species)->growth_rate)-1;
  gc_rq_clear(&run);gc_int(&run)->rq.waiting=false;gc_int(&run)->rq.current.type=RP_NONE;
  gc_rq_push(&run,gc_rp(RP_NEW_BATTLE,0,0,0));gc_run_until_screen(&run);drain(&run,false);
  CHECK(run.wave==5701&&run.n_enemy>=1);doubles[mode]+=run.double_battle;bosses[mode]+=run.enemy[0].b.boss_segments>0;
  for(int i=0;i<run.n_enemy_items;i++)if(gc_item_key_locked(run.enemy_items[i].key)){locked++;CHECK(run.enemy_items[i].non_transferable);}
  CHECK(gc_run_can_save(&run));size_t length=gc_run_save(&run,bytes,sizeof bytes);CHECK(length&&gc_run_load(&loaded,bytes,length,&why));CHECK(!loaded.load_dropped);same_save();compare();
  int steps=0;while(run.wave==5701&&!run.finished&&steps++<80){
   gc_choice c={GC_CH_FIGHT,0,-1,0};
   switch(run.screen.kind){
    case GC_SCREEN_COMMAND:{double best=-1;for(int j=0;j<4;j++)if(run.screen.move_ok[j]){int move=run.party[0].b.moves[j];double value=pr_moves[move].power*pr_get_attack_type_effectiveness(&run.bt,&run.enemy[0].b,pr_moves[move].type,&run.party[0].b,move);if(value>best){best=value;c.a=j;}}break;}
    case GC_SCREEN_TARGET:c.kind=GC_CH_TARGET;c.a=2;break;
    case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;
    case GC_SCREEN_LEARN_MOVE:c.kind=GC_CH_FORGET;c.a=-1;break;
    case GC_SCREEN_EVOLVE:c.kind=GC_CH_EVOLVE;break;
    case GC_SCREEN_BIOME:c.kind=GC_CH_BIOME;c.a=0;break;
    case GC_SCREEN_CHECK_SWITCH:c.kind=GC_CH_CONFIRM;c.a=0;break;
    default:CHECK(false);
   }
   CHECK(gc_run_choose(&run,&c,&why));drain(&run,true);
   CHECK(gc_run_choose(&loaded,&c,&why));drain(&loaded,false);compare();
  }
  CHECK(steps<80);if(run.wave==5702){wins[mode]++;CHECK(run.party[0].b.level==65536);same_save();}
  else {losses[mode]++;CHECK(run.finished&&run.screen.kind==GC_SCREEN_GAME_OVER);}
 }
 for(int j=0;j<2;j++)CHECK(wins[j]>=1&&losses[j]>=1&&doubles[j]>=1&&bosses[j]>=1);
 CHECK(locked>0&&exp_events>0&&level_events>0);
 printf("Endless journey: 16 retained contexts; wins %d/%d, losses %d/%d, doubles %d/%d, bosses %d/%d, locked stock %d, EXP %d, level events %d; %d checks passed\n",wins[0],wins[1],losses[0],losses[1],doubles[0],doubles[1],bosses[0],bosses[1],locked,exp_events,level_events,checks);return 0;
}
