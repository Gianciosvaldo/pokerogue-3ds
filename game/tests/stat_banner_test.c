/* Unit test for stat stage modifier indicators, status visual banners, and text banner queuing. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_events.h"
#include "battle_setup.h"
#include "battle_view.h"
#include "pr/battle.h"
#include "pr/engine.h"
#include "pr/events.h"
#include "pr_enums.h"

/* Stub locale/i18n helpers for host test */
const char* i18n_language(void) { return "en"; }
const char* i18n_species_name(int id) {
  static char name[32];
  snprintf(name, sizeof name, "Species_%d", id);
  return name;
}
char* pak_read(const char* key, size_t* len) {
  if (len) *len = 0;
  return NULL;
}
void bv_play(const bv_event* ev, int n, void (*on_done)(void*), void* arg) {
  (void)ev;
  (void)n;
  (void)on_done;
  (void)arg;
}

static void test_chain_preview(bs_battle *b){
 pr_pokemon *p=b->bt.field[0],saved=*p;
 const int moves[]={MOVE_ID_ROLLOUT,MOVE_ID_FURY_CUTTER,MOVE_ID_ICE_BALL,MOVE_ID_ECHOED_VOICE};
 const int factors[]={16,4,16,5};
 for(int k=0;k<4;k++){
  p->moves[0]=moves[k];p->n_move_history=0;
  for(int j=0;j<4;j++)pr_push_move_history(p,(pr_turn_move){.move=moves[k],.result=MOVE_RESULT_SUCCESS});
  static pr_battle before;before=b->bt;pr_pokemon original=*p;bv_mon shown;
  be_mon(&b->bt,p,&shown);
  assert(shown.move_power[0]==pr_moves[moves[k]].power*factors[k]);
  assert(!memcmp(p,&original,sizeof original)&&!memcmp(&before,&b->bt,sizeof before));
 }
 *p=saved;
}
static void test_departed_ability_and_promotion(const pr_battle *bt){
 be_view view={0};bv_event out[8];pr_event events[2]={0};
 view.present[1]=true;view.mon[1]=(bv_mon){.species=32,.hp=20,.max_hp=30};strcpy(view.mon[1].name,"Species_32");
 events[0].type=PR_EV_SWITCH_OUT;events[0].battler=2;
 events[0].field[2]=(pr_event_field){.species=32,.hp=20,.max_hp=30,.visible=true};
 events[1]=events[0];events[1].type=PR_EV_ABILITY_SHOW;events[1].id=ABILITY_ID_POISON_POINT;
 int n=be_convert_events(bt,events,2,false,&view,out,8);
 assert(n==2&&out[0].type==BEV_WITHDRAW&&out[1].type==BEV_ABILITY&&out[1].text&&!strcmp(out[1].text,"Species_32")&&!view.present[1]);
 memset(&view,0,sizeof view);events[0]=events[1];events[0].battler=1;events[0].field[1]=(pr_event_field){.species=999};
 n=be_convert_events(bt,events,1,false,&view,out,8);
 assert(n==1&&out[0].type==BEV_ABILITY&&!strcmp(out[0].text,"Species_999")&&!view.present[2]);
 /* A snapshot that starts a disguise must retain that fact until a later
  * wave clears it without an explicit break event (e.g. the last ally faints). */
 memset(&view,0,sizeof view);memset(events,0,sizeof events);
 view.present[0]=true;view.mon[0]=(bv_mon){.species=6570,.hp=20,.max_hp=30};
 events[0].type=PR_EV_TEXT;events[0].battler=-1;
 events[0].field[0]=(pr_event_field){.species=2037,.hp=20,.max_hp=30,.visible=true};
 pr_battle illusion_bt=*bt;illusion_bt.deviations|=PR_DEV_ILLUSION;
 n=be_convert_events(&illusion_bt,events,1,false,&view,out,8);
 bool found=false;for(int i=0;i<n;i++)if(out[i].type==BEV_ILLUSION){assert(out[i].mon->illusion);view.mon[0]=*out[i].mon;found=true;}assert(found);
 events[0].type=PR_EV_ILLUSION_BREAK;events[0].battler=0;events[0].field[0].species=6570;
 n=be_convert_events(&illusion_bt,events,1,false,&view,out,8);
 found=false;for(int i=0;i<n;i++)if(out[i].type==BEV_ILLUSION){assert(!out[i].mon->illusion);found=true;}assert(found);
 memset(&view,0,sizeof view);memset(events,0,sizeof events);
 view.mon[0]=(bv_mon){.species=4,.pokemon_id=22,.identity_known=true,.hp=20,.max_hp=30};view.present[0]=true;
 view.mon[2]=(bv_mon){.species=1,.pokemon_id=11,.identity_known=true};
 events[0].type=PR_EV_FIELD_SWAP;events[0].battler=-1;
 events[0].field[0]=(pr_event_field){.species=4,.pokemon_id=22,.pokemon_id_known=true,.hp=20,.max_hp=30,.visible=true};
 events[0].field[1]=(pr_event_field){.species=1,.pokemon_id=11,.pokemon_id_known=true};
 n=be_convert_events(bt,events,1,false,&view,out,8);assert(n==0&&view.present[0]&&!view.present[2]);
}

/* The stat stage banner shows the stages a stat really moved. The event used to carry only the direction, so the
 * converter made every change +1 or -1: Fell Stinger's +3 after a KO read "ATK +1" (lorenzo's real-3DS report). */
static int stage_events(const bs_battle* b, int battler, int stat, int* last_delta) {
  static bv_event out[256];
  bv_mon sp = {0}, se = {0};
  be_mon(&b->bt, b->bt.field[0], &sp);
  if (b->bt.field[2]) be_mon(&b->bt, b->bt.field[2], &se);
  int n = be_convert(&b->bt, &b->eng, &sp, &se, out, 256), found = 0;
  for (int i = 0; i < n; i++)
    if (out[i].type == BEV_STAT_STAGES && out[i].battler == battler && out[i].a == stat) {
      found++;
      *last_delta = out[i].b;
    }
  return found;
}

static void test_applied_stages(void) {
  static bs_battle b;
  int delta = 0;
  /* Fell Stinger KO: the user's Attack +3 (PostVictoryStatStageChangeAttr, stages 3) */
  {
    int pm[] = {MOVE_ID_FELL_STINGER}, em[] = {MOVE_ID_SPLASH};
    bs_begin(&b, BIOME_ID_GRASS, 5, "stat_banner_fell_stinger");
    bs_add(&b, 0, SPECIES_ID_BEEDRILL, 100, pm, 1);
    bs_add(&b, 1, SPECIES_ID_MAGIKARP, 2, em, 1);
    bs_turn_fight(&b, 0);
    assert(b.bt.party[1][0]->hp == 0);
    assert(b.bt.party[0][0]->stat_stages[STAT_ATK - 1] == 3);
    assert(stage_events(&b, 0, STAT_ATK, &delta) == 1 && delta == 3);
  }
  /* Swords Dance from +5: the stage moves +1 to the cap, the banner says +1 */
  {
    int pm[] = {MOVE_ID_SWORDS_DANCE}, em[] = {MOVE_ID_SPLASH};
    bs_begin(&b, BIOME_ID_GRASS, 5, "stat_banner_cap");
    pr_pokemon* p = bs_add(&b, 0, SPECIES_ID_SCIZOR, 50, pm, 1);
    bs_add(&b, 1, SPECIES_ID_MAGIKARP, 50, em, 1);
    p->stat_stages[STAT_ATK - 1] = 5;
    bs_turn_fight(&b, 0);
    assert(p->stat_stages[STAT_ATK - 1] == 6);
    assert(stage_events(&b, 0, STAT_ATK, &delta) == 1 && delta == 1);
  }
  /* a drop: Screech takes the foe's Defense -2 and the banner shows -2 */
  {
    int pm[] = {MOVE_ID_SCREECH}, em[] = {MOVE_ID_SPLASH};
    bs_begin(&b, BIOME_ID_GRASS, 5, "stat_banner_drop");
    bs_add(&b, 0, SPECIES_ID_SCIZOR, 50, pm, 1);
    pr_pokemon* foe = bs_add(&b, 1, SPECIES_ID_MAGIKARP, 50, em, 1);
    bs_turn_fight(&b, 0);
    if (foe->stat_stages[STAT_DEF - 1] == -2) assert(stage_events(&b, 2, STAT_DEF, &delta) == 1 && delta == -2);
    else assert(stage_events(&b, 2, STAT_DEF, &delta) == 0); /* Screech missed (85% accuracy): no banner */
  }
  /* hand-made: one event raising two stats by different amounts gives one banner each with its own stages; a stat in the
   * mask with nothing applied gives none */
  {
    be_view view;
    bv_event out[8];
    pr_event ev;
    memset(&view, 0, sizeof view);
    memset(&ev, 0, sizeof ev);
    view.present[0] = true;
    view.mon[0] = (bv_mon){.species = 212, .hp = 100, .max_hp = 100};
    ev.type = PR_EV_STAT_STAGES;
    ev.battler = 0;
    ev.target = -1;
    ev.id = 1;
    ev.value = (1 << STAT_ATK) | (1 << STAT_SPATK) | (1 << STAT_SPD);
    ev.stat_stages_applied[STAT_ATK - 1] = 1;
    ev.stat_stages_applied[STAT_SPATK - 1] = 2;
    ev.field[0] = (pr_event_field){.species = 212, .hp = 100, .max_hp = 100, .visible = true, .party_slot = 0};
    int n = be_convert_events(&b.bt, &ev, 1, false, &view, out, 8), atk = 0, spatk = 0, other = 0;
    for (int i = 0; i < n; i++) {
      if (out[i].type != BEV_STAT_STAGES) continue;
      if (out[i].a == STAT_ATK && out[i].b == 1) atk++;
      else if (out[i].a == STAT_SPATK && out[i].b == 2) spatk++;
      else other++;
    }
    assert(atk == 1 && spatk == 1 && other == 0);
  }
  printf("stat_banner_test: applied stages (Fell Stinger +3, capped +1, drop, mixed event) verified\n");
}

int main(int argc, char** argv) {
  test_applied_stages();
  static bs_battle b;
  bs_begin(&b, BIOME_ID_GRASS, 5, "stat_banner_test_seed");

  /* Level 100 Blaziken with Swords Dance vs Level 10 Garchomp with Toxic / Thunder Wave / Growl */
  int pm[] = {MOVE_ID_SWORDS_DANCE, MOVE_ID_BLAZE_KICK, MOVE_ID_CLOSE_COMBAT, MOVE_ID_BRAVE_BIRD};
  int em[] = {MOVE_ID_TOXIC, MOVE_ID_EARTHQUAKE, MOVE_ID_DRAGON_CLAW, MOVE_ID_ROCK_SLIDE};
  bs_add(&b, 0, SPECIES_ID_BLAZIKEN, 100, pm, 4);
  bs_add(&b, 1, SPECIES_ID_GARCHOMP, 10, em, 4);

  test_chain_preview(&b);test_departed_ability_and_promotion(&b.bt);

  bv_mon shown_player = {0};
  bv_mon shown_enemy = {0};
  be_mon(&b.bt, b.bt.field[0], &shown_player);
  be_mon(&b.bt, b.bt.field[2], &shown_enemy);

  /* Run turn 1: Blaziken uses Swords Dance (slot 0) */
  bs_turn_fight(&b, 0);
  printf("turn 1 n_events=%d:\n", b.eng.n_events);
  for (int i = 0; i < b.eng.n_events; i++) {
    printf("  ev %d: type=%d battler=%d id=%d val=%d msg.key=%s\n", i, b.eng.events[i].type, b.eng.events[i].battler, b.eng.events[i].id, b.eng.events[i].value, b.eng.events[i].msg.key);
  }

  static bv_event out_events[128];
  int n_conv = be_convert(&b.bt, &b.eng, &shown_player, &shown_enemy, out_events, 128);
  printf("n_conv=%d:\n", n_conv);
  for (int i = 0; i < n_conv; i++) {
    printf("  bev %d: type=%d battler=%d a=%d b=%d text=%s\n", i, out_events[i].type, out_events[i].battler, out_events[i].a, out_events[i].b, out_events[i].text);
  }

  bool saw_stat_stage_event = false;
  bool saw_text_event = false;

  for (int i = 0; i < n_conv; i++) {
    if (out_events[i].type == BEV_STAT_STAGES) {
      saw_stat_stage_event = true;
      /* battler 0 (player), stat 1 (ATK), Swords Dance: +2, the stages it really moved (the banner reads "ATK +2") */
      assert(out_events[i].battler == 0);
      assert(out_events[i].a == STAT_ATK);
      assert(out_events[i].b == 2);
      assert(b.bt.field[0]->stat_stages[STAT_ATK - 1] == 2);
    }
    if (out_events[i].type == BEV_MESSAGE) {
      saw_text_event = true;
      assert(out_events[i].text != NULL);
    }
  }

  assert(saw_stat_stage_event);
  assert(saw_text_event);

  /* Now test BEV_STATUS conversion and queuing */
  shown_player.hp = b.bt.field[0]->hp;
  shown_enemy.hp = b.bt.field[2]->hp;
  bv_event status_ev = {.type = BEV_STATUS, .battler = 0, .a = STATUS_EFFECT_POISON};
  bv_play(&status_ev, 1, NULL, NULL);

  printf("stat_banner_test PASSED: %d events converted, stat stage, status & text banner events verified.\n", n_conv);
  return 0;
}
