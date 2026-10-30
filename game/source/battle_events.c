/* battle-core's events as battle screen events (see battle_events.h). */
#include "battle_events.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_anim.h"
#include "i18n.h"
#include "pak.h"
#include "pr/events.h"
#include "pr_enums.h"

/* ------------------------------------------------------------------ text source */

#define LOC_MAX 48
static struct {
  char lang[16], file[64]; /* "mystery-encounters/the-expert-pokemon-breeder-dialogue" is 54 */
  char* text; /* NULL = missing */
  size_t len;
} g_loc[LOC_MAX];
static int g_nloc;

static const char* loc_file(const char* lang, const char* file, size_t* len) {
  for (int i = 0; i < g_nloc; i++)
    if (!strcmp(g_loc[i].lang, lang) && !strcmp(g_loc[i].file, file)) return *len = g_loc[i].len, g_loc[i].text;
  char key[128];
  snprintf(key, sizeof key, "locales/%s/%s.json", lang, file);
  size_t n = 0;
  char* t = pak_read(key, &n);
  if (g_nloc == LOC_MAX) { /* full: drop the oldest */
    free(g_loc[0].text);
    memmove(&g_loc[0], &g_loc[1], sizeof g_loc[0] * (LOC_MAX - 1));
    g_nloc--;
  }
  snprintf(g_loc[g_nloc].lang, sizeof g_loc[g_nloc].lang, "%s", lang);
  snprintf(g_loc[g_nloc].file, sizeof g_loc[g_nloc].file, "%s", file);
  g_loc[g_nloc].text = t;
  g_loc[g_nloc].len = n;
  g_nloc++;
  *len = n;
  return t;
}

static const char* text_get(void* ctx, const char* file, int fallback, size_t* len) {
  (void)ctx;
  /* upstream fallbackLng: es-419 -> es-ES -> en; everything else -> en */
  const char* lang = i18n_language();
  const char* chain[3];
  int n = 0;
  chain[n++] = lang;
  if (!strcmp(lang, "es-419")) chain[n++] = "es-ES";
  if (strcmp(lang, "en")) chain[n++] = "en";
  if (fallback < 0 || fallback >= n) return NULL;
  return loc_file(chain[fallback], file, len);
}

const pr_text_source* be_text_source(void) {
  static pr_text_source src = {text_get, NULL, NULL};
  src.lang = i18n_language();
  return &src;
}

void be_format_message(const pr_msg *msg,char *out,size_t size){
  pr_msg display=*msg;
  for(int i=0;i<display.n_args;i++)if(display.args[i].kind==PR_ARG_MOVE)display.args[i].v|=PR_MSG_MOVE_BARE;
  pr_msg_format(&display,be_text_source(),out,size);
}


/* ------------------------------------------------------------------ Pokemon */

void be_mon(const pr_battle* bt, const pr_pokemon* p, bv_mon* m) {
  memset(m, 0, sizeof *m);
  /* PR_DEV_ILLUSION: a disguised Pokemon looks, is named and is typed as its disguise */
  const bool look = pr_shows_illusion(bt, p);
  m->pokemon_id=p->id;m->identity_known=true;
  m->illusion=look;
  const pr_form* form = pr_shown_form(bt, p);
  m->species = form->species;
  m->form = form->form_index;
  m->gender = look ? pr_apparent_gender(p, true) : pr_get_gender(p);
  m->female = m->gender == 1;
  m->shiny = p->shiny;
  m->variant = p->variant;
  m->sprite_scale = (float)pr_get_sprite_scale(p);
  snprintf(m->name, sizeof m->name, "%s", i18n_species_name(form->species));
  m->level = p->level;
  m->hp = p->hp;
  m->max_hp = p->stats[STAT_HP];
  m->status = p->status;
  int8_t types[PR_MAX_TYPES];
  int n = pr_get_types(p, types, look ? PR_TYPES_USE_ILLUSION : 0);
  for (int i = 0; i < n && i < 3; i++) m->types[m->n_types++] = types[i];
  for (int i = 0; i < PR_N_MOVES(p) && i < 4; i++) {
    int base = pr_moves[PR_MOVES(p)[i]].pp;
    int step = base / 5 < 1 ? 1 : base / 5;
    int max = PR_MAX_PP(p)[i] > 0 ? PR_MAX_PP(p)[i] : base + PR_PP_UP(p)[i] * step;
    m->moves[i] = PR_MOVES(p)[i];
    const int move=m->moves[i];m->move_power[i]=pr_moves[move].power;
    if(bt&&(move==MOVE_ID_ROLLOUT||move==MOVE_ID_ICE_BALL||move==MOVE_ID_FURY_CUTTER||move==MOVE_ID_ECHOED_VOICE)){
      /* The native multiplier skips the in-flight entry. Add that entry on
       * copies to preview the next use, without touching gameplay state/RNG. */
      static pr_battle preview;static pr_pokemon candidate;
      preview=*bt;candidate=*p;
      pr_push_move_history(&candidate,(pr_turn_move){.move=move,.result=MOVE_RESULT_SUCCESS});
      double power=pr_moves[move].power;
      pr_apply_variable_power(&preview,&candidate,NULL,move,&power);
      m->move_power[i]=(int)power;
    }
    m->pp_max[i] = max;
    m->pp_left[i] = max - PR_PP_USED(p)[i];
  }
  m->n_moves = PR_N_MOVES(p);
  m->boss = p->is_boss;
  m->boss_segments = p->boss_segments;
  m->boss_segment_index = p->boss_segment_index;
  for (int i = 0; i < 7; i++) m->stat_stages[i] = p->stat_stages[i];
}

/* ------------------------------------------------------------------ events */

static int view_slot(int battler){return (battler%2)*2+(battler>=2);}

#define TXT_MAX 64
static char g_txt[TXT_MAX][256];
static char ability_actor_names[PR_MAX_EVENTS][64];

typedef struct {
  int species, form;
  int hp, status;
  bool fainted, present, visible, substitute;
  int max_hp;
  int shield;                /* bossSegmentIndex the screen shows */
} seen_t;

/* Switch snapshots identify the actual Pokemon, even if its form and party
 * position changed later in the batch. Older synthetic snapshots use the
 * legacy appearance lookup below. */
static const pr_pokemon* be_switched_in(const pr_pokemon* const* party, int n, const pr_pokemon* on_field, const pr_event_field* f) {
  if (!f->species) return NULL;
  if (f->pokemon_id_known) {
    for (int k = 0; k < n; k++)
      if (party[k] && party[k]->id == f->pokemon_id) return party[k];
    return on_field && on_field->id == f->pokemon_id ? on_field : NULL;
  }
  if (f->party_slot >= 0 && f->party_slot < n) {
    const pr_pokemon* p = party[f->party_slot];
    if (p && p->species == f->species && p->form == f->form && (f->max_hp <= 0 || p->stats[STAT_HP] == f->max_hp))
      return p;
    /* PR_DEV_ILLUSION: the snapshot showed its disguise, which looks like another party member; the slot is the one
     * (the disguise may have ended later in the same events) */
    if (p && (p->illusion.active || p->ability == ABILITY_ID_ILLUSION || p->passive == ABILITY_ID_ILLUSION)
        && (f->max_hp <= 0 || p->stats[STAT_HP] == f->max_hp))
      return p;
  }
  const pr_pokemon *same_hp = NULL, *same = NULL;
  for (int k = 0; k < n; k++) {
    const pr_pokemon* p = party[k];
    if (!p || p->species != f->species || p->form != f->form) continue;
    if (!same || p == on_field) same = p;
    if (f->max_hp > 0 && p->stats[STAT_HP] == f->max_hp && (!same_hp || p == on_field)) same_hp = p;
  }
  if (same_hp || same) return same_hp ? same_hp : same;
  /* its species or form changed since (it evolved or changed form later in the same events): the slot */
  if (f->party_slot >= 0 && f->party_slot < n) return party[f->party_slot];
  return NULL;
}

/* PR_DEV_ILLUSION: the look a field snapshot shows (a disguise starting or ending), on the screen's copy */
static void look_of(bv_mon* mon, const bv_mon* was, const pr_event_field* f) {
  *mon = *was;
  mon->species = f->species;
  mon->form = f->form;
  mon->gender = f->gender;
  mon->female = mon->gender == 1;
  mon->sprite_scale = f->sprite_scale;
  mon->n_types = f->n_types;
  for (int k = 0; k < 3; k++) mon->types[k] = f->types[k];
  snprintf(mon->name, sizeof mon->name, "%s", i18n_species_name(mon->species));
}

int be_convert_events(const pr_battle* bt, const pr_event* events, int n_events, bool settled, be_view* view,
                      bv_event* out, int max) {
  int n = 0, nt = 0;
  const char* go_text = NULL;
  seen_t seen[4] = {{0}};
  bv_mon current[4];
  for(int k=0;k<4;k++){
    int i=view_slot(k);view->sent[i]=NULL;current[i]=view->mon[i];
    const bv_mon *m=&view->mon[i];
    seen[k]=(seen_t){m->species,m->form,m->hp,m->status,!view->present[i],view->present[i],!m->hidden,m->substitute,m->max_hp,m->boss_segment_index};
  }
  enum { SENT_MAX = 16 };
  static bv_mon sent[SENT_MAX];
  int n_sent = 0;
#define PUT(...)                                    \
  do {                                              \
    if (n < max) out[n++] = (bv_event){__VA_ARGS__}; \
  } while (0)

  for (int i = 0; i < n_events; i++) {
    const pr_event* ev = &events[i];
    int b = ev->battler;
    if(ev->type==PR_EV_FIELD_SWAP){
      bool already=current[0].identity_known&&ev->field[0].pokemon_id_known&&current[0].pokemon_id==ev->field[0].pokemon_id;
      if(!already){
      seen_t seen_tmp=seen[0];seen[0]=seen[1];seen[1]=seen_tmp;
      bv_mon tmp=current[0];current[0]=current[2];current[2]=tmp;
      PUT(.type=BEV_FIELD_SWAP,.battler=1,.target=0);
      }
      for(int k=0;k<2;k++)view->sent[view_slot(k)]=be_switched_in((const pr_pokemon*const*)bt->party[0],bt->n_party[0],bt->field[k],&ev->field[k]);
    }
    bool single = b>=0&&b<4; /* all four native battler slots */
    /* In the air or underground: the snapshot after this event says whether the Pokemon is still in
     * sight, and the change came before the event (the tag goes at MOVE_EFFECT, before the hit's
     * animation; "X flew up high!" follows the hiding). A switch or a faint changes it their own way. */
    if (ev->type != PR_EV_SWITCH_OUT && ev->type != PR_EV_SWITCH_IN && ev->type != PR_EV_FAINT) {
      for (int k = 0; k < 4; k++) {
        /* a Pokemon sent out later in these events (dragged out by Roar, Whirlwind...) already shows in the
         * snapshots: that is a replacement, not a form change */
        bool replaced = false;
        for (int j = i + 1; j < n_events && !replaced; j++)
          replaced = events[j].type == PR_EV_SWITCH_IN && events[j].battler == k;
        if (!replaced && seen[k].present && !seen[k].fainted && ev->field[k].species && !ev->field[k].fainted) {
          if (ev->field[k].visible != seen[k].visible) {
            PUT(.type = BEV_VISIBLE, .battler = k, .a = ev->field[k].visible);
            seen[k].visible = ev->field[k].visible;
          }
          /* a hit broke one of a boss's shields (Pokemon.damage clears them before the damage is shown) */
          if (ev->field[k].boss_segments && ev->field[k].boss_segment_index < seen[k].shield) {
            PUT(.type = BEV_BOSS_SHIELD, .battler = k, .a = ev->field[k].boss_segment_index);
            seen[k].shield = ev->field[k].boss_segment_index;
            /* Transform and Illusion redraws copy this actor later in the batch. */
            current[view_slot(k)].boss_segment_index = (uint8_t)seen[k].shield;
          }
          /* the maximum changed (a form with other base stats: Zygarde, Morpeko): the bar's numbers follow */
          if (ev->field[k].max_hp > 0 && ev->field[k].max_hp != seen[k].max_hp) {
            PUT(.type = BEV_MAX_HP, .battler = k, .a = ev->field[k].max_hp);
            seen[k].max_hp = ev->field[k].max_hp;
          }
            /* a Transform or a disguise ending (PR_DEV_ILLUSION) redraws the whole look below, not just the form */
            bool transforming = (ev->type == PR_EV_TRANSFORM || ev->type == PR_EV_ILLUSION_BREAK) && b == k;
            for (int j = i + 1; j < n_events && !transforming; j++)
              transforming = (events[j].type == PR_EV_TRANSFORM || events[j].type == PR_EV_ILLUSION_BREAK) && events[j].battler == k;
            if (!transforming && ev->field[k].species != seen[k].species && (bt->deviations & PR_DEV_ILLUSION) && n_sent < SENT_MAX) {
            /* another species without a Transform: a disguise (it took one at the battle's start) */
            bv_mon* mon = &sent[n_sent++];
            look_of(mon, &current[view_slot(k)], &ev->field[k]);
            mon->illusion=true;
            mon->hp = seen[k].hp; mon->max_hp = seen[k].max_hp; mon->status = seen[k].status;
            PUT(.type = BEV_ILLUSION, .battler = k, .a = 0, .mon = mon);
            seen[k].species = mon->species;
            seen[k].form = mon->form;
            current[view_slot(k)] = *mon;
          } else if (!transforming && (ev->field[k].species != seen[k].species || ev->field[k].form != seen[k].form)) {
            PUT(.type = BEV_FORM_CHANGE, .battler = k, .a = ev->field[k].species, .b = ev->field[k].form, .c = ev->field[k].gender + 2);
            seen[k].species = ev->field[k].species;
            seen[k].form = ev->field[k].form;
          }
        }
      }
    }
    switch (ev->type) {
      case PR_EV_ILLUSION_BREAK: {
        /* the disguise ends: the real Pokemon's look, name and types, with the Transform sound as breakIllusion plays */
        if (!single || !seen[b].present || n_sent >= SENT_MAX) break;
        bv_mon* mon = &sent[n_sent++];
        look_of(mon, &current[view_slot(b)], &ev->field[b]);
        mon->illusion=false;
        mon->hp = seen[b].hp; mon->max_hp = seen[b].max_hp; mon->status = seen[b].status;   /* the bar as the screen has it */
        PUT(.type = BEV_ILLUSION, .battler = b, .a = 1, .mon = mon);
        seen[b].species = mon->species;
        seen[b].form = mon->form;
        current[view_slot(b)] = *mon;
        break;
      }
      case PR_EV_TRANSFORM: {
        if (!single || !seen[b].present || n_sent >= SENT_MAX) break;
        bv_mon* mon = &sent[n_sent++];
        *mon = current[view_slot(b)];
        mon->species = ev->field[b].species;
        mon->form = ev->field[b].form;
        mon->gender = ev->transform.gender;
        mon->female = mon->gender == 1;
        mon->sprite_scale = ev->transform.sprite_scale;
        mon->hp = ev->field[b].hp;
        mon->max_hp = ev->field[b].max_hp;
        mon->status = ev->field[b].status;
        mon->substitute = ev->field[b].substitute;
        mon->hidden = !ev->field[b].visible;
        mon->n_types = ev->transform.n_types;
        for (int k = 0; k < 3; k++) mon->types[k] = ev->transform.types[k];
        mon->n_moves = ev->transform.n_moves;
        for (int k = 0; k < 4; k++) {
          mon->moves[k] = ev->transform.moves[k];
          mon->pp_max[k] = ev->transform.pp_max[k];
          mon->pp_left[k] = ev->transform.pp_left[k];
        }
        memcpy(mon->stat_stages, ev->transform.stat_stages, sizeof mon->stat_stages);
        PUT(.type = BEV_TRANSFORM, .battler = b, .a = ev->value, .mon = mon);
        seen[b].species = mon->species;
        seen[b].form = mon->form;
        current[view_slot(b)] = *mon;
        break;
      }
      case PR_EV_TEXT:
        if (!ev->msg.key || nt >= TXT_MAX) break;
        /* "Will you switch Pokemon?" is asked as a prompt in upstream (SET battle style only); the run doesn't
         * ask it, and the question alone would be a dead end on screen */
        if (!strcmp(ev->msg.key, "battle:switchQuestion")) break;
        be_format_message(&ev->msg,g_txt[nt],sizeof g_txt[nt]);
        if (i + 1 < n_events && events[i + 1].type == PR_EV_SWITCH_IN && events[i + 1].battler >= 0 &&
            events[i + 1].battler < 2) {
          /* "Go! X!": SwitchSummonPhase shows it while the ball is thrown, so the summon shows it */
          go_text = g_txt[nt++];
          break;
        }
        /* value = callbackDelay, id = prompt (0 = upstream's defaults) */
        PUT(.type = BEV_MESSAGE, .battler = -1, .text = g_txt[nt], .a = ev->value, .prompt = ev->id == 1);
        nt++;
        break;
      case PR_EV_SHINY_SPARKLE:
        if(single&&seen[b].present)PUT(.type=BEV_SHINY_SPARKLE,.battler=b);
        break;
      case PR_EV_MOVE_ANIM:
      case PR_EV_CHARGE_ANIM:
      case PR_EV_COMMON_ANIM:
        /* (a weather or terrain animation on a Pokemon that has just fainted has nobody to play on) */
        if (single && seen[b].present)
          PUT(.type = BEV_MOVE_ANIM, .battler = b, .target = ev->target, .a = ev->id,
              .b = ev->type == PR_EV_MOVE_ANIM ? BA_MOVE : ev->type == PR_EV_CHARGE_ANIM ? BA_CHARGE : BA_COMMON);
        break;
      case PR_EV_DAMAGE:
        if (!single || !seen[b].present) break;
        PUT(.type = BEV_DAMAGE, .battler = b, .a = ev->field[b].hp, .b = ev->id);
        seen[b].hp = ev->field[b].hp;
        break;
      case PR_EV_HP:
        if (!single) break;
        PUT(.type = BEV_HEAL, .battler = b, .a = ev->field[b].hp);
        seen[b].hp = ev->field[b].hp;
        break;
      case PR_EV_STAT_STAGES:
        if (!single) break;
        for (int s = STAT_ATK; s <= STAT_EVA; s++)
          if (ev->value & (1 << s)) PUT(.type = BEV_STAT_STAGES, .battler = b, .a = s, .b = ev->id ? 1 : -1);
        break;
      case PR_EV_ABILITY_SHOW:
        if(single){
          const char *name=current[view_slot(b)].name;
          if(!*name||current[view_slot(b)].species!=ev->field[b].species)name=i18n_species_name(ev->field[b].species);
          /* An ability can react after its owner fled or was recalled. Capture
           * the actor now; showing this overlay must not put its sprite back. */
          if(i<PR_MAX_EVENTS&&ev->field[b].species>0){
            snprintf(ability_actor_names[i],sizeof ability_actor_names[i],"%s",name);
            PUT(.type=BEV_ABILITY,.battler=b,.a=ev->id,.b=ev->value,.text=ability_actor_names[i]);
          }else PUT(.type=BEV_ABILITY,.battler=b,.a=ev->id,.b=ev->value);
        }
        break;
      case PR_EV_FAINT:
        if (!single || seen[b].fainted) break;
        /* the damage that made it faint isn't always an event of its own (Disguise busting): drain the bar */
        if (seen[b].hp > 0) {
          PUT(.type = BEV_DAMAGE, .battler = b, .a = 0, .b = HIT_RESULT_INDIRECT);
          seen[b].hp = 0;
        }
        PUT(.type = BEV_FAINT, .battler = b);
        seen[b].fainted = true;
        seen[b].present = false;
        break;
      case PR_EV_SWITCH_OUT:
        if (!single || !seen[b].present) break;
        /* A damaging forced exit applies leaveField before the move's DAMAGE
         * event. Drain its already-updated HP before withdrawing the sprite. */
        if(ev->field[b].hp<seen[b].hp){PUT(.type=BEV_DAMAGE,.battler=b,.a=ev->field[b].hp,.b=HIT_RESULT_INDIRECT);seen[b].hp=ev->field[b].hp;}
        PUT(.type = BEV_WITHDRAW, .battler = b);
        seen[b].present = false;
        break;
      case PR_EV_SWITCH_IN: {
        if (!single) break;
        const int side = b >= 2;
        /* value is the field slot; the Pokemon is found from the snapshot (species, form, max HP), as the
         * party may have been reordered by later switches in the same batch */
        /* (once the battle is over, the run keeps the enemy's party itself) */
        const pr_pokemon* const* party = (const pr_pokemon* const*)bt->party[side];
        int n_party = bt->n_party[side];
        if (side && !n_party && view->enemy_party) {
          party = view->enemy_party;
          n_party = view->n_enemy_party;
        }
        /* (a turn can send out two Pokemon on one side: a switch and then a drag-out; each keeps its own data) */
        bv_mon* sm = &sent[n_sent++ % SENT_MAX];
        const pr_pokemon* p = be_switched_in(party, n_party, bt->field[b], &ev->field[b]);
        view->sent[view_slot(b)] = p;
        if (p) {
          be_mon(bt, p, &(*sm));
          if (!side && view->exp_ratio) (*sm).exp_ratio = view->exp_ratio(p);
          if (!side && view->level_of) (*sm).level = view->level_of(p);
          if ((*sm).species != ev->field[b].species || (*sm).form != ev->field[b].form) {
            /* it changed after being sent out (the events that follow show that): send it out as it was */
            (*sm).pokemon_id=ev->field[b].pokemon_id;(*sm).identity_known=ev->field[b].pokemon_id_known;
          (*sm).species = ev->field[b].species;
            (*sm).form = ev->field[b].form;
            snprintf((*sm).name, sizeof (*sm).name, "%s", i18n_species_name(ev->field[b].species));
          }
        } else {
          memset(&(*sm), 0, sizeof (*sm));
          (*sm).pokemon_id=ev->field[b].pokemon_id;(*sm).identity_known=ev->field[b].pokemon_id_known;
          (*sm).species = ev->field[b].species;
          (*sm).form = ev->field[b].form;
          snprintf((*sm).name, sizeof (*sm).name, "%s", i18n_species_name(ev->field[b].species));
          (*sm).level = 1;
          (*sm).gender = -1;
          view->not_found++;
        }
        (*sm).hp = ev->field[b].hp;
        if (ev->field[b].max_hp > 0) (*sm).max_hp = ev->field[b].max_hp;
        (*sm).status = ev->field[b].status;
        (*sm).boss_segments = ev->field[b].boss_segments;
        (*sm).boss_segment_index = ev->field[b].boss_segment_index;
        (*sm).gender = ev->field[b].gender;
        (*sm).female = (*sm).gender == 1;
        (*sm).variant = ev->field[b].variant;
        (*sm).sprite_scale = ev->field[b].sprite_scale;
        (*sm).n_types = ev->field[b].n_types;
        for (int k = 0; k < 3; k++) (*sm).types[k] = ev->field[b].types[k];
        current[view_slot(b)] = *sm;
        /* a Pokemon dragged out (Roar, Whirlwind, Dragon Tail) has no SWITCH_OUT: the one on the field leaves */
        if (seen[b].present) PUT(.type = BEV_WITHDRAW, .battler = b);
        PUT(.type = BEV_SUMMON, .battler = b, .mon = &(*sm), .text = go_text);
        go_text = NULL;
        seen[b] = (seen_t){ev->field[b].species, ev->field[b].form, ev->field[b].hp, ev->field[b].status, false, true, true, ev->field[b].substitute, sm->max_hp, sm->boss_segment_index};
        break;
      }
      case PR_EV_POKEMON_ANIM:
        /* the Substitute doll's animations (Commander's aren't drawn yet) */
        if (single && ev->id <= POKEMON_ANIM_TYPE_SUBSTITUTE_REMOVE) {
          PUT(.type = BEV_POKEMON_ANIM, .battler = b, .a = ev->id);
          seen[b].substitute = ev->id != POKEMON_ANIM_TYPE_SUBSTITUTE_REMOVE;
        }
        break;
      default: /* ABILITY_HIDE: not drawn yet */
        break;
    }
    /* status icons follow the snapshot */
    for (int k = 0; k < 4; k++)
      if (seen[k].present && ev->field[k].species && ev->field[k].status != seen[k].status) {
        PUT(.type = BEV_STATUS, .battler = k, .a = ev->field[k].status);
        seen[k].status = ev->field[k].status;
      }
  }

  /* catch up on what the events didn't show, from the battle as it stands once the events have
   * happened (not when the run went on after them: a victory, the experience, the next wave) */
  for (int k = 0; k < 4 && settled; k++) {
    const pr_pokemon* p = bt->field[k];
    if (!p || !seen[k].present) continue;
    if (p->hp > 0 && p->stats[STAT_HP] > 0 && p->stats[STAT_HP] != seen[k].max_hp) PUT(.type = BEV_MAX_HP, .battler = k, .a = p->stats[STAT_HP]);
    if (p->boss_segments && p->boss_segment_index < seen[k].shield) {
      PUT(.type = BEV_BOSS_SHIELD, .battler = k, .a = p->boss_segment_index);
      seen[k].shield = p->boss_segment_index;
      current[view_slot(k)].boss_segment_index = (uint8_t)seen[k].shield;
    }
    if (p->hp < seen[k].hp) PUT(.type = BEV_DAMAGE, .battler = k, .a = p->hp, .b = HIT_RESULT_INDIRECT);
    else if (p->hp > seen[k].hp) PUT(.type = BEV_HEAL, .battler = k, .a = p->hp);
    if (p->status != seen[k].status) PUT(.type = BEV_STATUS, .battler = k, .a = p->status);
    if (p->hp <= 0 && !seen[k].fainted) PUT(.type = BEV_FAINT, .battler = k);
    /* a doll gone without its animation (SubstituteTag.onRemove while the Pokemon was in focus) */
    if (seen[k].substitute && !pr_get_battler_tag(p, BATTLER_TAG_TYPE_SUBSTITUTE))
      PUT(.type = BEV_POKEMON_ANIM, .battler = k, .a = -1);
    const pr_form* form = pr_shown_form(bt, p);
    if (p->hp > 0 && form->species != seen[k].species && (bt->deviations & PR_DEV_ILLUSION) && n_sent < SENT_MAX) {
      /* another species: a disguise started or ended (PR_DEV_ILLUSION), the whole look follows */
      bv_mon* mon = &sent[n_sent++];
      bv_mon live;
      be_mon(bt, p, &live);
      *mon = current[view_slot(k)];
      mon->species = live.species; mon->form = live.form; mon->gender = live.gender; mon->female = live.female;
      mon->sprite_scale = live.sprite_scale; mon->n_types = live.n_types;
      memcpy(mon->types, live.types, sizeof mon->types);
      memcpy(mon->name, live.name, sizeof mon->name);
      mon->hp = seen[k].hp; mon->max_hp = seen[k].max_hp; mon->status = seen[k].status;   /* the bar as the screen has it */
      PUT(.type = BEV_ILLUSION, .battler = k, .a = 0, .mon = mon);
      seen[k].species = form->species;
      seen[k].form = form->form_index;
    } else if (p->hp > 0 && (form->species != seen[k].species || form->form_index != seen[k].form)) {
      {
        bv_mon live;
        be_mon(bt, p, &live);
        PUT(.type = BEV_FORM_CHANGE, .battler = k, .a = form->species, .b = form->form_index, .c = live.gender + 2);   /* the look it shows: a disguise's gender too */
      }
      seen[k].species = form->species;
      seen[k].form = form->form_index;
    }
  }
#undef PUT
  for(int k=0;k<4;k++)view->present[view_slot(k)]=seen[k].present;
  return n;
}

int be_convert(const pr_battle* bt, const pr_engine* e, const bv_mon* shown_player, const bv_mon* shown_enemy,
               bv_event* out, int max) {
  be_view view = {{*shown_player, *shown_enemy}, {true, true}, NULL, 0};
  return be_convert_events(bt, e->events, e->n_events, true, &view, out, max);
}
