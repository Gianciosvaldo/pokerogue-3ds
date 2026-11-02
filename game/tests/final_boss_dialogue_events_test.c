/* Real FaintPhase events -> the real app converter, including split batches and
 * a field recipient replaced before conversion. Pinned English dialogue fixture. */
#include "battle_events.h"
#include "battle_setup.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static pr_battle bt;static pr_engine engine;static pr_pokemon mons[2];
static char locale[4096],name_buffer[64];static const char *language="en";
const char *i18n_language(void){return language;}
const char *i18n_species_name(int species){snprintf(name_buffer,sizeof name_buffer,"%s",species==SPECIES_ID_ETERNATUS?"Eternatus":"Replacement");return name_buffer;}
char *pak_read(const char *key,size_t *n){if(strcmp(key,"locales/en/dialogue-final-boss.json")){*n=0;return NULL;}*n=strlen(locale);char *p=malloc(*n+1);assert(p);memcpy(p,locale,*n+1);return p;}
static void setup(unsigned mode,unsigned wave,unsigned side,unsigned form,unsigned hp)
{
 memset(&bt,0,sizeof bt);memset(&engine,0,sizeof engine);memset(mons,0,sizeof mons);bt.engine=&engine;bt.game_mode=mode;bt.wave_index=wave;bt.ignoring_effect_source=-1;bt.rng_live=bt.battle_rng_live=true;engine.n_levels=1;engine.current=-1;
 for(unsigned i=0;i<2;++i){pr_pokemon *p=&mons[i];p->id=101+i;p->species=SPECIES_ID_ETERNATUS;p->hp=100;p->level=30;p->on_field=true;p->is_player=!i;p->ability=p->base_ability=ABILITY_ID_BALL_FETCH;p->ability_override=p->custom_ability=p->custom_passive=-1;for(int k=0;k<6;++k)p->stats[k]=100;bt.field[i?2:0]=p;bt.party[i][0]=p;bt.n_party[i]=1;}
 mons[side].hp=hp;mons[side].form=form;
}
static be_view before(void){be_view v={0};v.mon[0]=(bv_mon){.species=SPECIES_ID_ETERNATUS,.hp=100,.max_hp=100};v.mon[1]=(bv_mon){.species=SPECIES_ID_ETERNATUS,.form=1,.hp=0,.max_hp=100};v.present[0]=v.present[1]=true;return v;}
static void paused_before_faint(void)
{
 for(unsigned mode=0;mode<5;++mode)for(unsigned wave=199;wave<=201;++wave)for(unsigned form=0;form<2;++form){
  setup(mode,wave,1,form,0);be_view view=before();view.mon[1].form=form;view.mon[1].hp=1;bv_event out[32];
  /* A switch prompt interrupted the turn after fatal damage but before its
   * FaintPhase. Catch-up can drain HP; the final boss must not disappear yet. */
  int n=be_convert_events(&bt,NULL,0,true,&view,out,32);unsigned faints=0;for(int k=0;k<n;++k)faints+=out[k].type==BEV_FAINT;
  bool final=(mode==GAME_MODES_CLASSIC||mode==GAME_MODES_CHALLENGE)&&wave==200;
  assert(faints==(unsigned)!final);assert(view.present[1]==final);
  if(!final)continue;
  int ph=pr_phase_new(&engine,PH_FAINT,2);engine.current=ph;engine.pool[ph].prevent_instant_revive=true;engine.pool[ph].source=-1;pr_phase_run(&bt,&engine,ph);
  n=be_convert_events(&bt,engine.events,engine.n_events,true,&view,out,32);int talk=-1,faint=-1;
  for(int k=0;k<n;++k){if(out[k].type==BEV_DIALOGUE)talk=k;if(out[k].type==BEV_FAINT)faint=k;}
  if(form){assert(talk>=0&&faint>talk&&!view.present[1]);}
  else{assert(talk<0&&faint<0&&view.present[1]&&mons[1].hp==1);}
 }
}
static void played_turns(void)
{
 static bs_battle match;const int attack[]={MOVE_ID_TACKLE},idle[]={MOVE_ID_SPLASH};unsigned finishes=0;
 for(unsigned seed=0;seed<24;++seed){
  char label[48];snprintf(label,sizeof label,"final-boss-dialogue-%u",seed);bs_begin(&match,BIOME_ID_END,200,label);
  pr_pokemon *player=bs_add(&match,0,SPECIES_ID_MEWTWO,100,attack,1),*boss=bs_add(&match,1,SPECIES_ID_ETERNATUS,100,idle,1);
  assert(player&&boss);player->ability=player->base_ability=boss->ability=boss->base_ability=ABILITY_ID_BALL_FETCH;
  boss->form=1;pr_recalculate_stats(&match.bt,boss);boss->hp=1;
  be_view view={0};view.present[0]=view.present[1]=true;be_mon(&match.bt,player,&view.mon[0]);be_mon(&match.bt,boss,&view.mon[1]);
  for(unsigned turn=0;turn<8&&!bs_outcome(&match);++turn){
   bs_turn_fight(&match,0);assert(!match.bt.n_unported&&!match.bt.n_deferred&&!match.eng.overflow);
   bv_event out[PR_MAX_EVENTS*2];int n=be_convert_events(&match.bt,match.eng.events,match.eng.n_events,true,&view,out,PR_MAX_EVENTS*2),talk=-1,faint=-1;
   for(int k=0;k<n;++k){if(out[k].type==BEV_DIALOGUE){assert(talk<0);talk=k;assert(!strcmp(out[k].text,"…Magnificent.")&&!strcmp(out[k].name,"Eternatus"));}if(out[k].type==BEV_FAINT&&out[k].battler==2){assert(talk>=0);faint=k;}}
   if(bs_outcome(&match)){assert(talk>=0&&faint>talk&&match.eng.stop==PH_VICTORY);finishes++;}
  }
  assert(bs_outcome(&match)==1);
 }
 assert(finishes==24);
}
int main(void)
{
 FILE *f=fopen("../../battle-core/tests/fixtures/final-boss-dialogue.en.json","rb");assert(f);size_t size=fread(locale,1,sizeof locale-1,f);assert(feof(f));fclose(f);locale[size]=0;pr_closures_init();
 unsigned dialogues=0,controls=0;
 for(unsigned mode=0;mode<5;++mode)for(unsigned wave=199;wave<=201;++wave)for(unsigned side=0;side<2;++side)for(unsigned form=0;form<2;++form)for(unsigned hp=0;hp<2;++hp){
  setup(mode,wave,side,form,hp);int ph=pr_phase_new(&engine,PH_FAINT,side?2:0);engine.current=ph;engine.pool[ph].prevent_instant_revive=true;engine.pool[ph].source=-1;pr_phase_run(&bt,&engine,ph);
  unsigned talks=0;int dialogue=-1,faint=-1;for(int k=0;k<engine.n_events;++k){if(engine.events[k].type==PR_EV_DIALOGUE){talks++;dialogue=k;}if(engine.events[k].type==PR_EV_FAINT)faint=k;}
  bool expected=(mode==GAME_MODES_CLASSIC||mode==GAME_MODES_CHALLENGE)&&wave==200&&side==1&&form==1&&!hp;
  assert(talks==(unsigned)expected);
  if(!expected){controls++;continue;}
  assert(dialogue>=0&&faint>dialogue);assert(engine.events[dialogue].field[2].visible && engine.events[dialogue].field[2].hp==0);
  for(unsigned fallback=0;fallback<2;++fallback){language=fallback?"fr":"en";be_view view=before();bv_event out[32];
   /* Live battle data already moved on; the event must still name Eternatus. */
   mons[1].species=SPECIES_ID_PIKACHU;mons[1].form=0;bt.field[2]=NULL;
   int n=be_convert_events(&bt,engine.events,engine.n_events,false,&view,out,32),talk_at=-1,faint_at=-1;
   for(int k=0;k<n;++k){if(out[k].type==BEV_DIALOGUE)talk_at=k;if(out[k].type==BEV_FAINT){assert(talk_at>=0);faint_at=k;}}
   assert(talk_at>=0&&faint_at>talk_at);assert(out[talk_at].battler==2&&out[talk_at].a==SPECIES_ID_ETERNATUS&&!out[talk_at].atlas);
   if(strcmp(out[talk_at].text,"…Magnificent."))fprintf(stderr,"dialogue text [%s], name [%s]\n",out[talk_at].text,out[talk_at].name);
   assert(out[talk_at].text&&out[talk_at].name);
   assert(!strcmp(out[talk_at].text,"…Magnificent.")&&!strcmp(out[talk_at].name,"Eternatus"));
   (void)i18n_species_name(SPECIES_ID_PIKACHU);assert(!strcmp(out[talk_at].name,"Eternatus"));
   /* An event-store boundary may separate the dialogue and faint. */
   view=before();n=be_convert_events(&bt,engine.events,dialogue+1,false,&view,out,32);unsigned seen=0;for(int k=0;k<n;++k){assert(out[k].type!=BEV_FAINT);seen+=out[k].type==BEV_DIALOGUE;}assert(seen==1&&view.present[1]);
   n=be_convert_events(&bt,engine.events+dialogue+1,engine.n_events-dialogue-1,false,&view,out,32);seen=0;for(int k=0;k<n;++k){assert(out[k].type!=BEV_DIALOGUE);seen+=out[k].type==BEV_FAINT;}assert(seen==1&&!view.present[1]);dialogues++;
  }
 }
 language="en";paused_before_faint();played_turns();
 assert(dialogues==4&&controls==118);printf("Final boss dialogue app: real phase/converter order, frozen speaker, English fallback split batches, paused-faint catch-up and 24 played Classic turns (%u dialogues, %u negative controls) passed\n",dialogues,controls);
}
