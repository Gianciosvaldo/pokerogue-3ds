/* Actual doubles commands and optional history-summary save continuity. */
#define main retained_doubles_main
#include "doubles_host_test.c"
#undef main
#include "../../battle-core/tests/last-resort-history-fixture.inc"
static unsigned char changed[GC_SAVE_MAX];
static uint32_t lr_hash(const char *s){uint32_t h=2166136261u;while(*s){h^=(unsigned char)*s++;h*=16777619u;}return h;}
static uint32_t lr_u32(const unsigned char *s){uint32_t n;memcpy(&n,s,4);return n;}
static uint16_t lr_u16(const unsigned char *s){uint16_t n;memcpy(&n,s,2);return n;}
static void lr_crc(size_t length){uint32_t crc=~0u;for(size_t i=20;i<length;i++){crc^=changed[i];for(int j=0;j<8;j++)crc=crc>>1^((0u-(crc&1))&0xedb88320u);}crc=~crc;memcpy(changed+16,&crc,4);}
static size_t lr_section(size_t length){size_t p=20;while(p+8<=length&&lr_u32(changed+p)!=lr_hash("party_move_uses"))p+=8+lr_u32(changed+p+4);CHECK(p+8<=length);return p;}
static void lr_schema(size_t length,const pr_pokemon *lead){
 const char *why=NULL;memcpy(changed,save,length);size_t sec=lr_section(length),block=sec+8;
 CHECK(lr_u16(changed+block)==2);size_t data=block+2+12+2;
 CHECK(data+sizeof lead->history_used_moves<length);
 changed[data+sizeof lead->history_used_moves]=2;lr_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);changed[data+sizeof lead->history_used_moves-1]|=0x80;lr_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);uint16_t bad=2;memcpy(changed+block+2+6+4,&bad,2);lr_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);uint16_t wrong_count=0;memcpy(changed+block+2+12,&wrong_count,2);lr_crc(length);CHECK(!gc_run_load(&back,changed,length,&why));
 memcpy(changed,save,length);size_t duplicate=8+lr_u32(changed+sec+4);CHECK(length+duplicate<=sizeof changed);
 memcpy(changed+length,changed+sec,duplicate);uint32_t longer=(uint32_t)(length+duplicate-20);memcpy(changed+12,&longer,4);lr_crc(length+duplicate);CHECK(!gc_run_load(&back,changed,length+duplicate,&why));
 // Removing the optional section models a legacy save without losing unrelated data.
 memcpy(changed,save,length);size_t bytes=8+lr_u32(changed+sec+4);memmove(changed+sec,changed+sec+bytes,length-sec-bytes);length-=bytes;
 uint32_t body=(uint32_t)(length-20);memcpy(changed+12,&body,4);lr_crc(length);
 CHECK(gc_run_load(&back,changed,length,&why)&&!back.load_dropped);
 const pr_pokemon *p=back.bt.field[0];CHECK(p->history_used_complete==(p->n_move_history<=PR_MAX_MOVE_HISTORY));
 if(p->n_move_history>PR_MAX_MOVE_HISTORY){back.bt.n_unported=0;(void)pr_move_apply_conditions(&back.bt,MOVE_ID_LAST_RESORT,p,back.bt.field[2],3);CHECK(back.bt.n_unported);
  pr_push_move_history(back.bt.field[0],(pr_turn_move){.move=MOVE_ID_TACKLE,.use_mode=MOVE_USE_MODE_NORMAL});back.bt.n_unported=0;
  CHECK(!back.bt.field[0]->history_used_complete&&pr_move_apply_conditions(&back.bt,MOVE_ID_LAST_RESORT,back.bt.field[0],back.bt.field[2],3)&&!back.bt.n_unported);
  size_t partial=gc_run_save(&back,changed,sizeof changed);
  CHECK(partial&&gc_run_load(&back,changed,partial,&why)&&!back.load_dropped&&!back.bt.field[0]->history_used_complete);
  CHECK(pr_move_apply_conditions(&back.bt,MOVE_ID_LAST_RESORT,back.bt.field[0],back.bt.field[2],3)&&!back.bt.n_unported);
 }
}
static void lr_play(gc_run *run){int turn=run->bt.turn,n=0;while(run->bt.turn==turn&&n++<8){
 gc_choice choice=run->screen.kind==GC_SCREEN_COMMAND?(gc_choice){GC_CH_FIGHT,run->screen.slot?0:1,-1,0}:(gc_choice){GC_CH_TARGET,2,0,0};choose(run,choice);
 }CHECK(run->bt.turn==turn+1&&!run->bt.n_unported&&!run->eng.overflow);}
int main(int argc,char **argv){
 CHECK(argc==2);FILE *f=fopen(argv[1],"r");CHECK(f);lr_header(f);pr_closures_init();int owners=0,roundtrips=0;
 for(int i=0;i<432;i++){
  lr_case c;lr_read(f,i,&c);setup(1);r.n_items=r.n_enemy_items=0;gc_items_to_battle(&r);
  for(int k=0;k<4;k++){pr_pokemon *p=r.bt.field[k];p->custom_ability=ABILITY_ID_NONE;pr_refresh_ability(p);p->passive_flag=false;p->n_moves=1;p->moves[0]=MOVE_ID_SPLASH;p->hp=p->stats[STAT_HP]=100000;}
  pr_pokemon *p=r.bt.field[0];p->n_moves=c.user.n_moves;p->summon_moves_active=c.user.summon_moves_active;
  memcpy(p->moves,c.user.moves,sizeof p->moves);memcpy(p->transform_moves,c.user.transform_moves,sizeof p->transform_moves);
  p->n_move_history=c.user.n_move_history;memcpy(p->move_history,c.user.move_history,sizeof p->move_history);
  p->history_used_complete=c.user.history_used_complete;memcpy(p->history_used_moves,c.user.history_used_moves,sizeof p->history_used_moves);
  for(int k=0;k<4;k++)p->pp_used[k]=p->transform_pp_used[k]=p->pp_up[k]=p->transform_pp_up[k]=0,p->max_pp_override[k]=p->transform_max_pp_override[k]=-1;
  for(int stage=0;stage<5;stage++){
   if(stage==1)lr_changed(p);
   if(stage==2)p->moves[1]=c.future_move;
   if(stage==3){lr_changed(p);pr_reset_summon_data(p);}
   if(stage==4)pr_push_move_history(p,(pr_turn_move){.move=MOVE_ID_SPLASH,.use_mode=MOVE_USE_MODE_NORMAL,.result=PR_MOVE_RESULT_UNDEFINED,.n_targets=1,.targets={2}});
   pr_rng before=r.bt.rng,seed=r.bt.battle_rng;
   r.bt.n_unported=0;CHECK(pr_move_apply_conditions(&r.bt,MOVE_ID_LAST_RESORT,p,r.bt.field[2],3)==c.expected[stage]&&!r.bt.n_unported);
   CHECK(rngsame(before,r.bt.rng)&&rngsame(seed,r.bt.battle_rng));
   const char *why=NULL;size_t size=gc_run_save(&r,save,sizeof save);CHECK(size&&gc_run_load(&back,save,size,&why)&&!back.load_dropped);
   CHECK(!memcmp(back.bt.field[0]->history_used_moves,p->history_used_moves,sizeof p->history_used_moves)&&back.bt.field[0]->history_used_complete==p->history_used_complete);
   CHECK(pr_move_apply_conditions(&back.bt,MOVE_ID_LAST_RESORT,back.bt.field[0],back.bt.field[2],3)==c.expected[stage]&&!back.bt.n_unported);roundtrips++;
   if(stage==0&&c.shape==0&&c.mode==MOVE_USE_MODE_NORMAL&&(c.length==8||c.length==1024))lr_schema(size,p);
   if(stage==0&&c.shape==0&&c.length>=9){
    int hp=r.bt.field[2]->hp;lr_play(&r);lr_play(&back);CHECK((r.bt.field[2]->hp<hp)==c.expected[stage]);
    CHECK(r.bt.field[2]->hp==back.bt.field[2]->hp&&rngsame(r.bt.rng,back.bt.rng)&&rngsame(r.bt.battle_rng,back.bt.battle_rng));owners++;
    // The following condition/save stages need the original command save point.
    CHECK(gc_run_load(&r,save,size,&why));p=r.bt.field[0];drain(&r);
   }
  }
 }
 lr_footer(f);fclose(f);CHECK(roundtrips==2160&&owners==30);
 printf("Last Resort command/save: %d owner turns, %d native condition roundtrips, %d checks passed\n",owners,roundtrips,checks);return 0;
}
