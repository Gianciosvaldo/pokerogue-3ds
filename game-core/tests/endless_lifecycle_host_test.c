/* Extend, rather than replace, the retained 16 wide-boundary contexts. */
#define main retained_wide_journey_main
#include "endless_journey_host_test.c"
#undef main
static int learned[2],evolved[2],terminal[2],life_wins[2],life_losses[2];
typedef struct { int kind,a,b,c,hp;double exp_amount,exp_before; } progress_event;
typedef struct { int count;progress_event event[16]; } progress_trace;
static progress_trace observed[2];
static void progression(gc_run *r,int family,bool count,progress_trace *trace){
 memset(trace,0,sizeof *trace);gc_event ev;while(gc_run_next_event(r,&ev)){
  if(ev.kind==GC_EV_EXP||ev.kind==GC_EV_LEVEL_UP||ev.kind==GC_EV_MOVE_LEARNED||ev.kind==GC_EV_EVOLVED){
   CHECK(trace->count<16);progress_event *record=&trace->event[trace->count++];
   record->kind=ev.kind;record->a=ev.a;record->b=ev.b;record->c=ev.c;record->hp=ev.hp;
   record->exp_amount=ev.exp_amount;record->exp_before=ev.exp_before;
   if(ev.kind==GC_EV_EXP)CHECK(ev.exp_amount>=0&&isfinite(ev.exp_amount)&&isfinite(ev.exp_before));
   if(family==0&&ev.kind==GC_EV_LEVEL_UP)CHECK(ev.c==19&&ev.b==20);
   if(family==0&&ev.kind==GC_EV_EVOLVED){CHECK(ev.b==SPECIES_ID_RALTS&&ev.c==SPECIES_ID_KIRLIA);if(count)evolved[mode]++;}
   if(family==0&&ev.kind==GC_EV_MOVE_LEARNED){CHECK(ev.c==MOVE_ID_PSYBEAM&&ev.b==3);if(count)learned[mode]++;}
  }
 }CHECK(!r->ev_overflow);
}
static void compare_lifecycle(void){compare();CHECK(!run.bt.n_deferred&&!loaded.bt.n_deferred);}
static void lifecycle(int family){
 const char *why=NULL;char seed[48];snprintf(seed,sizeof seed,"endless-integration-%d",seed_index);
 gc_run_new(&run,mode?GAME_MODES_SPLICED_ENDLESS:GAME_MODES_ENDLESS,seed,777);
 gc_starter s={.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_SEED_FLARE,MOVE_ID_EARTH_POWER,MOVE_ID_FLAMETHROWER,MOVE_ID_ICE_BEAM}};memset(s.ivs,31,6);
 CHECK(gc_run_set_starters(&run,&s,1,&why));drain(&run,false);
 const int first_wave=family?5849:25;
 run.wave=first_wave-1;s.species=family?SPECIES_ID_MEWTWO:SPECIES_ID_RALTS;s.moves[0]=MOVE_ID_PSYSTRIKE;s.moves[1]=MOVE_ID_AURA_SPHERE;
 gc_mon_new_player(&run,&run.party[0],&s,family?1000000:19);
 run.party[0].exp=gc_level_total_exp(family?1000000:20,gc_species(run.party[0].b.species)->growth_rate)-(family?0:1);
 gc_rq_clear(&run);gc_int(&run)->rq.waiting=false;gc_int(&run)->rq.current.type=RP_NONE;
 gc_rq_push(&run,gc_rp(RP_NEW_BATTLE,0,0,0));gc_run_until_screen(&run);drain(&run,false);
 CHECK(run.wave==first_wave&&gc_run_can_save(&run));size_t n=gc_run_save(&run,bytes,sizeof bytes);
 CHECK(n&&gc_run_load(&loaded,bytes,n,&why));CHECK(!loaded.load_dropped);same_save();compare_lifecycle();
 int steps=0;bool saved_final=false;
 while(!run.finished&&(family||run.wave==first_wave)&&steps++<300){
  if(family&&run.wave==5850&&gc_run_can_save(&run)&&!saved_final){same_save();n=gc_run_save(&run,bytes,sizeof bytes);CHECK(n&&gc_run_load(&loaded,bytes,n,&why));CHECK(!loaded.load_dropped);compare_lifecycle();saved_final=true;}
  gc_choice c={GC_CH_FIGHT,0,-1,0};
  switch(run.screen.kind){
   case GC_SCREEN_COMMAND:{double best=-1;for(int j=0;j<4;j++)if(run.screen.move_ok[j]){int move=run.party[0].b.moves[j];double value=pr_moves[move].power*pr_get_attack_type_effectiveness(&run.bt,&run.enemy[0].b,pr_moves[move].type,&run.party[0].b,move);if(value>best){best=value;c.a=j;}}break;}
   case GC_SCREEN_TARGET:c.kind=GC_CH_TARGET;c.a=2;break;
   case GC_SCREEN_REWARD:CHECK(!family||run.wave<5850);c.kind=GC_CH_SKIP;break;
   case GC_SCREEN_LEARN_MOVE:c.kind=GC_CH_FORGET;c.a=3;break;
   case GC_SCREEN_EVOLVE:c.kind=GC_CH_EVOLVE;break;
   case GC_SCREEN_BIOME:c.kind=GC_CH_BIOME;c.a=0;break;
   case GC_SCREEN_CHECK_SWITCH:c.kind=GC_CH_CONFIRM;c.a=0;break;
   default:CHECK(false);
  }
  CHECK(gc_run_choose(&run,&c,&why));progression(&run,family,true,&observed[0]);
  CHECK(gc_run_choose(&loaded,&c,&why));progression(&loaded,family,false,&observed[1]);CHECK(!memcmp(observed,observed+1,sizeof observed[0]));compare_lifecycle();
 }
 CHECK(steps<300);
 if(family){CHECK(saved_final&&run.finished&&run.screen.kind==GC_SCREEN_CLEAR&&run.wave==5850);CHECK(!gc_run_can_save(&run)&&!gc_run_can_save(&loaded));CHECK(!gc_rq_has(&run,RP_NEW_BATTLE)&&!gc_rq_has(&run,RP_SELECT_MODIFIER));terminal[mode]++;}
 else if(run.wave==26){life_wins[mode]++;CHECK(run.party[0].b.species==SPECIES_ID_KIRLIA&&run.party[0].b.moves[3]==MOVE_ID_PSYBEAM);same_save();}
 else {life_losses[mode]++;CHECK(run.finished&&run.screen.kind==GC_SCREEN_GAME_OVER);}
}
int main(void){CHECK(retained_wide_journey_main()==0);
 for(int family=0;family<2;family++)for(mode=0;mode<2;mode++)for(seed_index=0;seed_index<8;seed_index++)lifecycle(family);
 for(mode=0;mode<2;mode++){CHECK(learned[mode]>0&&evolved[mode]>0&&life_wins[mode]>0&&life_losses[mode]>0);CHECK(terminal[mode]==8);}
 printf("Endless lifecycle: 32 added contexts; learned %d/%d, evolved %d/%d, learning-chain wins %d/%d losses %d/%d, terminal clears %d/%d; %d combined checks passed\n",learned[0],learned[1],evolved[0],evolved[1],life_wins[0],life_wins[1],life_losses[0],life_losses[1],terminal[0],terminal[1],checks);return 0;
}
