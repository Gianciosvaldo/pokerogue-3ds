/* Real controller commands, turn reset, encounter transition and save boundary. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
int main(void){
 setup(1);CHECK(r.double_battle);r.n_items=0;gc_items_to_battle(&r);
 for(int k=0;k<4;k++){
  gc_mon *m=k<2?&r.party[k]:&r.enemy[k-2];
  if(k<2){gc_starter s={.species=SPECIES_ID_BLISSEY,.tera_type=-1,.moves={MOVE_ID_FIERY_DANCE,MOVE_ID_INSTRUCT,MOVE_ID_SPLASH,MOVE_ID_DAZZLING_GLEAM}};memset(s.ivs,15,6);gc_mon_new_player(&r,m,&s,1000);}
  else{pr_enemy_gen g={.id=100u+k,.friendship=70,.tera_type=-1};for(int i=0;i<6;i++)g.ivs[i]=15;gc_mon_new_enemy(&r,m,SPECIES_ID_BLISSEY,1000,&g,0);for(int i=0;i<4;i++)gc_mon_set_move(m,i,MOVE_ID_SPLASH);}
  pr_pokemon*p=&m->b;pr_calculate_stats(&r.bt,p);p->on_field=true;p->ability=p->base_ability=p->custom_ability=ABILITY_ID_DANCER;p->ability_override=-1;p->passive_flag=false;p->stat_stages[STAT_SPD-1]=(int[]){0,-1,-3,-2}[k];r.bt.field[k]=r.bt.party[k/2][k%2]=p;
 }
 choose(&r,(gc_choice){GC_CH_FIGHT,0,-1,0});CHECK(r.screen.kind==GC_SCREEN_TARGET);choose(&r,(gc_choice){GC_CH_TARGET,2,0,0});CHECK(r.screen.kind==GC_SCREEN_COMMAND&&r.screen.slot==1);
 choose(&r,(gc_choice){GC_CH_FIGHT,1,-1,0});CHECK(r.screen.kind==GC_SCREEN_TARGET);choose(&r,(gc_choice){GC_CH_TARGET,0,0,0});
 CHECK(!r.eng.overflow&&!r.battle_events_overflow&&!r.bt.n_unported);CHECK(r.screen.kind==GC_SCREEN_COMMAND&&r.screen.slot==0&&r.bt.turn==2);
 int moves=0;for(int i=0;i<r.eng.n_trace;i++)if(!strncmp(r.eng.trace[i],"MovePhase:",10))moves++;
 CHECK(moves==11);CHECK(r.party[0].b.pp_used[0]==2&&r.party[1].b.pp_used[1]==1);
 uint32_t prior[12];bool complete=false;CHECK(!r.bt.n_last_turn_order&&pr_prior_turn_movers(&r.bt,prior,&complete)==0&&complete);CHECK(!gc_run_can_save(&r)&&!gc_run_save(&r,save,sizeof save));
 int wave=r.wave;for(int step=0;step<160&&r.wave==wave;step++){
  gc_choice c={0};switch(r.screen.kind){
   case GC_SCREEN_COMMAND:c=r.screen.slot==0?(gc_choice){GC_CH_RUN,0,0,0}:(gc_choice){GC_CH_FIGHT,2,-1,0};break;
   case GC_SCREEN_TARGET:c=(gc_choice){GC_CH_TARGET,2,0,0};break;
   case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;
   case GC_SCREEN_LEARN_MOVE:c=(gc_choice){GC_CH_FORGET,-1,0,0};break;
   case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;break;
   case GC_SCREEN_CHECK_SWITCH:c.kind=GC_CH_CONFIRM;break;
   default:fprintf(stderr,"turn-order lifecycle screen%d\n",r.screen.kind);CHECK(false);
  }choose(&r,c);
 }
 while(r.screen.kind==GC_SCREEN_CHECK_SWITCH)choose(&r,(gc_choice){GC_CH_CONFIRM,0,0,0});
 CHECK(r.wave==wave+1&&gc_run_can_save(&r));CHECK(!r.bt.n_last_turn_order&&!r.bt.n_unported);size_t n=gc_run_save(&r,save,sizeof save);const char*why=NULL;CHECK(n&&gc_run_load(&back,save,n,&why)&&!back.load_dropped);CHECK(!back.bt.n_last_turn_order&&pr_prior_turn_movers(&back.bt,prior,&complete)==0&&complete);
 printf("Turn-order controller: eleven MovePhases, next encounter and save; %d checks passed\n",checks);return 0;
}
