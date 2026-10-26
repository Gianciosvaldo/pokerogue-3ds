/* Real command owner and legal saves with separate objects sharing an ID. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static void fusion_turn(gc_run*run){choose(run,(gc_choice){GC_CH_FIGHT,0,-1,0});CHECK(run->screen.kind==GC_SCREEN_TARGET);choose(run,(gc_choice){GC_CH_TARGET,2,0,0});CHECK(run->screen.kind==GC_SCREEN_COMMAND&&run->screen.slot==1);choose(run,(gc_choice){GC_CH_FIGHT,1,-1,0});CHECK(run->screen.kind==GC_SCREEN_TARGET);choose(run,(gc_choice){GC_CH_TARGET,2,0,0});CHECK(run->screen.kind==GC_SCREEN_COMMAND&&run->screen.slot==0&&run->bt.turn==2);CHECK(!run->eng.overflow&&!run->battle_events_overflow&&!run->bt.n_unported);CHECK(!run->bt.n_last_turn_order);CHECK(!gc_run_can_save(run)&&!gc_run_save(run,save,sizeof save));}
int main(void){setup(1);CHECK(r.double_battle);r.n_items=0;gc_items_to_battle(&r);
 for(int k=0;k<4;k++){gc_mon*m=k<2?&r.party[k]:&r.enemy[k-2];if(k<2){gc_starter s={.species=SPECIES_ID_BLISSEY,.tera_type=-1,.moves={MOVE_ID_FUSION_FLARE,MOVE_ID_FUSION_BOLT,MOVE_ID_SPLASH}};memset(s.ivs,15,6);gc_mon_new_player(&r,m,&s,1000);}else{pr_enemy_gen g={.id=100u+k,.friendship=70,.tera_type=-1};for(int i=0;i<6;i++)g.ivs[i]=15;gc_mon_new_enemy(&r,m,SPECIES_ID_BLISSEY,1000,&g,0);gc_mon_set_move(m,0,MOVE_ID_SPLASH);}
  pr_pokemon*p=&m->b;pr_calculate_stats(&r.bt,p);p->id=k<2?957185811:33+11*(k-2);p->on_field=true;p->ability=p->base_ability=p->custom_ability=ABILITY_ID_NONE;p->ability_override=-1;p->passive_flag=false;p->stat_stages[STAT_SPD-1]=(int[]){-1,0,-3,-2}[k];r.bt.field[k]=r.bt.party[k/2][k%2]=p;
 }
 /* Constructors use run RNG; project it as normal encounter setup does. */
 r.bt.rng=r.run.rng;
 size_t n=gc_run_save(&r,save,sizeof save);const char*why=NULL;CHECK(n&&gc_run_load(&back,save,n,&why)&&!back.load_dropped);CHECK(back.bt.field[0]!=back.bt.field[1]&&back.bt.field[0]->id==back.bt.field[1]->id);drain(&back);CHECK(rngsame(r.bt.rng,back.bt.rng)&&rngsame(r.bt.battle_rng,back.bt.battle_rng));
 fusion_turn(&r);fusion_turn(&back);for(int k=0;k<4;k++){CHECK(r.bt.field[k]->hp==back.bt.field[k]->hp);CHECK(!memcmp(r.bt.field[k]->pp_used,back.bt.field[k]->pp_used,sizeof r.bt.field[k]->pp_used));}CHECK(rngsame(r.bt.rng,back.bt.rng)&&rngsame(r.bt.battle_rng,back.bt.battle_rng));CHECK(!r.party[1].b.pp_used[0]&&r.party[1].b.pp_used[1]==1);CHECK(r.party[0].b.pp_used[0]==1&&!r.party[0].b.pp_used[1]);
 printf("Turn actors: real Fusion commands with colliding IDs, initial save/restore and turn reset; %d checks passed\n",checks);return 0;}
