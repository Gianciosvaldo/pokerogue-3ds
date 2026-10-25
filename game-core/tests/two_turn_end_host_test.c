/* Actual doubles command/victory/reward/next-encounter owner. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
typedef struct{int move,mode,result,n;int targets[4];}native_charge_move;
typedef struct{int type,move,own,turns;}native_charge_tag;
typedef struct{int nq;native_charge_move queue[4];int nt;native_charge_tag tags[4];int visible,pp,stages[7],nh;native_charge_move history[8];}native_charge_frame;
typedef struct{int move,slot,target;native_charge_frame before,cleaned;}native_charge_case;
#include "fixtures/two-turn-end.native.inc"
static const native_charge_case* current_native;
static void match_move(const pr_turn_move*got,const native_charge_move*want,bool result){CHECK(got->move==want->move&&got->use_mode==want->mode&&got->n_targets==want->n);if(result)CHECK(got->result==want->result);for(int i=0;i<want->n;i++)CHECK(got->targets[i]==want->targets[i]);}
static void match_frame(const pr_pokemon*p,const native_charge_frame*f){
 CHECK(p->n_move_queue==f->nq&&p->n_tags==f->nt&&p->pp_used[0]==f->pp&&p->n_move_history==f->nh);
 CHECK((p->on_field&&!pr_find_battler_tag_class(p,PR_TAGCLASS_SEMI_INVULNERABLE))==f->visible);
 for(int i=0;i<7;i++)CHECK(p->stat_stages[i]==f->stages[i]);
 for(int i=0;i<f->nq;i++)match_move(&p->move_queue[i],&f->queue[i],false);
 for(int i=0;i<f->nh;i++)match_move(&p->move_history[i],&f->history[i],true);
 for(int i=0;i<f->nt;i++){const pr_battler_tag*t=&p->tags[i];CHECK(t->type==f->tags[i].type&&t->source_move==f->tags[i].move&&t->source_id==(f->tags[i].own?(int64_t)p->id:-1)&&t->turn_count==f->tags[i].turns);}
}
static int watched_slot=-1,watched_move,cleanups,visible_frames;
static bool observe=true;
static int next_widths;
void __real_gc_battle_end_cleanup(gc_run*run);
void __wrap_gc_battle_end_cleanup(gc_run*run){
 if(!observe){__real_gc_battle_end_cleanup(run);return;}
 pr_pokemon*p=run->bt.field[watched_slot];CHECK(p&&pr_get_battler_tag(p,BATTLER_TAG_TYPE_CHARGING));CHECK(p->n_move_queue==1&&p->move_queue[0].move==watched_move);
 match_frame(p,&current_native->before);
 pr_pokemon before=*p;pr_rng global=run->bt.rng,battle=run->bt.battle_rng,local=run->run.rng;int events=run->n_battle_events;
 bool hidden=pr_find_battler_tag_class(p,PR_TAGCLASS_SEMI_INVULNERABLE)!=NULL;
 __real_gc_battle_end_cleanup(run);cleanups++;match_frame(p,&current_native->cleaned);
 CHECK(!p->n_move_queue&&!pr_get_battler_tag(p,BATTLER_TAG_TYPE_CHARGING)&&!pr_find_battler_tag_class(p,PR_TAGCLASS_SEMI_INVULNERABLE));
 CHECK(p->pp_used[0]==1&&p->pp_used[0]==before.pp_used[0]&&p->hp==before.hp&&p->level==before.level&&p->id==before.id);
 CHECK(!memcmp(p->stats,before.stats,sizeof p->stats)&&!memcmp(p->stat_stages,before.stat_stages,sizeof p->stat_stages));
 CHECK(p->n_move_history==before.n_move_history&&!memcmp(p->move_history,before.move_history,sizeof p->move_history));
 CHECK(p->history_protect_count==before.history_protect_count&&p->history_protect_complete==before.history_protect_complete&&p->history_last_known==before.history_last_known&&p->history_last_present==before.history_last_present);
 CHECK(!memcmp(p->history_last,before.history_last,sizeof p->history_last)&&!memcmp(p->history_used_moves,before.history_used_moves,sizeof p->history_used_moves));
 CHECK(rngsame(global,run->bt.rng)&&rngsame(battle,run->bt.battle_rng)&&rngsame(local,run->run.rng));
 if(hidden){CHECK(run->n_battle_events==events+1&&run->battle_events[events].type==PR_EV_NONE&&run->battle_events[events].field[watched_slot].visible);visible_frames++;}
 else CHECK(run->n_battle_events==events);
}
static int first_target(const gc_screen*s,int wanted){if(wanted>=0&&s->target_ok[wanted])return wanted;for(int k=0;k<4;k++)if(s->target_ok[k])return k;return-1;}
static void play(int move,int slot,int target,int seed){
 setup(seed);CHECK(r.double_battle&&r.battle_type==BATTLE_TYPE_WILD);r.n_items=0;gc_items_to_battle(&r);r.bt.weather=WEATHER_TYPE_NONE;r.bt.terrain=TERRAIN_NONE;
 watched_move=move;watched_slot=slot;int starts=cleanups;
 for(int k=0;k<4;k++){
  pr_pokemon*p=r.bt.field[k];CHECK(p);p->hp=p->stats[STAT_HP]=k<2?100000:1;
  for(int j=1;j<6;j++)p->stats[j]=k<2?100000:1;
  p->stats[STAT_SPD]=k==slot?4000:k<2?2000:1;
  memset(p->stat_stages,0,sizeof p->stat_stages);p->n_moves=2;p->moves[0]=k==slot?move:k<2?MOVE_ID_DAZZLING_GLEAM:MOVE_ID_SPLASH;p->moves[1]=MOVE_ID_SPLASH;
  p->ability=p->base_ability=p->custom_ability=ABILITY_ID_NONE;p->passive_flag=false;
  for(int j=0;j<4;j++)p->pp_used[j]=p->pp_up[j]=0,p->max_pp_override[j]=-1;
 }
 pr_pokemon*p=r.bt.field[slot];p->stat_stages[1]=2;pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_FURY_CUTTER,.use_mode=MOVE_USE_MODE_NORMAL,.result=MOVE_RESULT_SUCCESS,.n_targets=1,.targets={2}});
 uint32_t id=p->id;int wave=r.wave;
 for(int step=0;step<40&&r.screen.kind!=GC_SCREEN_REWARD;step++){
  gc_choice c={0};switch(r.screen.kind){
   case GC_SCREEN_COMMAND:c=(gc_choice){GC_CH_FIGHT,0,-1,0};break;
   case GC_SCREEN_TARGET:c=(gc_choice){GC_CH_TARGET,first_target(&r.screen,r.screen.slot==slot?target:2),0,0};break;
   case GC_SCREEN_LEARN_MOVE:c=(gc_choice){GC_CH_FORGET,-1,0,0};break;
   case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;break;
   default:fprintf(stderr,"two-turn move%d unexpected screen%d\n",move,r.screen.kind);CHECK(false);
  }choose(&r,c);
 }
 CHECK(r.screen.kind==GC_SCREEN_REWARD&&cleanups==starts+1&&!r.battle_events_overflow&&!r.bt.n_unported);
 // The following natural wave can be single or double; every surviving owner is clear.
 for(int step=0;step<40&&r.wave==wave;step++){
  gc_choice c={0};switch(r.screen.kind){case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;case GC_SCREEN_LEARN_MOVE:c=(gc_choice){GC_CH_FORGET,-1,0,0};break;case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;break;case GC_SCREEN_CHECK_SWITCH:c.kind=GC_CH_CONFIRM;break;default:fprintf(stderr,"next wave screen%d\n",r.screen.kind);CHECK(false);}choose(&r,c);
 }
 CHECK(r.wave==wave+1);next_widths|=1<<r.double_battle;p=NULL;for(int k=0;k<r.n_party;k++)if(r.party[k].b.id==id)p=&r.party[k].b;
 CHECK(p&&!p->n_move_queue&&!pr_get_battler_tag(p,BATTLER_TAG_TYPE_CHARGING)&&!pr_find_battler_tag_class(p,PR_TAGCLASS_SEMI_INVULNERABLE));
 for(int step=0;step<20&&r.screen.kind!=GC_SCREEN_COMMAND;step++){
  CHECK(r.screen.kind==GC_SCREEN_CHECK_SWITCH);choose(&r,(gc_choice){GC_CH_CONFIRM,0,0,0});
 }
 CHECK(r.screen.kind==GC_SCREEN_COMMAND&&gc_run_can_save(&r));
 const char*why=NULL;size_t bytes=gc_run_save(&r,save,sizeof save);if(!bytes||!gc_run_load(&back,save,bytes,&why)||back.load_dropped){fprintf(stderr,"two-turn save move%d slot%d target%d bytes%zu why%s dropped%d\n",move,slot,target,bytes,why?why:"-",back.load_dropped);CHECK(false);}
 const pr_pokemon*restored=NULL;for(int k=0;k<back.n_party;k++)if(back.party[k].b.id==id)restored=&back.party[k].b;
 CHECK(restored&&!restored->n_move_queue&&!pr_get_battler_tag(restored,BATTLER_TAG_TYPE_CHARGING)&&!pr_find_battler_tag_class(restored,PR_TAGCLASS_SEMI_INVULNERABLE));
}

static bool native_charge_id(int move){for(size_t i=0;i<sizeof native_charging_ids/sizeof native_charging_ids[0];i++)if(native_charging_ids[i]==move)return true;return false;}
static void catalog_guards(void){
 setup(1);r.n_party=1;r.bt.field[1]=NULL;pr_pokemon base=r.party[0].b;
 for(int move=0;move<PR_MOVE_COUNT;move++){
  pr_pokemon*p=&r.party[0].b;*p=base;p->on_field=true;p->stat_stages[0]=2;p->pp_used[0]=3;
  p->n_move_queue=4;p->move_queue[0]=(pr_turn_move){.move=MOVE_ID_OUTRAGE};p->move_queue[1]=(pr_turn_move){.move=move};p->move_queue[2]=(pr_turn_move){.move=MOVE_ID_NONE};p->move_queue[3]=(pr_turn_move){.move=MOVE_ID_ROLLOUT};
  p->n_tags=3;p->tags[0]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_CHARGING,.source_move=move,.source_id=p->id,.turn_count=1};p->tags[1]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_FLYING,.source_move=move,.turn_count=1};p->tags[2]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_INGRAIN,.source_move=MOVE_ID_INGRAIN};
  pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_FURY_CUTTER,.use_mode=1,.result=1});
  r.bt.field[0]=p;r.eng.n_events=r.n_battle_events=r.ev_head=r.ev_count=0;r.run.rng=r.bt.rng;
  pr_rng global=r.bt.rng,battle=r.bt.battle_rng,local=r.run.rng;
  __real_gc_battle_end_cleanup(&r);bool charge=native_charge_id(move);
  CHECK(p->n_move_queue==(charge?3:4)&&p->n_tags==(charge?1:3));
  CHECK(p->move_queue[0].move==MOVE_ID_OUTRAGE&&p->move_queue[charge?1:2].move==MOVE_ID_NONE&&p->move_queue[charge?2:3].move==MOVE_ID_ROLLOUT);
  if(!charge)CHECK(p->move_queue[1].move==move&&p->tags[0].type==BATTLER_TAG_TYPE_CHARGING&&p->tags[1].type==BATTLER_TAG_TYPE_FLYING);
  CHECK(p->tags[p->n_tags-1].type==BATTLER_TAG_TYPE_INGRAIN&&p->stat_stages[0]==2&&p->pp_used[0]==3&&p->n_move_history==1&&p->move_history[0].move==MOVE_ID_FURY_CUTTER);
  CHECK(r.n_battle_events==(charge?1:0)&&rngsame(global,r.bt.rng)&&rngsame(battle,r.bt.battle_rng)&&rngsame(local,r.run.rng));
 }
 // Old or partial state: queue without tags, charging without a queue, and an orphan hiding tag.
 for(int shape=0;shape<3;shape++){
  pr_pokemon*p=&r.party[0].b;*p=base;r.eng.n_events=r.n_battle_events=r.ev_head=r.ev_count=0;
  p->n_move_queue=shape==0?1:0;p->move_queue[0]=(pr_turn_move){.move=MOVE_ID_DIG};p->n_tags=shape?1:0;p->tags[0]=(pr_battler_tag){.type=shape==1?BATTLER_TAG_TYPE_CHARGING:BATTLER_TAG_TYPE_UNDERGROUND,.source_move=MOVE_ID_DIG,.source_id=p->id,.turn_count=1};
  __real_gc_battle_end_cleanup(&r);CHECK(!p->n_move_queue&&!p->n_tags&&r.n_battle_events==(shape==2?1:0));
 }
 // A stale bench charge is cleaned without publishing a sprite for that bench.
 r.n_party=3;r.party[0].b.n_tags=r.party[0].b.n_move_queue=0;pr_pokemon*bench=&r.party[2].b;*bench=base;bench->on_field=false;bench->n_move_queue=1;bench->move_queue[0]=(pr_turn_move){.move=MOVE_ID_DIG};bench->n_tags=1;bench->tags[0]=(pr_battler_tag){.type=BATTLER_TAG_TYPE_UNDERGROUND,.source_move=MOVE_ID_DIG,.turn_count=1};r.eng.n_events=r.n_battle_events=r.ev_head=r.ev_count=0;
 __real_gc_battle_end_cleanup(&r);CHECK(!bench->n_move_queue&&!bench->n_tags&&!r.n_battle_events);
}
int main(void){
 catalog_guards();
 for(int i=0;i<28;i++){current_native=&native_charge_cases[i];play(current_native->move,current_native->slot,current_native->target,1);}
 for(int seed=2;seed<=17;seed++){current_native=&native_charge_cases[1];play(current_native->move,current_native->slot,current_native->target,seed);}
 CHECK(cleanups==44&&visible_frames==44&&next_widths==3);printf("Two-turn encounter cleanup: 28 pinned charge/cleanup frames, 44 real doubles victories with single/double successors, next-command saves and encounters; %d checks passed\n",checks);return 0;
}
