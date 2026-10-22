/* The starter select screen on the UI stack (see starter_host.h). */
#include "starter_host.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "gc/gacha.h"

#include "bgm.h"
#include "i18n.h"
#include "starter_data.h"
#include "system_data.h"
#include "ui_widgets.h"
#include "pr_data.h"
const pr_species* menu_species(uint16_t id);
#include "ui.h"

static struct {
  gc_system* sys;
  sh_events ev;
  sm_entry entries[SS_MAX_ENTRIES];
  int n;
  uint32_t rng;
  uint16_t editing;
  int mode, slot, page, count;
  char labels[32][128];
  const char* options[32];
  int values[32];
} H;

/* ---- what the screen asks the game ---- */

static void describe(void* ctx, uint16_t species, starter_info* out) {
  (void)ctx;
  ss_describe(H.sys, species, out);
}

/* Math.floor(Math.random() * n) for Randomize */
static int rnd(void* ctx, int n) {
  (void)ctx;
  H.rng ^= H.rng << 13;
  H.rng ^= H.rng >> 17;
  H.rng ^= H.rng << 5;
  return n > 0 ? (int)(H.rng % (uint32_t)n) : 0;
}

/* The team was confirmed: game-core's rules decide whether it is a team. The screen only offers what the dex holds, so a refusal
 * means the dex and game-core disagree about a default (the message says what game-core said). */
static bool on_start(void* ctx, const uint16_t* species, int n) {
  (void)ctx;
  gc_starter team[6];
  const char* why = NULL;
  if (!ss_team(H.sys, species, n, team, &why)) {
    char text[200], base[96];
    i18n_t(base, sizeof base, "starterSelectUiHandler:invalidParty", NULL);
    snprintf(text, sizeof text, "%s (%s)", base, why ? why : "");
    starter_message(text);
    return false;
  }
  if (H.ev.on_start) H.ev.on_start(team, n, H.ev.arg);
  return true;
}

static void on_exit(void* ctx) {
  (void)ctx;
  if (H.ev.on_exit) H.ev.on_exit(H.ev.arg);
}

/* The options stay alive until ui_push_options pops and calls us back. */
enum { EDIT_TOP, EDIT_NATURE, EDIT_SHINY, EDIT_ABILITY, EDIT_SLOTS, EDIT_MOVES, EDIT_CANDY };
extern const char* const menu_nature_keys[];
static void editor_show(int mode);
static void editor_add(int value,const char* label) {
  if(H.count>=32)return;
  int i=H.count++;snprintf(H.labels[i],sizeof H.labels[i],"%s",label);
  H.options[i]=H.labels[i];H.values[i]=value;
}
static bool editor_save(void) {
  return H.ev.save_profile ? H.ev.save_profile(H.ev.arg) : sysd_save();
}
static void editor_refresh(void) {
  H.n=ss_list(H.sys,H.entries,SS_MAX_ENTRIES,NULL);
  starter_refresh();
}
static void editor_done(int row,void* arg) {
  (void)arg;
  int mode=H.mode;
  if(row<0 || row>=H.count) {if(mode!=EDIT_TOP)editor_show(EDIT_TOP);return;}
  int value=H.values[row];
  if(mode==EDIT_TOP) {
    if(value==-1)return;
    if(value>=0) {H.page=0;editor_show(value);return;}
  }
  if(value==-1) {editor_show(EDIT_TOP);return;}
  if(mode==EDIT_SLOTS) {H.slot=value;H.page=0;editor_show(EDIT_MOVES);return;}
  if(mode==EDIT_MOVES && value<0) {H.page+=value==-10?1:-1;editor_show(EDIT_MOVES);return;}
  /* Settle any pending profile transaction before taking the rollback snapshot. */
  if(!editor_save()) {starter_message("The profile could not be saved. Nothing was changed.");return;}
  gc_starter_entry* p=&H.sys->starters[H.editing];
  gc_starter_entry before=*p;
  int eggs_before=H.sys->n_eggs;
  gc_egg egg_before={0};
  if(eggs_before>=0 && eggs_before<GC_MAX_EGGS)egg_before=H.sys->eggs[eggs_before];
  uint32_t rng_before=H.rng;
  gc_starter selected;bool ok=ss_starter(H.sys,H.editing,&selected);
  if(mode==EDIT_NATURE) {selected.nature=value;p->nature_preference=value;p->preference_flags|=1;}
  else if(mode==EDIT_SHINY) {selected.shiny=value>0;selected.variant=value?value-1:0;p->appearance_preference=value;p->preference_flags|=2;}
  else if(mode==EDIT_ABILITY) {selected.ability_index=value;p->ability_preference=value;p->preference_flags|=4;}
  else if(mode==EDIT_MOVES) {
    int old=-1;for(int i=0;i<4;i++)if(selected.moves[i]==value)old=i;
    if(old>=0)selected.moves[old]=selected.moves[H.slot];
    selected.moves[H.slot]=value;
    memcpy(p->moveset,selected.moves,sizeof p->moveset);
  } else if(mode==EDIT_CANDY && value==2) {
    gc_gacha_ctx ctx={0};pr_seedstr seed;char key[80];
    snprintf(key,sizeof key,"species-egg-%u-%u-%u",rng_before,ui_frame(),(unsigned)rnd(NULL,0x7fffffff));
    pr_seed_from_ascii(&seed,key);pr_rng_sow(&ctx.entropy,&seed);
    time_t now=time(NULL);ctx.timestamp_ms=now>0?(uint64_t)now*1000:0;
    const char* why=NULL;ok=gc_system_buy_species_egg(H.sys,H.editing,&ctx,&why);
    if(!ok && why && !strcmp(why,"egg:tooManyEggs")) {
      H.rng=rng_before;starter_message("Your egg storage is full (99 eggs).");return;
    }
  } else if(mode==EDIT_CANDY) ok=gc_system_buy_starter_unlock(H.sys,H.editing,value==1);
  else if(mode==EDIT_TOP && value==-3) {p->passive_attr^=2;selected.passive=(p->passive_attr&2)!=0;}
  if(mode!=EDIT_CANDY) ok=ok && gc_starter_is_available(H.sys,&selected,NULL,NULL);
  if(!ok) {*p=before;H.rng=rng_before;ui_play_error();editor_show(mode);return;}
  if(!editor_save()) {
    *p=before;H.rng=rng_before;H.sys->n_eggs=eggs_before;
    if(eggs_before>=0 && eggs_before<GC_MAX_EGGS)H.sys->eggs[eggs_before]=egg_before;
    editor_refresh();starter_message("The profile could not be saved. Nothing was changed.");return;
  }
  ui_play_select();editor_refresh();
  editor_show(mode==EDIT_MOVES?EDIT_SLOTS:EDIT_TOP);
}
static void editor_show(int mode) {
  H.mode=mode;H.count=0;
  gc_starter selected;if(!ss_starter(H.sys,H.editing,&selected))return;
  const gc_starter_entry* p=&H.sys->starters[H.editing];
  char label[128];
  if(mode==EDIT_TOP) {
    editor_add(EDIT_SLOTS,"Moves");editor_add(EDIT_NATURE,"Nature");
    editor_add(EDIT_SHINY,"Shiny tier");editor_add(EDIT_ABILITY,"Ability");
    snprintf(label,sizeof label,"Use candies (%d)",p->candy_count);editor_add(EDIT_CANDY,label);
    if(p->passive_attr&1)editor_add(-3,(p->passive_attr&2)?"Disable passive":"Enable passive");
    editor_add(-1,"Back");
  } else if(mode==EDIT_NATURE) {
    for(int n=0;n<25;n++)if(H.sys->dex[H.editing].nature_attr&(1u<<(n+1))) {
      char key[64],name[96];snprintf(key,sizeof key,"nature:%s",menu_nature_keys[n]);i18n_t(name,sizeof name,key,NULL);
      snprintf(label,sizeof label,"%s%s",selected.nature==n?"* ":"",name);editor_add(n,label);
    }
  } else if(mode==EDIT_SHINY || mode==EDIT_ABILITY) {
    const char* looks[]={"Normal (Luck +0)","Shiny (Luck +1)","Rare shiny (Luck +2)","Epic shiny (Luck +3)"};
    const pr_species* species=menu_species(H.editing);const pr_form* form=species?&pr_forms[species->form0+selected.form]:NULL;
    for(int v=0;v<(mode==EDIT_SHINY?4:3);v++) {
      gc_starter candidate=selected;
      if(mode==EDIT_SHINY){candidate.shiny=v>0;candidate.variant=v?v-1:0;}
      else candidate.ability_index=v;
      if(!gc_starter_is_available(H.sys,&candidate,NULL,NULL))continue;
      if(mode==EDIT_SHINY)snprintf(label,sizeof label,"%s%s",(selected.shiny?selected.variant+1:0)==v?"* ":"",looks[v]);
      else {int ability=v==0?form->ability1:v==1?form->ability2:form->ability_hidden;
        snprintf(label,sizeof label,"%s%s%s",selected.ability_index==v?"* ":"",i18n_ability_name(ability),v==2?" (Hidden)":"");}
      editor_add(v,label);
    }
  } else if(mode==EDIT_SLOTS) {
    for(int i=0;i<4;i++)if(selected.moves[i]) {
      snprintf(label,sizeof label,"%d: %s",i+1,i18n_move_name(selected.moves[i]));editor_add(i,label);
    }
  } else if(mode==EDIT_MOVES) {
    uint16_t pool[64];int n=gc_starter_moves(H.sys,H.editing,selected.form,pool,64);
    for(int i=H.page*28;i<n && i<(H.page+1)*28;i++)editor_add(pool[i],i18n_move_name(pool[i]));
    if((H.page+1)*28<n)editor_add(-10,"Next page");
    if(H.page>0)editor_add(-11,"Previous page");
  } else if(mode==EDIT_CANDY) {
    int cost=gc_system_starter_unlock_cost(H.editing);
    if(p->passive_attr&1)snprintf(label,sizeof label,"Passive unlocked");
    else snprintf(label,sizeof label,"Unlock passive (%d)",cost);
    editor_add(0,label);
    const pr_species *species=menu_species(H.editing);
    if(species&&species->nforms&&pr_forms[species->form0].ability_hidden){
      if(p->ability_attr&4)snprintf(label,sizeof label,"Hidden ability unlocked");
      else snprintf(label,sizeof label,"Unlock hidden ability (%d)",cost);
      editor_add(1,label);
    }
    snprintf(label,sizeof label,"Buy species egg (%d)",gc_system_species_egg_cost(H.sys,H.editing));editor_add(2,label);
  }
  if(!H.count)editor_add(-1,"No unlocked choices");
  ui_push_options(H.options,H.count,true,editor_done,NULL);
}
static void configure(void* ctx,uint16_t species) {
  (void)ctx;H.editing=species;H.page=0;editor_show(EDIT_TOP);
}

/* ---- the screen ---- */

static bool sh_input(ui_screen* s, ui_button b) {
  (void)s;
  return starter_input((mui_button)b); /* both are upstream's Button enum, in order */
}

/* A tap on the bottom screen (physical px): the screen's canvas starts 30 px down */
static bool sh_touch(ui_screen* s, float x, float y) {
  (void)s;
  return starter_touch(x, y - 30);
}

static void sh_update(ui_screen* s, float dt) {
  (void)s;
  mui_tick(dt);
  starter_update(dt);
}

static void sh_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_BOTTOM) {
    if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0, 0, 0, 255));
    mui_set_origin(RR_BOTTOM, 0, 30); /* UI_BOTTOM_CANVAS */
    starter_draw();
  } else {
    if (ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0, 0, 0, 255));
    mui_set_origin(RR_TOP, 40, 30); /* UI_TOP_CANVAS */
    starter_draw_top();
  }
}

static void sh_leave(ui_screen* s) {
  (void)s;
  starter_close();
}

/* SelectStarterPhase.start: the menu music */
static void sh_enter(ui_screen* s) {
  (void)s;
  bgm_on_starter_select();
}

static ui_screen g_screen = {"starter-select", UI_FADE, sh_enter, sh_leave, NULL, sh_input, sh_update, sh_draw, NULL, sh_touch};

bool sh_open(gc_system* sys, const char* notice, const sh_events* ev) {
  H.sys = sys;
  H.ev = *ev;
  H.n = ss_list(sys, H.entries, SS_MAX_ENTRIES, NULL);
  if (H.n == 0) return false;
  H.rng = 0x9E3779B9u ^ (uint32_t)(ui_now() * 1000.0f) ^ (ui_frame() * 2654435761u);
  if (!H.rng) H.rng = 1;
  const starter_args args = {H.entries, H.n, gc_starter_budget(GAME_MODES_CLASSIC)};
  const starter_events events = {describe, on_start, on_exit, rnd, NULL, configure};
  starter_open(&args, &events);
  ui_set(&g_screen);
  if (notice && *notice) starter_message(notice);
  return true;
}
