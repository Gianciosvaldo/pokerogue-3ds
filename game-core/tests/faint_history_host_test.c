/* Real command -> sacrificial move -> forced replacement -> Revival Blessing,
 * with finite ordinary PP, no injected revives or PP refunds after setup. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
static int own_index(const pr_pokemon*p){for(int k=0;k<r.n_party;k++)if(p==&r.party[k].b)return k;return -1;}
int main(void){
 const char*why=NULL;gc_starter team[6]={0};
 gc_run_new(&r,GAME_MODES_ENDLESS,"doubles-controller-1",1);
 r.n_items=3;for(int k=0;k<3;k++)r.items[k]=(gc_item){.key=(const char*[]){"LURE","SUPER_LURE","MAX_LURE"}[k],.stack=1,.arg=-1,.battle_count=(int[]){10,15,30}[k]};
 for(int k=0;k<6;k++){team[k].species=k?SPECIES_ID_PAWMOT:SPECIES_ID_ELECTRODE;team[k].tera_type=-1;memset(team[k].ivs,15,sizeof team[k].ivs);}
 gc_starter basic[3]={team[0],team[1],team[2]};basic[0].species=SPECIES_ID_BULBASAUR;basic[1].species=SPECIES_ID_CHARMANDER;basic[2].species=SPECIES_ID_SQUIRTLE;
 if(!gc_run_set_starters(&r,basic,3,&why)){fprintf(stderr,"faint starters %s\n",why?why:"-");CHECK(false);}
 for(int k=0;k<6;k++){gc_mon_new_player(&r,&r.party[k],&team[k],30);r.party_order[k]=k;r.party[k].b.on_field=k<2;}
 r.bt.n_participants=2;r.bt.participants[0]=r.party[0].b.id;r.bt.participants[1]=r.party[1].b.id;r.n_party=6;r.bt.n_party[0]=6;for(int k=0;k<6;k++)r.bt.party[0][k]=&r.party[k].b;drain(&r);CHECK(r.double_battle&&r.bt.field[0]==&r.party[0].b);
 r.n_items=0;gc_items_to_battle(&r);
 for(int k=0;k<6;k++){
  pr_pokemon*p=&r.party[k].b;p->n_moves=2;p->moves[0]=k?MOVE_ID_REVIVAL_BLESSING:MOVE_ID_SELF_DESTRUCT;p->moves[1]=k?MOVE_ID_GROWL:MOVE_ID_EXPLOSION;
  p->hp=p->stats[STAT_HP]=100000;for(int stat=1;stat<6;stat++)p->stats[stat]=1000;p->stats[STAT_SPD]=k?100:2000;
  p->custom_ability=p->ability=p->base_ability=ABILITY_ID_NONE;p->ability_override=-1;p->passive_flag=false;
  for(int m=0;m<4;m++){p->pp_used[m]=0;p->pp_up[m]=3;p->max_pp_override[m]=-1;}
 }
 for(int k=2;k<4;k++){pr_pokemon*p=r.bt.field[k];CHECK(p);p->hp=p->stats[STAT_HP]=1000000000;p->n_moves=1;p->moves[0]=MOVE_ID_SPLASH;p->pp_used[0]=0;p->pp_up[0]=3;p->max_pp_override[0]=-1;p->stats[STAT_SPD]=1;p->ability=p->base_ability=p->custom_ability=ABILITY_ID_NONE;p->ability_override=-1;p->passive_flag=false;}
 bool planned_death=false,planned_revive=false;int revivals=0;pr_pokemon*subject=&r.party[0].b;
 for(int step=0;step<500;step++){
  if(r.screen.kind==GC_SCREEN_COMMAND&&revivals>=13&&subject->hp>0)break;
  gc_choice c={0};
  switch(r.screen.kind){
   case GC_SCREEN_COMMAND:{
    if(r.screen.slot==0){planned_death=false;planned_revive=false;}
    pr_pokemon*active=r.bt.field[r.screen.slot];int index=own_index(active);CHECK(index>=0);
    if(index==0){int move=active->pp_used[0]<8?0:1;CHECK(active->pp_used[move]<8);c=(gc_choice){GC_CH_FIGHT,move,-1,0};planned_death=true;}
    else if(subject->hp>0&&pr_battler_index(&r.bt,subject)<0&&r.screen.slot==0)c=(gc_choice){GC_CH_POKEMON,0,-1,0};
    else if((subject->hp<=0||planned_death)&&!planned_revive&&active->pp_used[0]<4){c=(gc_choice){GC_CH_FIGHT,0,-1,0};planned_revive=true;}
    else c=(gc_choice){GC_CH_FIGHT,1,-1,0};break;
   }
   case GC_SCREEN_TARGET:{int target=2;while(target<4&&!r.screen.target_ok[target])target++;CHECK(target<4);c=(gc_choice){GC_CH_TARGET,target,0,0};break;}
   case GC_SCREEN_SWITCH:
    if(r.screen.revival_blessing){CHECK(r.screen.pick_ok[0]&&subject->hp<=0);c=(gc_choice){GC_CH_PICK,0,-1,0};revivals++;}
    else{int pick=-1;for(int k=1;k<6;k++)if(r.screen.pick_ok[k]&&r.party[k].b.pp_used[0]<4){pick=k;break;}if(pick<0)for(int k=1;k<6;k++)if(r.screen.pick_ok[k]){pick=k;break;}CHECK(pick>=0);c=(gc_choice){GC_CH_PICK,pick,-1,0};}break;
   default:fprintf(stderr,"faint command unexpected screen%d turn%d history%u\n",r.screen.kind,r.bt.turn,r.bt.faint_history_total[0]);CHECK(false);
  }
  choose(&r,c);CHECK(!r.eng.overflow&&!r.battle_events_overflow&&!r.bt.n_unported);
  CHECK(!gc_run_can_save(&r));
 }
 CHECK(r.screen.kind==GC_SCREEN_COMMAND&&r.bt.faint_history_total[0]>=13&&r.bt.n_faint_history[0]==12&&revivals==13&&subject->hp>0);
 bool unknown=true;CHECK(pr_faint_history_contains(&r.bt,0,subject->id,&unknown)&&!unknown);
 // Win normally, then confirm the next encounter clears only per-battle logs.
 int wave=r.wave,arena_faints=r.bt.player_faints;
 CHECK((uint32_t)arena_faints==r.bt.faint_history_total[0]);
 for(int k=0;k<6;k++){bool seen=pr_faint_history_contains(&r.bt,0,r.party[k].b.id,&unknown);CHECK(!unknown&&seen==(k==0||r.party[k].b.hp<=0));}
 for(int k=0;k<6;k++){r.party[k].b.moves[0]=MOVE_ID_DAZZLING_GLEAM;r.party[k].b.pp_used[0]=0;}
 for(int k=2;k<4;k++)if(r.bt.field[k])r.bt.field[k]->hp=1;
 for(int step=0;step<80&&r.wave==wave;step++){
  gc_choice c={0};switch(r.screen.kind){case GC_SCREEN_COMMAND:c=(gc_choice){GC_CH_FIGHT,0,-1,0};break;case GC_SCREEN_TARGET:c=(gc_choice){GC_CH_TARGET,2,0,0};break;case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;case GC_SCREEN_LEARN_MOVE:c=(gc_choice){GC_CH_FORGET,-1,0,0};break;case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;break;case GC_SCREEN_CHECK_SWITCH:c.kind=GC_CH_CONFIRM;break;default:fprintf(stderr,"faint nextwave screen%d\n",r.screen.kind);CHECK(false);}choose(&r,c);
 }
 for(int step=0;step<20&&r.screen.kind==GC_SCREEN_CHECK_SWITCH;step++)choose(&r,(gc_choice){GC_CH_CONFIRM,0,0,0});
 CHECK(r.wave==wave+1&&!r.bt.n_faint_history[0]&&!r.bt.n_faint_history[1]&&!r.bt.faint_history_total[0]&&!r.bt.faint_history_total[1]&&!r.bt.n_fainted_ids[0]&&r.bt.player_faints==arena_faints);
 CHECK(gc_run_can_save(&r));size_t n=gc_run_save(&r,save,sizeof save);CHECK(n&&gc_run_load(&back,save,n,&why)&&!back.load_dropped&&back.bt.player_faints==arena_faints&&!back.bt.n_faint_history[0]);
 printf("Faint history: 13 real sacrificial/revival command cycles, next encounter and save; %d checks passed\n",checks);return 0;}
