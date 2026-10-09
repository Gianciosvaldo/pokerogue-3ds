/* Diagnostic policy only: decisions never mutate the game or use its RNG. */
#ifndef GC_ENDGAME_DIAGNOSTIC_H
#define GC_ENDGAME_DIAGNOSTIC_H
#include "gc/run.h"
#include "pr/ai.h"
#include <string.h>
static const pr_form *diag_form(const pr_pokemon *p){for(int i=0;i<PR_SPECIES_COUNT;i++)if(pr_species_table[i].id==p->species){const pr_species *s=&pr_species_table[i];return &pr_forms[s->form0+(p->form<s->nforms?p->form:0)];}return &pr_forms[0];}
static double diag_move_score(const pr_pokemon *p,const pr_pokemon *foe,int id){
 if(id<=0||id>=PR_MOVE_COUNT)return 0;
 const pr_move *m=&pr_moves[id];if(m->power<=0)return 0;
 const pr_form *a=diag_form(p),*b=diag_form(foe);double score=m->power;
 score*=m->category==MOVE_CATEGORY_PHYSICAL?(double)p->stats[1]/(foe->stats[2]?foe->stats[2]:1):(double)p->stats[3]/(foe->stats[4]?foe->stats[4]:1);
 if(m->type==a->type1||m->type==a->type2)score*=1.5;
 if(m->type>=0&&m->type<PR_TYPE_COUNT){if(b->type1>=0)score*=pr_type_chart_x2[m->type][b->type1]/2.0;if(b->type2>=0)score*=pr_type_chart_x2[m->type][b->type2]/2.0;}
 return score;
}
static int diag_hurt(const gc_run *run,bool fainted){int best=-1;double hp=2;for(int i=0;i<run->n_party;i++){const pr_pokemon *p=&run->party[i].b;if((p->hp<=0)!=fainted)continue;double f=p->stats[0]?(double)p->hp/p->stats[0]:1;if(f<hp){best=i;hp=f;}}return fainted||hp<0.75?best:-1;}
static int diag_low_pp(const gc_run *run,int *slot){int best=-1,lowest=999;for(int i=0;i<run->n_party;i++)if(run->party[i].b.hp>0){const pr_pokemon *p=&run->party[i].b;for(int j=0;j<p->n_moves;j++){int left=gc_move_max_pp(p,j)-p->pp_used[j];if(left<lowest){lowest=left;best=i;*slot=j;}}}return lowest<=3?best:-1;}
static bool diag_offer(const gc_run *run,const gc_offer *o,gc_choice *c){
 int faint=diag_hurt(run,true),hurt=diag_hurt(run,false),slot=0,pp=diag_low_pp(run,&slot);const char *k=o->key;if(!k)return false;
 c->b=-1;c->c=-1;
 if(strstr(k,"REVIVE")){if(faint<0)return false;c->b=faint;return true;}
 if(strstr(k,"POTION")||strstr(k,"FULL_RESTORE")){if(hurt<0)return false;c->b=hurt;return true;}
 if(strstr(k,"ETHER")||strstr(k,"ELIXIR")){if(pp<0)return false;c->b=pp;c->c=slot;return true;}
 return false;
}
static bool diag_choice(const gc_run *run,gc_choice *c){
 const gc_screen *s=gc_run_screen(run);memset(c,0,sizeof *c);c->b=c->c=-1;
 if(run->n_party<1||run->n_party>6||s->n_options<0||s->n_options>GC_MAX_OPTIONS||s->n_shop<0||s->n_shop>16)return false;
 for(int i=0;i<run->n_party;i++)if(run->party[i].b.n_moves>4)return false;
 switch(s->kind){
 case GC_SCREEN_COMMAND:{
  const gc_mon *me=gc_field_mon(run,0),*foe=gc_field_mon(run,1);if(!me||!foe)return false;
  const pr_pokemon *p=&me->b;double best=-1;int move=-1;
  for(int j=0;j<4;j++)if(s->move_ok[j]){double score=diag_move_score(p,&foe->b,p->moves[j]);if(score>best){best=score;move=j;}}
  if(s->can_switch&&(best<=0||p->hp*5<p->stats[0])){int pick=-1;double score=0;for(int i=0;i<run->n_party;i++){const pr_pokemon *q=&run->party[i].b;if(q->on_field||q->hp<=0)continue;for(int j=0;j<q->n_moves;j++)if(q->pp_used[j]<gc_move_max_pp(q,j)){double value=diag_move_score(q,&foe->b,q->moves[j])*(double)q->hp/(q->stats[0]?q->stats[0]:1);if(value>score){score=value;pick=i;}}}if(pick>=0){c->kind=GC_CH_POKEMON;c->a=pick;return true;}}
  if(s->can_ball&&best<=0){for(int i=POKEBALL_TYPE_MASTER_BALL;i>=0;i--)if(run->pokeballs[i]){c->kind=GC_CH_BALL;c->a=i;return true;}}
  for(int j=0;j<4;j++)if(s->move_ok[j]&&p->hp*2<p->stats[0]&&(p->moves[j]==MOVE_ID_RECOVER||p->moves[j]==MOVE_ID_SOFT_BOILED)){c->kind=GC_CH_FIGHT;c->a=j;return true;}
  if(move<0&&!s->struggle)return false;
  c->kind=GC_CH_FIGHT;c->a=move<0?0:move;return true;
 }
 case GC_SCREEN_SWITCH:{c->kind=GC_CH_PICK;if(s->can_skip){c->a=-1;return true;}int pick=-1,hp=-1;for(int i=0;i<run->n_party;i++)if(s->pick_ok[i]&&run->party[i].b.hp>hp){pick=i;hp=run->party[i].b.hp;}c->a=pick;return pick>=0;}
 case GC_SCREEN_CHECK_SWITCH:c->kind=GC_CH_CONFIRM;c->a=0;return true;
 case GC_SCREEN_LEARN_MOVE:c->kind=GC_CH_FORGET;c->a=-1;return true; /* keep the documented fixture moves */
 case GC_SCREEN_EVOLVE:c->kind=GC_CH_CANCEL;c->a=1;return true;
 case GC_SCREEN_REWARD:
  c->kind=GC_CH_REWARD;
  for(int i=0;i<s->n_options;i++)if(diag_offer(run,&s->options[i],c)){c->a=i;return true;}
  c->kind=GC_CH_SHOP;
  for(int i=0;i<s->n_shop;i++)if(s->shop[i].cost<=run->money&&diag_offer(run,&s->shop[i],c)){c->a=i;return true;}
  c->kind=GC_CH_REWARD;for(int i=0;i<s->n_options;i++)if(s->options[i].key&&s->options[i].target==GC_TARGET_NONE){c->a=i;c->b=c->c=-1;return true;}
  c->kind=GC_CH_SKIP;c->b=c->c=-1;return true;
 case GC_SCREEN_BIOME:c->kind=GC_CH_BIOME;c->a=0;return s->n_biomes>0;
 case GC_SCREEN_MYSTERY:c->kind=GC_CH_MYSTERY;for(int i=0;i<s->n_options;i++)if(s->option_ok[i]){c->a=i;c->b=0;c->c=0;return true;}return false;
 default:return false;
 }
}
#endif
