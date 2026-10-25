/* The starter select screen on the UI stack (see starter_host.h). */
#include "starter_host.h"

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "gc/gacha.h"

#include "bgm.h"
#include "i18n.h"
#include "starter_data.h"
#include "starter_filter.h"
#include "system_data.h"
#include "ui_widgets.h"
#include "pr_data.h"
const pr_species* menu_species(uint16_t id);
#include "ui.h"

static struct {
  gc_system* sys;
  sh_events ev;
  sm_entry all[SS_MAX_ENTRIES];     /* every species the dex lists, in species order */
  sm_entry entries[SS_MAX_ENTRIES]; /* what the screen shows: `all` less the filter, in the sort's order */
  int n, n_all;
  ssf filter;
  int fmode, fpage;
  uint32_t rng;uint64_t pokerus_bucket;
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
  if (H.ev.validate_team && !H.ev.validate_team(team,n,H.ev.arg,&why)) {
    starter_message(why && *why ? why : "This team cannot enter this mode.");
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
/* the list again from the system data (after a purchase or a change of filter) */
static void rebuild(void) {
  H.n_all=ss_list(H.sys,H.all,SS_MAX_ENTRIES,NULL);
  H.n=ssf_apply(H.sys,&H.filter,H.all,H.n_all,H.entries);
}
static void editor_refresh(void) {
  const int before=H.n;
  rebuild();
  if(H.n!=before)starter_set_entries(H.entries,H.n);
  else starter_refresh();
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
  } else if(mode==EDIT_CANDY && value==3) ok=gc_system_buy_starter_reduction(H.sys,H.editing);
  else if(mode==EDIT_CANDY) ok=gc_system_buy_starter_unlock(H.sys,H.editing,value==1);
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
    int reduction=gc_system_starter_reduction_cost(H.sys,H.editing);
    if(reduction>=0){
      gc_system *sys=H.sys;gc_starter_entry old=sys->starters[H.editing];
      sys->starters[H.editing].value_reduction++;double value=gc_starter_value(sys,H.editing);sys->starters[H.editing]=old;
      snprintf(label,sizeof label,"Reduce cost to %.2g (%d candies)",value,reduction);editor_add(3,label);
    }
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

/* ---- the filter and sort menu (upstream's filter bar, as a list) ---- */

enum { FM_TOP, FM_GEN, FM_TYPE, FM_CAUGHT, FM_RARITY };
enum { FR_GEN, FR_TYPE, FR_CAUGHT, FR_RARITY, FR_PASSIVE, FR_REDUCTION, FR_WIN, FR_HA, FR_EGG, FR_POKERUS, FR_SORT, FR_ORDER, FR_RESET, FR_DONE };
static void filter_show(int mode);

static const char* const state_names[SSF_STATES] = {"Any", "Yes", "No", "Bought once", "Bought twice", "Can unlock now"};
static const char* const sort_names[SSF_SORT_COUNT] = {"Number", "Cost", "Candies", "IVs", "Name", "Times caught", "Times hatched"};

static const char* filter_label(void* ctx) {
  (void)ctx;
  static char label[24];
  const int n=ssf_active(&H.filter);
  if(n)snprintf(label,sizeof label,"Filter (%d)",n);else snprintf(label,sizeof label,"Filter");
  return label;
}

static void filter_apply(void) {
  H.n=ssf_apply(H.sys,&H.filter,H.all,H.n_all,H.entries);
  starter_set_entries(H.entries,H.n);
}

/* a tri-state column: the states it offers, in the order a press steps through them */
static uint8_t step_state(uint8_t cur, const uint8_t* offered, int n) {
  for(int i=0;i<n;i++)if(offered[i]==cur)return offered[(i+1)%n];
  return offered[0];
}

static void filter_add(int value,const char* label) {
  if(H.count>=32)return;
  int i=H.count++;snprintf(H.labels[i],sizeof H.labels[i],"%s",label);
  H.options[i]=H.labels[i];H.values[i]=value;
}

static void filter_done(int row,void* arg) {
  (void)arg;
  const int mode=H.fmode;
  if(row<0||row>=H.count){if(mode==FM_TOP)return;filter_show(FM_TOP);return;}
  const int v=H.values[row];
  static const uint8_t yes_no[]={SSF_OFF,SSF_ON,SSF_EXCLUDE};
  static const uint8_t passive[]={SSF_OFF,SSF_ON,SSF_UNLOCKABLE,SSF_EXCLUDE};
  static const uint8_t reduction[]={SSF_OFF,SSF_ON,SSF_ONE,SSF_TWO,SSF_UNLOCKABLE,SSF_EXCLUDE};
  if(mode==FM_TOP) {
    switch(v) {
      case FR_GEN: filter_show(FM_GEN);return;
      case FR_TYPE: filter_show(FM_TYPE);return;
      case FR_CAUGHT: filter_show(FM_CAUGHT);return;
      case FR_RARITY: filter_show(FM_RARITY);return;
      case FR_PASSIVE: H.filter.passive=step_state(H.filter.passive,passive,4);break;
      case FR_REDUCTION: H.filter.reduction=step_state(H.filter.reduction,reduction,6);break;
      case FR_WIN: H.filter.win=step_state(H.filter.win,yes_no,3);break;
      case FR_HA: H.filter.hidden_ability=step_state(H.filter.hidden_ability,yes_no,3);break;
      case FR_EGG: H.filter.egg=step_state(H.filter.egg,yes_no,3);break;
      case FR_POKERUS: H.filter.pokerus=step_state(H.filter.pokerus,yes_no,3);break;
      case FR_SORT: H.filter.sort=(uint8_t)((H.filter.sort+1)%SSF_SORT_COUNT);break;
      case FR_ORDER: H.filter.descending=!H.filter.descending;break;
      case FR_RESET: ssf_reset(&H.filter);break;
      default: return; /* Done */
    }
    ui_play_select();
    filter_apply();
    filter_show(FM_TOP);
    return;
  }
  if(v<0) {filter_show(FM_TOP);return;} /* Back */
  /* a toggle in a multiple-choice column; an empty column would hide everything, so the last one stays on */
  if(mode==FM_GEN){uint16_t m=(uint16_t)(H.filter.gens^(1u<<v));if(m)H.filter.gens=m;}
  else if(mode==FM_TYPE){uint32_t m=H.filter.types^(1u<<v);if(m)H.filter.types=m;}
  else if(mode==FM_CAUGHT){uint8_t m=(uint8_t)(H.filter.caught^v);if(m)H.filter.caught=m;}
  else if(mode==FM_RARITY){uint8_t m=(uint8_t)(H.filter.rarity^v);if(m)H.filter.rarity=m;}
  ui_play_select();
  filter_apply();
  filter_show(mode);
}

static void filter_show(int mode) {
  H.fmode=mode;H.count=0;
  char label[128];
  if(mode==FM_TOP) {
    const ssf* f=&H.filter;
    const struct {int row;const char* name;int on,all;} multi[]={
      {FR_GEN,"Generation",__builtin_popcount(f->gens),9},{FR_TYPE,"Type",__builtin_popcount(f->types),18},
      {FR_CAUGHT,"Caught",__builtin_popcount(f->caught),4},{FR_RARITY,"Rarity",__builtin_popcount(f->rarity),4}};
    for(int i=0;i<4;i++){
      if(multi[i].on==multi[i].all)snprintf(label,sizeof label,"%s: all",multi[i].name);
      else snprintf(label,sizeof label,"%s: %d of %d",multi[i].name,multi[i].on,multi[i].all);
      filter_add(multi[i].row,label);
    }
    const struct {int row;const char* name;uint8_t state;} tri[]={
      {FR_PASSIVE,"Passive",f->passive},{FR_REDUCTION,"Cost reduction",f->reduction},{FR_WIN,"Has won",f->win},
      {FR_HA,"Hidden ability",f->hidden_ability},{FR_EGG,"Egg affordable",f->egg},{FR_POKERUS,"Pokerus",f->pokerus}};
    for(int i=0;i<6;i++) {
      /* the Exclude state reads as "locked" for the unlocks and "No" elsewhere */
      const char* st=state_names[tri[i].state];
      if(tri[i].state==SSF_EXCLUDE&&(tri[i].row==FR_PASSIVE||tri[i].row==FR_REDUCTION))st="Locked";
      if(tri[i].state==SSF_ON&&(tri[i].row==FR_PASSIVE))st="Unlocked";
      snprintf(label,sizeof label,"%s: %s",tri[i].name,st);filter_add(tri[i].row,label);
    }
    snprintf(label,sizeof label,"Sort by: %s",sort_names[f->sort]);filter_add(FR_SORT,label);
    snprintf(label,sizeof label,"Order: %s",f->descending?"Descending":"Ascending");filter_add(FR_ORDER,label);
    filter_add(FR_RESET,"Reset");filter_add(FR_DONE,"Done");
  } else if(mode==FM_GEN) {
    for(int g=0;g<9;g++){snprintf(label,sizeof label,"%s Gen %d",(H.filter.gens>>g)&1?"[x]":"[ ]",g+1);filter_add(g,label);}
    filter_add(-1,"Back");
  } else if(mode==FM_TYPE) {
    for(int t=0;t<18;t++){snprintf(label,sizeof label,"%s %s",(H.filter.types>>t)&1?"[x]":"[ ]",i18n_type_name(t));filter_add(t,label);}
    filter_add(-1,"Back");
  } else if(mode==FM_CAUGHT) {
    static const struct {int bit;const char* name;} c[]={{SSF_C_SHINY3,"Epic shiny"},{SSF_C_SHINY2,"Rare shiny"},{SSF_C_SHINY1,"Shiny"},{SSF_C_NORMAL,"Normal"}};
    for(int i=0;i<4;i++){snprintf(label,sizeof label,"%s %s",H.filter.caught&c[i].bit?"[x]":"[ ]",c[i].name);filter_add(c[i].bit,label);}
    filter_add(-1,"Back");
  } else if(mode==FM_RARITY) {
    static const struct {int bit;const char* name;} r[]={{SSF_R_ORDINARY,"Ordinary"},{SSF_R_SUB_LEGENDARY,"Sub-legendary"},{SSF_R_LEGENDARY,"Legendary"},{SSF_R_MYTHICAL,"Mythical"}};
    for(int i=0;i<4;i++){snprintf(label,sizeof label,"%s %s",H.filter.rarity&r[i].bit?"[x]":"[ ]",r[i].name);filter_add(r[i].bit,label);}
    filter_add(-1,"Back");
  }
  ui_push_options(H.options,H.count,true,filter_done,NULL);
}

static void open_filter(void* ctx) {
  (void)ctx;
  filter_show(FM_TOP);
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
  time_t now=time(NULL);uint64_t bucket=now>0?(uint64_t)now/10800:0;
  if(bucket!=H.pokerus_bucket){H.pokerus_bucket=bucket;editor_refresh();}
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
  time_t now=time(NULL);H.pokerus_bucket=now>0?(uint64_t)now/10800:0;
  H.ev = *ev;
  ssf_reset(&H.filter);
  rebuild();
  if (H.n == 0) return false;
  H.rng = 0x9E3779B9u ^ (uint32_t)(ui_now() * 1000.0f) ^ (ui_frame() * 2654435761u);
  if (!H.rng) H.rng = 1;
  const starter_args args = {H.entries, H.n, gc_starter_budget(GAME_MODES_CLASSIC), H.all, H.n_all};
  const starter_events events = {describe, on_start, on_exit, rnd, NULL, configure, open_filter, filter_label};
  starter_open(&args, &events);
  ui_set(&g_screen);
  if (notice && *notice) starter_message(notice);
  return true;
}
