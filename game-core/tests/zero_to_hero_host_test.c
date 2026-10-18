/* Actual switch-out Hero form, the once-per-battleData message, a legal save and the new-biome base-form reset. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static gc_run run,loaded;static unsigned char saved[GC_SAVE_MAX];static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"Zero to Hero host line%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void settle(gc_run *r){gc_event ev;while(gc_run_next_event(r,&ev)){}}
static int hero_messages(const gc_run *r)
{
 int n=0;
 for(int i=0;i<r->n_battle_events;i++){const pr_event *e=&r->battle_events[i];if(e->type==PR_EV_TEXT&&e->msg.key&&!strcmp(e->msg.key,"abilityTriggers:postSummonZeroToHero"))n++;}
 return n;
}
static void choose(gc_run *r,uint8_t kind,int a){const char *why=NULL;gc_choice c={kind,(int16_t)a,-1,0};bool ok=gc_run_choose(r,&c,&why);if(!ok)fprintf(stderr,"choice %d/%d refused on screen %d: %s\n",kind,a,r->screen.kind,why?why:"?");CHECK(ok);settle(r);}
int main(void){
 const char *why=NULL;gc_run_new(&run,GAME_MODES_CLASSIC,"zero-to-hero",777);
 gc_starter s[2]={{.species=SPECIES_ID_BULBASAUR,.tera_type=-1,.moves={MOVE_ID_TACKLE}},{.species=SPECIES_ID_SQUIRTLE,.tera_type=-1,.moves={MOVE_ID_SPLASH}}};
 memset(s[0].ivs,15,sizeof s[0].ivs);memset(s[1].ivs,15,sizeof s[1].ivs);
 CHECK(gc_run_set_starters(&run,s,2,&why));settle(&run);
 /* the lead becomes a natural Palafin (Zero form, Zero to Hero in every slot) */
 gc_mon *m=&run.party[run.party_order[0]];gc_starter z={.species=SPECIES_ID_PALAFIN,.tera_type=-1,.moves={MOVE_ID_TACKLE}};memset(z.ivs,15,sizeof z.ivs);
 gc_mon_new_player(&run,m,&z,50);m->b.on_field=true;run.bt.field[0]=&m->b;run.bt.party[0][0]=&m->b;
 pr_pokemon *p=&m->b,*enemy=run.bt.field[2];CHECK(enemy&&p->form==0&&p->ability==ABILITY_ID_ZERO_TO_HERO);
 enemy->moves[0]=MOVE_ID_SPLASH;enemy->n_moves=1;enemy->custom_ability=ABILITY_ID_BALL_FETCH;enemy->ability_override=-1;pr_refresh_ability(enemy);
 int32_t zero_atk=p->stats[STAT_ATK];const int me=(int)(m-run.party),other=run.party_order[1];CHECK(other!=me);
 /* switching out turns it into the Hero form; the message waits for its next entry */
 choose(&run,GC_CH_POKEMON,other);
 CHECK(p->form==1&&p->stats[STAT_ATK]>zero_atk&&run.bt.field[0]!=p&&hero_messages(&run)==0&&!p->zero_to_hero_shown);
 choose(&run,GC_CH_POKEMON,me);
 CHECK(run.bt.field[0]==p&&p->form==1&&hero_messages(&run)==1&&p->zero_to_hero_shown);
 /* the same battleData: a second entry shows nothing, and leaving again keeps Hero */
 choose(&run,GC_CH_POKEMON,other);CHECK(p->form==1&&hero_messages(&run)==0);
 choose(&run,GC_CH_POKEMON,me);CHECK(run.bt.field[0]==p&&p->form==1&&hero_messages(&run)==0);
 /* win the wave and reach the next legal save point; battleData lives on, so the mark stays */
 enemy->hp=1;choose(&run,GC_CH_FIGHT,0);
 for(int step=0;step<20&&!gc_run_can_save(&run);step++){
  gc_choice pick={0};
  if(run.screen.kind==GC_SCREEN_REWARD)pick.kind=GC_CH_SKIP;
  else if(run.screen.kind==GC_SCREEN_LEARN_MOVE){pick.kind=GC_CH_FORGET;pick.a=-1;}
  else if(run.screen.kind==GC_SCREEN_EVOLVE){pick.kind=GC_CH_CANCEL;pick.a=1;}
  else if(run.screen.kind==GC_SCREEN_BIOME){pick.kind=GC_CH_BIOME;pick.a=0;}
  else{fprintf(stderr,"unexpected battle continuation screen%d\n",run.screen.kind);return 1;}
  CHECK(gc_run_choose(&run,&pick,&why));settle(&run);
 }
 CHECK(gc_run_can_save(&run)&&run.wave==2&&p->form==1&&p->zero_to_hero_shown);
 /* a loaded Pokemon gets a new battleData object upstream: the form is saved, the mark is not */
 size_t n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);CHECK(gc_run_load(&loaded,saved,n,&why));
 pr_pokemon *q=NULL;for(int i=0;i<loaded.n_party;i++)if(loaded.party[i].b.species==SPECIES_ID_PALAFIN)q=&loaded.party[i].b;
 CHECK(q&&q->form==1&&q->stats[STAT_ATK]==p->stats[STAT_ATK]&&!q->zero_to_hero_shown);
 /* the next biome's arena reset: PostBattleInit queues and runs the Zero form change, battleData resets */
 run.wave=10;run.n_battle_events=0;gc_new_battle(&run);
 CHECK(run.wave==11&&p->form==0&&p->stats[STAT_ATK]==zero_atk&&!p->zero_to_hero_shown&&p->ability==ABILITY_ID_ZERO_TO_HERO);
 settle(&run);CHECK(gc_run_can_save(&run));n=gc_run_save(&run,saved,sizeof saved);CHECK(n>0);
 CHECK(gc_run_load(&loaded,saved,n,&why));q=NULL;for(int i=0;i<loaded.n_party;i++)if(loaded.party[i].b.species==SPECIES_ID_PALAFIN)q=&loaded.party[i].b;
 CHECK(q&&q->form==0);
 printf("Zero to Hero switch/message/save/new-biome: %d checks passed\n",checks);return 0;
}
