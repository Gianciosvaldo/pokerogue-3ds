/* The fixed Classic rival and the trainer dialogue events: wave 8 is Ivy/Finn with a talk before the fight and after the win, on a new
 * wave and on a loaded one. */
#include "../src/internal.h"
#include <stdio.h>
#include <stdlib.h>
static gc_run r, back;
static gc_system sys;
static unsigned char save[GC_SAVE_MAX];
static int checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"rival dialogue line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
/* what one stretch of play queued */
static gc_event evs[GC_MAX_EVENTS * 4];
static int n_evs;
static void drain(gc_run *run){gc_event e;while(gc_run_next_event(run,&e)&&n_evs<(int)(sizeof evs/sizeof *evs))evs[n_evs++]=e;}
static void setup(void){
  gc_system_init_defaults(&sys);
  gc_run_new(&r,GAME_MODES_CLASSIC,"rival-dialogue",31337);
  gc_starter st={.species=SPECIES_ID_BULBASAUR,.tera_type=-1};memset(st.ivs,31,6);
  const char *why=NULL;
  CHECK(gc_run_set_starters(&r,&st,1,&why));
  r.sys=&sys;
  n_evs=0;drain(&r);
}
/* play until the run reaches `wave` (its events queued) or `stop` says so; the lead is made unbeatable and the enemy harmless */
static void play_to(int wave,bool after_win){
  for(int step=0;step<4000;step++){
    if(!after_win&&r.wave>=wave&&gc_run_screen(&r)->kind==GC_SCREEN_COMMAND)return;
    const gc_screen *s=gc_run_screen(&r);gc_choice c={0};
    pr_pokemon *p=r.bt.field[0];
    if(p){p->hp=p->stats[0]=100000;p->stats[1]=p->stats[3]=100000;p->stat_stages[5]=6;}
    switch(s->kind){
    case GC_SCREEN_COMMAND:c=(gc_choice){GC_CH_FIGHT,0,-1,0};break;
    case GC_SCREEN_TARGET:c=(gc_choice){GC_CH_TARGET,s->target_ok[2]?2:3,0,0};break;
    case GC_SCREEN_REWARD:c.kind=GC_CH_SKIP;break;
    case GC_SCREEN_LEARN_MOVE:c=(gc_choice){GC_CH_FORGET,-1,0,0};break;
    case GC_SCREEN_EVOLVE:c.kind=GC_CH_CANCEL;break;
    case GC_SCREEN_CHECK_SWITCH:c.kind=GC_CH_CONFIRM;break;
    default:fprintf(stderr,"unexpected screen %d at wave %d\n",s->kind,r.wave);CHECK(false);
    }
    CHECK(gc_run_choose(&r,&c,NULL));
    if(after_win&&r.wave>wave)return;
    if(!after_win)n_evs=0;
    drain(&r);
  }
  CHECK(false);
}
static const gc_event *find(int kind,int from){for(int i=from;i<n_evs;i++)if(evs[i].kind==kind)return &evs[i];return NULL;}
static int index_of(int kind,int from){for(int i=from;i<n_evs;i++)if(evs[i].kind==kind)return i;return -1;}
int main(void){
  setup();
  play_to(8,false);
  CHECK(r.wave==8&&r.battle_type==BATTLE_TYPE_TRAINER&&r.trainer_type==TRAINER_TYPE_RIVAL);
  /* a male player meets the female variant: Ivy */
  CHECK(r.trainer_variant==TRAINER_VARIANT_FEMALE);
  const gc_trainer_config *c=gc_trainer_config_of(TRAINER_TYPE_RIVAL);
  CHECK(c&&c->has_char_sprite);
  /* wave -> trainer -> dialogue -> "would like to battle" */
  const int w=index_of(GC_EV_WAVE,0),t=index_of(GC_EV_TRAINER,0),d=index_of(GC_EV_DIALOGUE,0);
  CHECK(w>=0&&t>w&&d>t);
  CHECK(evs[d].a==TRAINER_TYPE_RIVAL&&evs[d].b==TRAINER_VARIANT_FEMALE&&evs[d].c==0);
  CHECK(evs[d].msg.key&&!strncmp(evs[d].msg.key,"dialogue:rival",14));
  const int m=index_of(GC_EV_MESSAGE,d);
  CHECK(m>d&&evs[m].msg.key&&!strcmp(evs[m].msg.key,"battle:trainerAppeared"));
  const char *enc_key=evs[d].msg.key;
  CHECK(gc_trainer_dialogue_key(&r,0)==enc_key);   /* seeded by the wave: the same line every time */
  CHECK(gc_trainer_dialogue_key(&r,1)&&strcmp(gc_trainer_dialogue_key(&r,1),enc_key));
  /* the save point: a loaded battle shows the line again */
  CHECK(gc_run_can_save(&r));
  const size_t n=gc_run_save(&r,save,sizeof save);
  CHECK(n&&gc_run_load(&back,save,n,NULL));
  n_evs=0;drain(&back);
  const gc_event *ld=find(GC_EV_DIALOGUE,0);
  CHECK(ld&&ld->c==0&&ld->msg.key&&!strcmp(ld->msg.key,enc_key));
  /* the win: "defeated", then the victory line */
  n_evs=0;
  play_to(8,true);
  int defeated=-1;
  for(int i=0;i<n_evs;i++)if(evs[i].kind==GC_EV_MESSAGE&&evs[i].msg.key&&!strcmp(evs[i].msg.key,"battle:trainerDefeated")){defeated=i;break;}
  CHECK(defeated>=0);
  CHECK(evs[defeated+1].kind==GC_EV_DIALOGUE&&evs[defeated+1].c==1&&evs[defeated+1].a==TRAINER_TYPE_RIVAL);
  CHECK(evs[defeated+1].msg.key&&!strncmp(evs[defeated+1].msg.key,"dialogue:rival",14));
  printf("rival dialogue: wave 8 is the female rival with a talk before and after, also on a loaded battle; %d checks passed\n",checks);
  return 0;
}
