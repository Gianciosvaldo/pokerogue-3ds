/* The battle screen (see battle_view.h). Positions are upstream's: the field container for the
 * arena and sprites, fieldUI for the info boxes, the ui container for the bottom strip. */
#include "battle_view.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_anim.h"
#include "gm_enum_names.h"
#include "i18n.h"
#include "menus.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "tween.h"
#include "ui.h"
#include "ui_text.h"
#include "ui_widgets.h"

#define EV_MAX 160
#define EV_TEXT 256

/* ------------------------------------------------------------------ state */

typedef struct {
  bool active;
  int battler;
  int stat;         /* 1..7 (STAT_ATK..STAT_EVA) */
  int stage_delta;  /* +1 or -1 */
  int new_stage;    /* updated stage value -6..+6 */
  float timer;      /* ms remaining (600ms) */
  char text[40];    /* formatted indicator string e.g. "+1 ATK" */
} stat_banner_state;

typedef struct {
  bool active;
  int battler;
  int status;       /* StatusEffect */
  float timer;      /* ms remaining (1000ms) */
  char text[32];    /* status banner string e.g. "PSN", "PAR", "BRN", "SLP", "FRZ" */
} status_banner_state;

typedef struct {
  bv_mon mon;
  bool present;       /* on the field */
  bool sprite_on;     /* sprite visible (damage blink) */
  float drop;         /* faint: px the sprite has dropped */
  float scale;        /* summon */
  bool black;         /* intro silhouette */
  float info_dx;      /* info box slide offset */
  bool info_on;
  float hp_shown;     /* the HP bar's displayed hp (tweened) */
  float exp_shown;
  float show_ms;      /* BEV_VISIBLE: ms until the hidden Pokemon shows again */
  stat_banner_state stat_banner;     /* active stat modifier indicator banner */
  status_banner_state status_banner; /* active status visual banner */
  /* the Substitute doll (SubstituteTag.sprite) and the Pokemon stepping aside for it */
  bool sub_on;        /* the doll is on the field */
  float sub_alpha;
  float sub_off;      /* 0..1 of getSubstituteOffset() the Pokemon has moved */
  float mon_alpha;    /* the Pokemon's alpha (0.5 behind its doll) */
  bool tint_on, tint_above; /* the doll's white copy (setTintFill) and whether it's over the Pokemon */
  float tint_scale, tint_alpha;
  char sprite_key[64];
  menu_sprite sprite; /* shared species/form/gender/shiny/variant loader */
  /* a Poke Ball's catch attempt (BEV_CATCH) */
  float catch_dy;     /* px the Pokemon has moved up into the ball (negative) */
  bool catch_on;      /* drawn as a silhouette in the ball's colour, fading back to normal as catch_untint goes 0 -> 1 */
  float catch_untint;
  int catch_rgb[3];
} side;

typedef enum { M_IDLE, M_EVENTS, M_COMMAND, M_FIGHT } mode;

static struct {
  bv_setup setup;
  char biome[32];
  side s[2]; /* 0 player, 1 enemy */
  float arena_player_x, arena_enemy_x, enemy_slide;
  float arena_old_x;  /* the previous wave's enemy platform, sliding out (setup.next_encounter) */
  bool arena_old_on;
  bool begun;         /* bv_begin has run */
  float field_scale;  /* the field container's scale (updateFieldScale), tweened by field_tw */
  tween field_tw;
  msgbox msg;
  float msg_wrap;
  mode mode;
  int cmd_cursor, move_cursor;
  /* events */
  bv_event ev[EV_MAX];
  char ev_text[EV_MAX][EV_TEXT];
  bv_mon ev_mon[EV_MAX];
  int n_ev, cur;
  int step;       /* the current event's stage */
  float t;        /* ms in the current stage */
  tween tw, tw2;
  void (*on_done)(void*);
  void* done_arg;
  void (*on_command)(bv_command, void*);
  void* command_arg;
  bv_command_opts opts;   /* what the command menu allows */
  char reason[160];       /* why the move just chosen can't be used (shown over the move's description) */
  float reason_ms;
  char ability_text[96];
  float ability_t;
  float ko_wait;
  bool blink;
  float fps;
  int fps_frames;
  float fps_t;
  /* the thrown ball (it outlives its event: a caught ball stays on the field for the messages) */
  struct {
    bool on;
    int type;              /* PokeballType 0..4 */
    int look;              /* 0 closed, 1 opening, 2 open */
    float x, y, angle, alpha, flash;
    float star_dx[3];      /* addPokeballCaptureStars: how far each of the three stars drifts sideways (randGauss(25)) */
    float stars_t;         /* ms since the stars came out, -1 = none */
    float tx, top, end_dy;  /* the throw's target: x, y of the ball while it hangs, and how far the Pokemon moves up to meet it */
    bool behind;           /* moved below the Pokemon (upstream does that 300 ms into the throw) */
    bool fading;
    float fade_t;
    bool sub;              /* the enemy had a Substitute doll the throw put away */
  } ball;
  bv_item foe[BV_MAX_FOE_ITEMS]; /* the foe's items while the command is asked */
  int n_foe;
  /* promptLevelUpStats' window (BEV_LEVEL_UP) */
  struct {
    bool on, totals;
    const int* stats;
  } lvl;
} B;

static side* side_of(int battler) { return &B.s[battler >= 2 ? 1 : 0]; }
bv_mon* bv_mon_of(int battler) { return &side_of(battler)->mon; }
int bv_biome(void) { return B.begun ? B.setup.biome : -1; }

/* ------------------------------------------------------------------ helpers */

static const char* mon_form_key(uint16_t species, int form) {
  int lo = 0, hi = PR_SPECIES_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (pr_species_table[mid].id == species) {
      const pr_species* sp = &pr_species_table[mid];
      if (!sp || sp->nforms <= 1) return "";
      if (form >= sp->nforms) form = sp->nforms - 1;
      const char* k = pr_forms[sp->form0 + form].form_key;
      return k ? k : "";
    }
    if (pr_species_table[mid].id < species) lo = mid + 1;
    else hi = mid - 1;
  }
  return "";
}

static float get_mon_scale(const bv_mon* m) {
  if (m->sprite_scale > 0) return m->sprite_scale;
  const char* fk = mon_form_key((uint16_t)m->species, m->form);
  /* getSpriteScale: G-Max and the starmobiles are 1.5x (isMax(); a mystery encounter's customPokemonData.spriteScale isn't here yet) */
  if (fk && (strstr(fk, "gigantamax") || strstr(fk, "eternamax") || strstr(fk, "starmobile"))) {
    return 1.5f;
  }
  return 1.0f;
}

static void update_mon_form(bv_mon* m, int species, int form) {
  m->sprite_scale = 0; /* a permanent form change derives a new scale */
  m->species = species;
  m->form = form;
  int lo = 0, hi = PR_SPECIES_COUNT - 1;
  while (lo <= hi) {
    int mid = (lo + hi) / 2;
    if (pr_species_table[mid].id == species) {
      const pr_species* sp = &pr_species_table[mid];
      if (sp && sp->nforms > 0 && form < sp->nforms) {
        const pr_form* f = &pr_forms[sp->form0 + form];
        m->n_types = 0;
        if (f->type1 >= 0) m->types[m->n_types++] = f->type1;
        if (f->type2 >= 0) m->types[m->n_types++] = f->type2;
      }
      break;
    }
    if (pr_species_table[mid].id < species) lo = mid + 1;
    else hi = mid - 1;
  }
}

static void sprite_key(const bv_mon* m, bool back, char* out, int n) {
  const char* b = back ? "back/" : "";
  const char* s = m->shiny ? "shiny/" : "";
  const char* fk = mon_form_key((uint16_t)m->species, m->form);

  if (fk && *fk) {
    snprintf(out, n, "images/pokemon/%s%s%d-%s", b, s, m->species, fk);
    if (img_has(out)) return;
    if (m->shiny) {
      snprintf(out, n, "images/pokemon/%s%d-%s", b, m->species, fk);
      if (img_has(out)) return;
    }
  }

  snprintf(out, n, "images/pokemon/%s%s%d", b, s, m->species);
  if (!img_has(out) && m->shiny) {
    snprintf(out, n, "images/pokemon/%s%d", b, m->species);
  }
}

static void load_mon_sprite(side* sd, bool player) {
  const bv_mon* m = &sd->mon;
  menu_sprite_free(&sd->sprite);
  menu_mon visual = {.species = m->species, .form = m->form, .gender = m->gender,
                     .shiny = m->shiny, .variant = m->variant};
  menu_sprite_load(&sd->sprite, &visual, player);
  sprite_key(m, player, sd->sprite_key, sizeof sd->sprite_key);
}

static void set_mon(side* sd, const bv_mon* m, bool player) {
  sd->mon = *m;
  load_mon_sprite(sd, player);
  sd->hp_shown = m->hp;
  sd->exp_shown = m->exp_ratio;
  sd->sprite_on = true;
  sd->drop = 0;
  sd->scale = get_mon_scale(m);
  sd->black = false;
  sd->show_ms = 0;
  /* resetSprite: no doll, no offset */
  sd->sub_on = sd->tint_on = sd->mon.substitute = false;
  sd->sub_off = 0;
  sd->mon_alpha = 1;
  memset(&sd->stat_banner, 0, sizeof sd->stat_banner);
  memset(&sd->status_banner, 0, sizeof sd->status_banner);
  ba_show(player ? 0 : 2);
}

static void cry(const bv_mon* m) {
  char k[32];
  snprintf(k, sizeof k, "cry/%d", m->species);
  ui_play_sound(k);
}

static const char* name_with_affix(int battler) {
  /* getPokemonNameWithAffix: "Wild X" / "the opposing X" for enemies */
  static char out[2][96];
  static int i;
  i ^= 1;
  side* sd = side_of(battler);
  if (battler < 2) return snprintf(out[i], 96, "%s", sd->mon.name), out[i];
  i18n_t(out[i], 96, B.setup.trainer ? "battle:foePokemonWithAffix" : "battle:wildPokemonWithAffix", "pokemonName",
         sd->mon.name, NULL);
  return out[i];
}

/* ------------------------------------------------------------------ drawing: field */

static const rr_frame* sprite_frame(side* sd) {
  if (sd->sprite.n) return menu_sprite_frame(&sd->sprite, ui_now());
  int frame = (int)(ui_now() * 10 / 1000); /* upstream anims run at 10 fps */
  return img_anim_frame(sd->sprite_key, frame);
}

/* The field as battle animations see it: where each Pokemon stands and its current frame. */
static void mon_pos(const side* sd, bool player, float* x, float* y);

static void anim_field(ba_mon out[4]) {
  memset(out, 0, 4 * sizeof *out);
  for (int k = 0; k < 2; k++) {
    side* sd = &B.s[k];
    ba_mon* m = &out[k ? 2 : 0];
    m->on_field = sd->present;
    m->player = !k;
    mon_pos(sd, !k, &m->x, &m->y);
    m->frame = sprite_frame(sd);
    m->half_h = (m->frame ? m->frame->src_h : 40) * sd->scale / 2;
    m->species = sd->mon.species;
    m->visible = !sd->mon.hidden;
  }
}

/* The player's Pokemon stands at y 132 instead of upstream's 148: upstream's message strip (132 down) covers its lower 16 px, and
 * here, with the strip's text on the bottom screen, that would hide part of the Pokemon for nothing (lorenzo, 2026-10-11). Its feet
 * sit on the strip's top edge. Animations follow (battle_anim.c maps upstream's positions onto these). */
#define PLAYER_Y 132

/* Where a side's Pokemon stands: its field position plus the Substitute offset (getSubstituteOffset:
 * the player's steps back left and down, the enemy's right and up). */
/* Empty rows under the Pokemon's feet in its sprite: the fewest across its frames, so an animation keeps its bounce. Most sprites have
 * none; about 95 front and 110 back sprites (mostly Gen 9's single 96x96 pictures, e.g. Lechonk's 32) have 10 or more, and upstream
 * stands them on the picture's bottom edge, so they float above the platform. Here they stand on their lowest drawn row instead
 * (lorenzo, 2026-10-11); a sprite with no gap is drawn exactly as before. */
static int foot_gap(const side* sd) {
  rr_atlas* a = sd->sprite.atlas ? sd->sprite.atlas : img_get(sd->sprite_key);
  int n = a ? rr_atlas_count(a) : 0, gap = 0;
  for (int i = 0; i < n; i++) {
    const rr_frame* f = rr_atlas_frame(a, i);
    int g = f->src_h - f->off_y - f->h;
    if (i == 0 || g < gap) gap = g;
  }
  return gap > 0 ? gap : 0;
}

static void mon_pos(const side* sd, bool player, float* x, float* y) {
  *x = (player ? 106 : 236 + B.enemy_slide) + sd->sub_off * (player ? -30 : 30);
  *y = (player ? PLAYER_Y : 84) + sd->sub_off * (player ? 10 : -10) + sd->catch_dy + foot_gap(sd) * sd->scale;
}

/* The doll ("pkmn__sub" / "pkmn__back__sub", origin (0.5, 1), half size on the player's side) at
 * the Pokemon's own field position, and its white copy. */
static void draw_sub(const side* sd, bool player, bool tint) {
  const rr_frame* f = img_frame(player ? "images/pokemon/back/sub" : "images/pokemon/sub", NULL);
  if (!f) return;
  float scale = player ? 0.5f : 1, alpha = tint ? sd->tint_alpha : sd->sub_alpha;
  if (tint) scale = sd->tint_scale;
  if (alpha <= 0 || scale <= 0) return;
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = 0.5f;
  o.origin_y = 1.0f;
  o.scale_x = o.scale_y = scale;
  int a = (int)(alpha * 255 + 0.5f);
  o.tint = RR_RGBA(255, 255, 255, a > 255 ? 255 : a);
  if (tint) rr_set_tone(255, 255, 255);
  rr_draw(f, player ? 106 : 236 + B.enemy_slide, player ? PLAYER_Y : 84, &o);
  if (tint) rr_set_tone(0, 0, 0);
}

static void draw_sprite(side* sd, bool player) {
  if (!sd->present || !sd->sprite_on || sd->scale <= 0 || sd->mon.hidden || ba_hidden(player ? 0 : 2)) return;
  const rr_frame* f = sprite_frame(sd);
  float x, y;
  mon_pos(sd, player, &x, &y);
  if (!f) {
    rr_rect(x - 20, y - 40 + sd->drop, 40, 40, player ? RR_RGBA(0xd0, 0x60, 0x40, 255) : RR_RGBA(0x50, 0x60, 0xa0, 255));
    return;
  }
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = 0.5f;
  o.origin_y = 1.0f;
  o.scale_x = o.scale_y = sd->scale;
  if (sd->black) o.tint = RR_RGBA(0, 0, 0, 255);
  else if (sd->catch_on) {
    /* pokemon.tint(color) is a flat fill: black through the multiply, then the colour added back as the tone */
    const int v = (int)(255 * sd->catch_untint + 0.5f);
    o.tint = RR_RGBA(v, v, v, 255);
    rr_set_tone((int)(sd->catch_rgb[0] * (1 - sd->catch_untint)), (int)(sd->catch_rgb[1] * (1 - sd->catch_untint)),
                (int)(sd->catch_rgb[2] * (1 - sd->catch_untint)));
  } else if (sd->mon_alpha < 1) o.tint = RR_RGBA(255, 255, 255, (int)(sd->mon_alpha * 255 + 0.5f));
  if (sd->drop > 0) {
    /* fainting: the sprite sinks below its base line */
    int vis = (int)(f->src_h - sd->drop);
    if (vis <= 0) return;
    o.origin_y = 0;
    rr_draw_cropped(f, x, y - f->src_h + sd->drop, f->src_w, vis, &o);
    return;
  }
  rr_draw(f, x, y, &o);
  if (sd->catch_on) rr_set_tone(0, 0, 0);
}

/* The thrown ball (pb atlas: "pb", "pb_opening", "pb_open", and gb/ub/rb/mb), origin (0.5, 0.625) */
static void draw_ball(void) {
  static const char* const NAME[5] = {"pb", "gb", "ub", "rb", "mb"};
  if (!B.ball.on || B.ball.alpha <= 0) return;
  const int type = B.ball.type >= 0 && B.ball.type < 5 ? B.ball.type : 0;
  char frame[16];
  snprintf(frame, sizeof frame, "%s%s", NAME[type], B.ball.look == 1 ? "_opening" : B.ball.look == 2 ? "_open" : "");
  const rr_frame* f = img_frame("images/pb", frame);
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = 0.5f;
  o.origin_y = 0.625f;
  o.angle = B.ball.angle * 3.14159265f / 180.0f;
  o.tint = RR_RGBA(255, 255, 255, (int)(B.ball.alpha * 255 + 0.5f));
  if (!f) {
    rr_rect(B.ball.x - 5, B.ball.y - 5, 10, 10, RR_RGBA(0xd5, 0x29, 0x29, (int)(B.ball.alpha * 255)));
    return;
  }
  rr_draw(f, B.ball.x, B.ball.y, &o);
  if (B.ball.stars_t >= 0 && B.ball.stars_t < 500) {
    /* addPokeballCaptureStars: three stars (pb_particles 4) at half alpha rise 10 px (250 ms, Sine.easeOut) and fall back fading
     * (250 ms, Sine.easeIn) while drifting sideways (500 ms, linear); gone after 500 ms (the last 75 ms fade out) */
    const rr_frame* sf = img_frame("images/effects/pb_particles", "4.png");
    if (!sf) sf = img_frame("images/effects/pb_particles", "4");
    const float t = B.ball.stars_t;
    if (sf) {
      const float dy = t < 250 ? -10 * ease(EASE_SINE_OUT, t / 250) : -10 * (1 - ease(EASE_SINE_IN, (t - 250) / 250));
      /* the 425 ms fade takes over the alpha from wherever the fall had brought it */
      const float fall = 0.5f * (1 - ease(EASE_SINE_IN, ((t < 425 ? t : 425) - 250) / 250));
      const float a = t < 250 ? 0.5f : t < 425 ? fall : fall * (1 - (t - 425) / 75);
      for (int k = 0; k < 3; k++) {
        rr_draw_opts so = RR_DEFAULT_OPTS;
        so.origin_x = 0.5f;
        so.origin_y = 0.625f;
        so.tint = RR_RGBA(255, 255, 255, (int)(a * 255 + 0.5f));
        rr_draw(sf, B.ball.x + B.ball.star_dx[k] * (t / 500), B.ball.y + dy, &so);
      }
    }
  }
  if (B.ball.flash > 0) {
    o.tint = RR_RGBA(0, 0, 0, (int)(B.ball.flash * 255 * B.ball.alpha + 0.5f));
    rr_draw(f, B.ball.x, B.ball.y, &o);
  }
}

/* updateFieldScale(): when a bigger-than-normal Pokemon (G-Max, starmobile) is on the field, the field (platforms, Pokemon, battle
 * animations; not the background or the strip) zooms out about the arena's bottom centre (160, 132): floor((1 / biggest scale)^0.7 * 40)
 * / 40, so 1.5x gives 0.75. setFieldScale tweens it over |change| * 6 * 200 ms, Sine.easeInOut. Upstream calls it when a Pokemon is
 * summoned or returned, after a form change and when the wild Pokemon appears; with nobody on the field the scale stays as it is. */
static void update_field_scale(bool instant) {
  float biggest = 0;
  for (int k = 0; k < 2; k++)
    if (B.s[k].present && B.s[k].mon.species > 0) {
      const float sc = get_mon_scale(&B.s[k].mon);
      if (sc > biggest) biggest = sc;
    }
  if (biggest <= 0) return;
  const float target = floorf(powf(1 / biggest, 0.7f) * 40) / 40;
  const float from = B.field_scale;
  if (fabsf(target - from) < 0.0001f) return;
  tween_start(&B.field_tw, from, target, instant ? 0 : fabsf(from - target) * 6 * 200, EASE_SINE_IN_OUT);
  if (instant) B.field_scale = target;
}

static void field_zoom(bool on) { rr_set_zoom(on ? B.field_scale : 1, 160, 132); }

static void draw_field(void) {
  char k[64];
  snprintf(k, sizeof k, "images/arenas/%s_bg", B.biome);
  if (img_has(k))
    img_draw(k, NULL, 0, 0, 0, 0);
  else
    rr_rect(0, 0, 320, 180, RR_RGBA(0x88, 0xc8, 0xf0, 255));
  field_zoom(true);
  snprintf(k, sizeof k, "images/arenas/%s_b", B.biome);
  if (B.arena_old_on) img_draw(k, NULL, B.arena_old_x, 0, 0, 0);
  img_draw(k, NULL, B.arena_enemy_x, 0, 0, 0);
  snprintf(k, sizeof k, "images/arenas/%s_a", B.biome);
  img_draw(k, NULL, B.arena_player_x, 0, 0, 0);
  field_zoom(false);
  field_zoom(true);
  ba_mon field[4];
  anim_field(field);
  ba_draw(BA_LAYER_BELOW_ENEMY, field);
  /* the enemy's doll is added over it; the player's Pokemon is brought over its doll */
  side *en = &B.s[1], *pl = &B.s[0];
  if (B.ball.behind) draw_ball();
  draw_sprite(en, false);
  if (en->present && en->sub_on) draw_sub(en, false, false);
  if (en->present && en->tint_on) draw_sub(en, false, true);
  ba_draw(BA_LAYER_MID, field);
  if (pl->present && pl->sub_on) draw_sub(pl, true, false);
  if (pl->present && pl->tint_on && !pl->tint_above) draw_sub(pl, true, true);
  draw_sprite(pl, true);
  if (pl->present && pl->tint_on && pl->tint_above) draw_sub(pl, true, true);
  ba_draw(BA_LAYER_TOP, field);
  if (!B.ball.behind) draw_ball();
  field_zoom(false);
}

/* ------------------------------------------------------------------ drawing: info boxes */

static const char* hp_frame(float ratio) { return ratio > 0.5f ? "high" : ratio > 0.25f ? "medium" : "low"; }

static void draw_numbers(const char* atlas, const char* s, float x, float y, int dir) {
  /* one image per character, 8 px apart, image origin (0.5, 0.5) */
  int n = (int)strlen(s);
  for (int i = 0; i < n; i++) {
    char fr[2] = {s[dir > 0 ? i : n - 1 - i], 0};
    img_draw(atlas, fr, x + dir * i * 8, y, 0.5f, 0.5f);
  }
}

static void draw_level(float lx, float ly, int level, bool red) {
  /* levelContainer at (lx, ly): overlay_lv at (5.5, 0) origin (1, 0.5); numbers at (9.5, -0.5) */
  char s[8];
  snprintf(s, sizeof s, "%d", level);
  int extra = (int)strlen(s) - 3;
  if (extra > 0) lx -= 8 * extra;
  img_draw("images/ui/text_images/en/battle_ui/overlay_lv", NULL, lx + 5.5f, ly, 1, 0.5f);
  draw_numbers(red ? "images/ui/numbers_red" : "images/ui/numbers", s, lx + 9.5f, ly - 0.5f, 1);
}

static void draw_types(const bv_mon* m, bool player, float bx, float by) {
  char k[48], fr[24];
  const char* who = player ? "player" : "enemy";
  float x1 = player ? -139 : -15, y1 = player ? -17 : -15.5f, y2 = player ? -1 : -2.5f;
  snprintf(k, sizeof k, "images/ui/pbinfo_%s_type%s", who, m->n_types > 1 ? "1" : "");
  gm_enum_lower(GM_E_POKEMON_TYPE, m->types[0], fr, sizeof fr);
  img_draw(k, fr, bx + x1, by + y1, 0, 0);
  if (m->n_types > 1) {
    snprintf(k, sizeof k, "images/ui/pbinfo_%s_type2", who);
    gm_enum_lower(GM_E_POKEMON_TYPE, m->types[1], fr, sizeof fr);
    img_draw(k, fr, bx + x1, by + y2, 0, 0);
  }
}

/* The name and gender sign: upstream's BATTLE_INFO is emerald at 72 canvas px (12 px here), which smears at 1:1 on the 3DS. They
 * use pkmnems at 8 px (INSTRUCTIONS_TEXT's font) in BATTLE_INFO's colours, 2 px higher so the baseline stays where it was. */
static float name_text(float x, float y, const char* t, rr_color c, rr_color sh) {
  text_opts o = text_defaults();
  o.color = c ? c : text_color(TS_BATTLE_INFO, false);
  o.shadow = sh ? sh : text_color(TS_BATTLE_INFO, true);
  return text_draw(TS_INSTRUCTIONS_TEXT, x, y - 2, t, &o);
}

static void draw_name(const bv_mon* m, bool player, float x, float y) {
  /* updateNameText: shortened with "." until it fits */
  char name[48];
  snprintf(name, sizeof name, "%s", m->name);
  float limit = 60 - ((m->gender < 0 ? 0 : 6) + (m->shiny ? 8 : 0) +
                      (fminf((float)snprintf(NULL, 0, "%d", m->level), 3) - 3) * 8);
  if (!player && m->boss) limit += 38;
  while (text_width(TS_INSTRUCTIONS_TEXT, name, NULL) > limit && strlen(name) > 2) {
    size_t n = strlen(name);
    int cut = name[n - 1] == '.' ? 2 : 1;
    name[n - cut] = 0;
    while (n > 1 && name[strlen(name) - 1] == ' ') name[strlen(name) - 1] = 0;
    strcat(name, ".");
  }
  float w = name_text(x, y, name, 0, 0);
  if (m->gender >= 0) {
    /* getGenderSymbol / getGenderColor */
    name_text(x + w, y, m->gender == 0 ? "♂" : "♀", m->gender == 0 ? RR_RGBA(0x40, 0xc8, 0xf8, 255) : RR_RGBA(0xf8, 0x90, 0x90, 255),
              m->gender == 0 ? RR_RGBA(0x00, 0x60, 0x90, 255) : RR_RGBA(0x98, 0x40, 0x38, 255));
  }
  if (m->status) {
    char fr[24];
    gm_enum_lower(GM_E_STATUS_EFFECT, m->status, fr, sizeof fr);
    img_draw("images/statuses", fr, x, y + 11.5f, 0, 0);
  }
}

static void draw_stat_indicators(side* sd, bool player, float bx, float by) {
  if (sd->stat_banner.active && sd->stat_banner.timer > 0) {
    float x = bx + (player ? -120 : -110);
    float y = by + (player ? -35 : -30);
    rr_rect(x, y, 70, 16, RR_RGBA(0, 0, 0, 192));
    text_opts o = text_defaults();
    o.color = sd->stat_banner.stage_delta > 0 ? RR_RGBA(0x50, 0xf0, 0x50, 255) : RR_RGBA(0xf0, 0x50, 0x50, 255);
    text_draw(TS_BATTLE_INFO, x + 4, y + 2, sd->stat_banner.text, &o);
  }
  if (sd->status_banner.active && sd->status_banner.timer > 0) {
    float x = bx + (player ? -45 : -35);
    float y = by + (player ? -35 : -30);
    rr_rect(x, y, 40, 16, RR_RGBA(0x80, 0x20, 0x20, 220));
    text_opts o = text_defaults();
    o.color = RR_RGBA(255, 255, 255, 255);
    text_draw(TS_BATTLE_INFO, x + 4, y + 2, sd->status_banner.text, &o);
  }
}

static void draw_player_info(side* sd) {
  if (!sd->present || !sd->info_on) return;
  const bv_mon* m = &sd->mon;
  float bx = 310 + sd->info_dx, by = -72; /* PlayerBattleInfo container */
  img_draw("images/ui/pbinfo_player", NULL, bx, by, 1, 0.5f);
  draw_types(m, true, bx, by);
  draw_name(m, true, bx - 115, by - 15.2f);
  draw_level(bx - 41, by - 10, m->level, false);
  float ratio = m->max_hp ? fminf(sd->hp_shown / m->max_hp, 1.0f) : 0;
  const rr_frame* hf = img_frame("images/ui/overlay_hp", hp_frame(ratio));
  int w = (int)floorf(48 * ratio + 0.5f);
  if (w < 1 && sd->hp_shown > 0) w = 1;
  if (hf && w > 0) rr_draw_cropped(hf, bx - 61, by - 1, w, hf->src_h, NULL);
  img_draw("images/ui/text_images/en/battle_ui/overlay_hp_label", NULL, bx - 62, by - 4, 1, 0);
  /* HP numbers: right to left from (-15, 10), "hp/max" */
  char s[24];
  snprintf(s, sizeof s, "%d/%d", (int)ceilf(sd->hp_shown - 0.001f), m->max_hp);
  draw_numbers("images/ui/numbers", s, bx - 15, by + 10, -1);
  /* EXP bar: overlay_exp at (-98, 18) masked to 85 px */
  img_draw("images/ui/text_images/en/battle_ui/overlay_exp_label", NULL, bx - 91, by + 20, 1, 1);
  const rr_frame* ef = img_frame("images/ui/overlay_exp", NULL);
  int ew = (int)floorf(85 * sd->exp_shown + 0.5f);
  if (ef && ew > 0) rr_draw_cropped(ef, bx - 98, by + 18, ew, ef->src_h, NULL);
  draw_stat_indicators(sd, true, bx, by);
}

static void draw_enemy_info(side* sd) {
  if (!sd->present || !sd->info_on) return;
  const bv_mon* m = &sd->mon;
  float bx = 140 + sd->info_dx, by = -141; /* EnemyBattleInfo container */
  img_draw("images/ui/pbinfo_enemy_mini", NULL, bx, by, 1, 0.5f);
  draw_types(m, false, bx, by);
  draw_name(m, false, bx - 124, by - 11.2f);
  if (m->caught_before) img_draw("images/ui/icon_owned", NULL, bx - 124, by - 11.2f + 11.75f, 0, 0);
  draw_level(bx - 50, by - 5, m->level, false);
  float ratio = m->max_hp ? fminf(sd->hp_shown / m->max_hp, 1.0f) : 0;
  const rr_frame* hf = img_frame("images/ui/overlay_hp", hp_frame(ratio));
  int w = (int)floorf(48 * ratio + 0.5f);
  if (w < 1 && sd->hp_shown > 0) w = 1;
  if (hf && w > 0) rr_draw_cropped(hf, bx - 71, by + 4.5f, w, hf->src_h, NULL);
  img_draw("images/ui/text_images/en/battle_ui/overlay_hp_label", NULL, bx - 72, by + 1.5f, 1, 0);
  draw_stat_indicators(sd, false, bx, by);
}

/* ------------------------------------------------------------------ drawing: bottom strip */

static void draw_command_menu(void) {
  ui_window(202, -48, 118, 48, WIN_NORMAL);
  static const char* const KEYS[4] = {"commandUiHandler:fight", "commandUiHandler:ball", "commandUiHandler:pokemon",
                                      "commandUiHandler:run"};
  for (int c = 0; c < 4; c++) {
    char s[32];
    i18n_t(s, sizeof s, KEYS[c], NULL);
    text_draw(TS_WINDOW_BATTLE_COMMAND, 217 + (c % 2 ? 55.8f : 0), -38.7f + (c < 2 ? 0 : 16), s, NULL);
  }
  ui_cursor(217 - 5 + (B.cmd_cursor % 2 ? 56 : 0), -38.7f + 8 + (B.cmd_cursor >= 2 ? 16 : 0));
}

static text_style pp_style(int left, int max) {
  float r = max ? (float)left / max : 1;
  if (r > 0.25f && r <= 0.5f) return TS_MOVE_PP_HALF_FULL;
  if (r > 0 && r <= 0.25f) return TS_MOVE_PP_NEAR_EMPTY;
  if (r == 0) return TS_MOVE_PP_EMPTY;
  return TS_MOVE_PP_FULL;
}

static void draw_fight_menu(void) {
  const bv_mon* m = &B.s[0].mon;
  ui_window(0, -48, 243, 48, WIN_NORMAL);
  ui_window(240, -48, 80, 48, WIN_NORMAL);
  for (int i = 0; i < 4; i++) {
    const char* nm = i < m->n_moves ? i18n_move_name(m->moves[i]) : "-";
    text_draw(TS_WINDOW, 18 + (i % 2 ? 114 : 0), -38.7f + (i < 2 ? 0 : 16), nm, NULL);
  }
  ui_cursor(13 + (B.move_cursor % 2 ? 114 : 0), -31 + (B.move_cursor >= 2 ? 15 : 0));
  if (B.move_cursor >= m->n_moves) return;
  int mv = m->moves[B.move_cursor];
  const pr_move* pm = mv >= 0 && mv < PR_MOVE_COUNT ? &pr_moves[mv] : NULL;
  char fr[24];
  /* moveInfoContainer at (1, 0) */
  /* upstream scales the type icon by 0.8; at 1:1 pixels that turns its 5 px lettering to mud ("NORMAL" cannot be read), so it is
   * drawn at full size, a little to the left of upstream's centre so it and the category icon (28 px) still fit the window */
  if (pm) {
    img_draw("images/types", gm_enum_lower(GM_E_POKEMON_TYPE, pm->type, fr, sizeof fr), 1 + 320 - 56, -36, 0.5f, 0.5f);
    img_draw("images/categories", gm_enum_lower(GM_E_MOVE_CATEGORY, pm->category, fr, sizeof fr), 1 + 320 - 24, -36,
             0.5f, 0.5f);
  }
  /* upstream draws these at 56 canvas px of the emerald font (about 9 px here), which is unreadable at 1:1 on the 3DS; the 8 px pixel
   * font (TS_MOVE_LABEL, the party screen's move labels) says the same words and numbers legibly in the same places. */
  text_opts lo = text_defaults();
  lo.origin_y = 0.5f;
  text_opts ro = text_defaults();
  ro.origin_x = 1;
  ro.origin_y = 0.5f;
  char s[32];
  text_draw(TS_MOVE_LABEL, 1 + 320 - 70, -26, i18n_t(s, sizeof s, "fightUiHandler:pp", NULL), &lo);
  text_draw(TS_MOVE_LABEL, 1 + 320 - 70, -18, i18n_t(s, sizeof s, "fightUiHandler:power", NULL), &lo);
  text_draw(TS_MOVE_LABEL, 1 + 320 - 70, -10, i18n_t(s, sizeof s, "fightUiHandler:accuracy", NULL), &lo);
  int left = m->pp_left[B.move_cursor], max = m->pp_max[B.move_cursor];
  snprintf(s, sizeof s, "%2d/%2d", left, max);
  text_style ps = pp_style(left, max);
  text_opts po = ro;
  po.color = text_color(ps, false);
  po.shadow = text_color(ps, true);
  text_draw(TS_MOVE_LABEL, 1 + 320 - 12, -26, s, &po);
  if (pm) {
    if (pm->power >= 0)
      snprintf(s, sizeof s, "%d", pm->power);
    else
      snprintf(s, sizeof s, "---");
    text_draw(TS_MOVE_LABEL, 1 + 320 - 12, -18, s, &ro);
    if (pm->accuracy >= 0)
      snprintf(s, sizeof s, "%d", pm->accuracy);
    else
      snprintf(s, sizeof s, "---");
    text_draw(TS_MOVE_LABEL, 1 + 320 - 12, -10, s, &ro);
  }
}

/* The free part of the bottom screen above the strip (y -240..-48): the move's description in
 * the fight menu (upstream shows it on the info button, MoveInfoOverlay). */
static void draw_move_details(void) {
  const bv_mon* m = &B.s[0].mon;
  if (B.move_cursor >= m->n_moves) return;
  char k[96], c[64], s[400];
  if (B.reason_ms > 0 && B.reason[0]) {
    /* the move can't be used: say why where the description is */
    snprintf(s, sizeof s, "%s", B.reason);
  } else {
    snprintf(k, sizeof k, "move:%s.effect", gm_enum_camel(GM_E_MOVE_ID, m->moves[B.move_cursor], c, sizeof c));
    i18n_t(s, sizeof s, k, NULL);
  }
  ui_window(0, -48 - 52, 320, 52, WIN_NORMAL);
  text_opts o = text_defaults();
  o.wrap_w = 320 - 16;
  o.max_lines = 3;
  text_draw(TS_WINDOW, 8, -48 - 52 + 4, s, &o);
}

static void draw_ability_bar(void) {
  if (B.ability_t <= 0) return;
  float w = text_width(TS_WINDOW, B.ability_text, NULL) + 16;
  ui_window(0, -48 - 30, w, 26, WIN_THIN);
  text_draw(TS_WINDOW, 8, -48 - 30 + 5, B.ability_text, NULL);
}

/* promptLevelUpStats' window (BattleMessageUiHandler): ui (320, -100) origin (1, 0), 80 + the labels' width by 100; labels end at
 * 320 - 73, the "+" column at 320 - 50 (gains only), the values end at 320 - 7; all from y -94, TextStyle.WINDOW */
static void draw_level_stats(void) {
  if (!B.lvl.on || !B.lvl.stats) return;
  char labels[192] = "", vals[96] = "";
  for (int s = 0; s < 6; s++) {
    char v[16];
    snprintf(v, sizeof v, "%d\n", B.lvl.totals ? B.lvl.stats[6 + s] : B.lvl.stats[6 + s] - B.lvl.stats[s]);
    strncat(vals, v, sizeof vals - strlen(vals) - 1);
    strncat(labels, i18n_stat_name(s), sizeof labels - strlen(labels) - 1);
    strncat(labels, "\n", sizeof labels - strlen(labels) - 1);
  }
  const float lw = text_width(TS_WINDOW, labels, NULL), w = 80 + lw;
  ui_window(320 - w, -100, w, 100, WIN_NORMAL);
  text_draw(TS_WINDOW, 320 - 73 - lw, -94, labels, NULL);
  if (!B.lvl.totals) text_draw(TS_WINDOW, 320 - 50, -94, "+\n+\n+\n+\n+\n+", NULL);
  text_opts o = text_defaults();
  o.origin_x = 1;
  o.align = 2;
  text_draw(TS_WINDOW, 320 - 7, -94, vals, &o);
}

/* The foe's items on the bottom screen (a choice of this port, not upstream's layout: its enemy ModifierBar sits on the top
 * screen at half scale). Icons at 3/4 scale, 12 a row, the stack count under each from 2 up; only while the command or a
 * move is being picked. */
static void draw_foe_items(void) {
  if (B.n_foe <= 0 || (B.mode != M_COMMAND && B.mode != M_FIGHT)) return;
  const int per_row = 12, rows = (B.n_foe + per_row - 1) / per_row;
  const float top = -226, cell = 26, h = 8 + rows * cell;
  ui_window(4, top, 312, h, WIN_THIN);
  for (int i = 0; i < B.n_foe; i++) {
    const float x = 10 + (i % per_row) * cell, y = top + 4 + (i / per_row) * cell;
    const rr_frame* f = B.foe[i].icon ? mui_frame("items", B.foe[i].icon) : NULL; /* the menus' copy of the atlas (shop, summary) */
    if (f) {
      rr_draw_opts o = RR_DEFAULT_OPTS;
      o.scale_x = o.scale_y = 0.75f;
      rr_draw(f, x, y, &o);
    }
    if (B.foe[i].count > 1) {
      char n[8];
      snprintf(n, sizeof n, "%d", B.foe[i].count);
      text_opts t = text_defaults();
      t.origin_x = 1;
      text_draw(TS_INSTRUCTIONS_TEXT, x + cell - 2, y + cell - 12, n, &t);
    }
  }
}

/* Upstream's ui container strip (y -48..0): the message window's bg frame, the command window or the fight windows, and the text.
 * It is drawn twice: on the bottom screen, where the touch-free controls live, and on the top screen over the field's last 48 rows,
 * where upstream puts it (it covers the lower part of the player's Pokemon, as upstream's does). `top` leaves out what upstream
 * draws elsewhere: the move description (MoveInfoOverlay) and the ability bar. */
static void draw_bottom(bool top) {
  /* the strip's background: bg frame <window style> at (0,0), origin (0,1) */
  char fr[4];
  snprintf(fr, sizeof fr, "%d", ui_window_style());
  if (B.mode != M_FIGHT) {
    if (img_frame("images/ui/bg", fr))
      img_draw("images/ui/bg", fr, 0, 0, 0, 1);
    else
      rr_rect(0, -48, 320, 48, RR_RGBA(0x30, 0x50, 0x68, 255));
  }
  if (B.mode == M_COMMAND) draw_command_menu();
  if (B.mode == M_FIGHT) {
    draw_fight_menu();
    if (!top) draw_move_details();
  } else {
    msgbox_draw(&B.msg, 12, -39);
  }
  draw_level_stats();
  if (!top) {
    draw_ability_bar();
    draw_foe_items();
  }
}

static void draw_debug(void) {
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_stats st = rr_get_stats();
  char d[160];
  snprintf(d, sizeof d, "%.0f fps  cpu %.1f ms  gpu %.1f ms  tex %.1f MB", B.fps, st.cpu_ms, st.gpu_ms,
           st.tex_bytes / 1048576.0);
  rr_text_sys(2, 2, 0.4f, d, RR_RGBA(0x90, 0x90, 0x98, 255));
}

/* ------------------------------------------------------------------ events */

static void finish_event(void) {
  B.cur++;
  B.step = 0;
  B.t = 0;
}

static float hp_tween_ms(float from, float to) {
  float d = fabsf(from - to) * 5; /* Phaser.Math.Clamp(|lastHp - hp| * 5, 250, 5000) */
  return d < 250 ? 250 : d > 5000 ? 5000 : d;
}

/* 0..1 through a tween that starts at `delay` ms and runs `ms` ms, eased */
static float seg(float t, float delay, float ms, ease_fn f) {
  float u = (t - delay) / ms;
  return ease(f, u < 0 ? 0 : u > 1 ? 1 : u);
}

/* PokemonAnimPhase's Substitute animations at time t (ms). Returns true while running. */
static bool substitute_anim(side* sd, int type, float t) {
  bool player = sd == &B.s[0];
  float full = player ? 0.5f : 1; /* getSpriteScale() * (isPlayer ? 0.5 : 1) */
  float u;
  switch (type) {
    case POKEMON_ANIM_TYPE_SUBSTITUTE_ADD:
      /* the Pokemon steps aside and fades to half; a white doll grows, then the doll shows and
       * the white fades off it */
      if (B.step == 0) ui_play_sound("battle_anims/PRSFX- Transform"), B.step = 1;
      u = seg(t, 0, 500, EASE_SINE_IN);
      sd->sub_off = u;
      sd->mon_alpha = 1 - 0.5f * u;
      sd->tint_on = true;
      sd->tint_above = false;
      if (t < 750) {
        sd->tint_scale = 0.01f + (full - 0.01f) * seg(t, 250, 500, EASE_CUBIC_IN_OUT);
        sd->tint_alpha = 1;
      } else {
        sd->sub_on = true;
        sd->sub_alpha = 1;
        sd->tint_scale = full;
        sd->tint_alpha = 1 - seg(t, 1000, 1000, EASE_CUBIC_OUT);
      }
      if (t < 2000) return true;
      sd->tint_on = false;
      sd->mon.substitute = true;
      return false;
    case POKEMON_ANIM_TYPE_SUBSTITUTE_PRE_MOVE:
      /* the doll fades out and the Pokemon comes forward, whole again, to use its move */
      if (!sd->sub_on) return false;
      sd->sub_alpha = 1 - seg(t, 0, 500, EASE_SINE_IN_OUT);
      u = seg(t, 250, 500, EASE_SINE_IN_OUT);
      sd->sub_off = 1 - u;
      sd->mon_alpha = 0.5f + 0.5f * u;
      return t < 750;
    case POKEMON_ANIM_TYPE_SUBSTITUTE_POST_MOVE:
      if (!sd->sub_on) return false;
      u = seg(t, 0, 500, EASE_SINE_IN_OUT);
      sd->sub_off = u;
      sd->mon_alpha = 1 - 0.5f * u;
      sd->sub_alpha = seg(t, 250, 500, EASE_SINE_IN_OUT);
      return t < 750;
    case POKEMON_ANIM_TYPE_SUBSTITUTE_REMOVE: {
      /* a white doll fades in over it, the doll goes, the white one flashes 8 times (100 ms
       * apart, from the first frame: startAt 200 > delay 100) and shrinks away while the Pokemon
       * steps back */
      if (!sd->sub_on && !sd->tint_on) return false;
      sd->tint_on = true;
      sd->tint_above = true;
      sd->tint_scale = full;
      if (t < 500) {
        sd->tint_alpha = seg(t, 0, 500, EASE_SINE_IN_OUT);
        return true;
      }
      sd->sub_on = false;
      sd->tint_alpha = 1;
      /* Phaser's timer (delay 100, startAt 200) runs its first two callbacks on the first two
       * frames, then one every 100 ms: callback k (0..7) is at 0, 17, 117, 217... after the fade.
       * The copy shows on even repeatCount (7 - k), and each callback plays a sound. */
      float ft = t - 500;
      int k = ft < 0 ? -1 : ft < 17 ? 0 : ft < 117 ? 1 : 1 + (int)((ft - 17) / 100);
      if (k > 7) k = 7;
      if (k > B.step - 1) {
        ui_play_sound("battle_anims/PRSFX- Substitute2");
        B.step = k + 1;
      }
      if ((7 - k) % 2) sd->tint_alpha = 0;
      if (k < 7) return true;
      float t0 = 500 + 617;
      /* ("Sine.cubicEaseIn" isn't a Phaser ease name, so Phaser falls back to linear) */
      sd->tint_scale = 0.01f + (full - 0.01f) * (1 - seg(t, t0, 500, EASE_LINEAR));
      u = seg(t, t0 + 250, 500, EASE_SINE_IN_OUT);
      sd->sub_off = 1 - u;
      sd->mon_alpha = 0.5f + 0.5f * u;
      if (t < t0 + 750) return true;
      sd->tint_on = sd->mon.substitute = false;
      sd->sub_off = 0;
      sd->mon_alpha = 1;
      return false;
    }
    default: /* -1: the tag went while the Pokemon was in focus (sprite.destroy()) */
      sd->sub_on = sd->tint_on = sd->mon.substitute = false;
      sd->sub_off = 0;
      sd->mon_alpha = 1;
      return false;
  }
}

/* AttemptCapturePhase's animation (src/phases/attempt-capture-phase.ts, doPokeballBounceAnim in data/pokeball.ts), t in ms
 * since the throw. The ball flies to the enemy (500 ms), opens, the Pokemon is pulled in as a silhouette in the ball's colour
 * (500 ms), the ball closes, falls and bounces (1044 ms), then shakes: the shake counter's checks come at 1 s, 2 s and 3 s after
 * the bounce and each ball that holds wobbles for 500 ms after its check; a ball that holds past the third check locks at 4 s
 * and the catch is done at 4.5 s. A ball that breaks free at check k has wobbled k - 1 times: it opens, the Pokemon grows back
 * (250 ms) and the ball fades. The shake checks themselves are battle-core's: `b` says how many the ball passed. */
#define BOUNCE_MAX 20
typedef struct {
  float t0, dur, y0, y1;
  bool fall;
} bounce_seg;

/* doPokeballBounceAnim(ball, 16, 72, 350): falls to 72, rebounds to half the way up each time, until the power is gone. The ball
 * hangs at `top` (16 upstream; here the Pokemon's middle, see catch_anim) when it starts to fall. */
static int bounce_build(bounce_seg* sg, float* total, float top) {
  float power = 1, y = top, t = 0;
  int n = 0;
  for (;;) {
    sg[n++] = (bounce_seg){t, power * 350, y, 72, true};
    t += power * 350;
    power = power > 0.01f ? power * 0.5f : 0;
    if (power == 0 || n >= BOUNCE_MAX - 1) break;
    y = 72 - 56 * power;
    sg[n++] = (bounce_seg){t, power * 350, 72, y, false};
    t += power * 350;
  }
  *total = t;
  return n;
}

#define CATCH_THROW 500   /* the ball arrives and opens */
#define CATCH_IN 1000     /* the Pokemon is inside and the ball closes */
#define CATCH_BOUNCE 1250 /* the bounce starts 250 ms later */

enum {
  CS_THROW = 1, CS_OPEN = 2, CS_CLOSE = 4, CS_BOUNCE0 = 8, /* 8 falls: 8 .. 1024 */
  CS_MOVE0 = 1 << 11, /* 3 passed checks: 2^11 .. 2^13 */
  CS_LOCK = 1 << 14, CS_FAIL = 1 << 15, CS_FAIL_DONE = 1 << 16, CS_INIT = 1 << 20
};
#define ONCE(bit, key)          \
  do {                          \
    if (!(B.step & (bit))) {    \
      B.step |= (bit);          \
      ui_play_sound(key);       \
    }                           \
  } while (0)

/* utils randGauss(stdev): a normal deviate (Box-Muller); only looks, never the battle's random numbers */
static float rand_gauss(float stdev) {
  const float u = (rand() + 1.0f) / ((float)RAND_MAX + 2.0f), v = (rand() + 1.0f) / ((float)RAND_MAX + 2.0f);
  return stdev * sqrtf(-2 * logf(u)) * cosf(6.2831853f * v);
}

static bool catch_anim(side* sd, const bv_event* e, float t) {
  static const int BALL_RGB[5][3] = {{0xd5, 0x29, 0x29}, {0x94, 0xb4, 0xde}, {0xe6, 0xcd, 0x31}, {0xd5, 0x29, 0x29}, {0xa4, 0x41, 0xbd}};
  const int type = e->a >= 0 && e->a < 5 ? e->a : 0;
  const bool caught = e->caught;
  const int w = caught ? 3 : e->b < 0 ? 0 : e->b > 2 ? 2 : e->b; /* checks passed */
  if (!sd->present) return false;
  if (!(B.step & CS_INIT)) {
    B.step |= CS_INIT;
    memset(&B.ball, 0, sizeof B.ball);
    B.ball.on = true;
    B.ball.stars_t = -1;
    B.ball.type = type;
    B.ball.x = 16;
    B.ball.y = 80;
    B.ball.alpha = 1;
    /* Upstream throws at (236, 16) whatever the Pokemon is, so a small one is hit well below the ball, which hangs above it. The ball
     * is thrown at the middle of the Pokemon as it is drawn instead, and the Pokemon shrinks into it there. */
    {
      float mx, my;
      mon_pos(sd, e->battler < 2, &mx, &my);
      const float feet = my - foot_gap(sd) * sd->scale; /* the lowest drawn row: catch_dy moves it */
      const rr_frame* f = sprite_frame(sd);
      const float full = f ? f->h * get_mon_scale(&sd->mon) : 40;
      B.ball.tx = mx;
      B.ball.top = feet - full / 2;
      if (B.ball.top < 16) B.ball.top = 16;
      B.ball.end_dy = (B.ball.top + (f ? f->h * 0.25f : 10) / 2) - feet;
    }
    B.ball.sub = sd->sub_on; /* substitute.sprite.setVisible(false) */
    sd->sub_on = false;
    for (int i = 0; i < 3; i++) sd->catch_rgb[i] = BALL_RGB[type][i];
  }
  bounce_seg sg[BOUNCE_MAX];
  float btotal;
  const int nseg = bounce_build(sg, &btotal, B.ball.top);
  const float S = CATCH_BOUNCE + btotal; /* the shake counter starts */
  const float t_fail = S + (w + 1) * 1000.0f;
  const float mon_scale = get_mon_scale(&sd->mon);

  ONCE(CS_THROW, "se/pb_throw");
  if (t < CATCH_THROW) {
    const float u = t / CATCH_THROW;
    B.ball.x = 16 + (B.ball.tx - 16) * u;                    /* Linear */
    B.ball.y = 80 + (B.ball.top - 80) * ease(EASE_CUBIC_OUT, u);
    B.ball.behind = t >= 300;                                 /* moveBelow(pokeball, pokemon) */
    return true;
  }
  B.ball.behind = true;
  B.ball.x = B.ball.tx;
  if (t < CATCH_IN) {
    ONCE(CS_OPEN, "se/pb_rel");
    B.ball.y = B.ball.top;
    B.ball.look = t < CATCH_THROW + 17 ? 1 : 2;
    sd->catch_on = true;
    sd->catch_untint = 0;
    const float u = ease(EASE_SINE_IN, (t - CATCH_THROW) / (CATCH_IN - CATCH_THROW));
    sd->scale = mon_scale + (0.25f - mon_scale) * u;
    sd->catch_dy = B.ball.end_dy * u; /* upstream: y 20, from the field position 84 */
    return true;
  }
  if (t < t_fail || caught) {
    sd->scale = 0.25f;
    sd->catch_dy = B.ball.end_dy;
    sd->sprite_on = false; /* it is inside */
    ONCE(CS_CLOSE, "se/pb_catch");
    B.ball.look = t < CATCH_IN + 17 ? 1 : 0;
    /* the bounce */
    const float tb = t - CATCH_BOUNCE;
    B.ball.y = B.ball.top;
    if (tb >= 0) {
      B.ball.y = 72;
      for (int i = 0; i < nseg; i++) {
        if (tb < sg[i].t0 || tb >= sg[i].t0 + sg[i].dur) continue;
        const float u = (tb - sg[i].t0) / sg[i].dur;
        B.ball.y = sg[i].y0 + (sg[i].y1 - sg[i].y0) * ease(sg[i].fall ? EASE_CUBIC_IN : EASE_CUBIC_OUT, u);
      }
      int k = 0;
      for (int i = 0; i < nseg; i++) {
        if (!sg[i].fall) continue;
        const int bit = CS_BOUNCE0 << k++; /* (ONCE takes its bit twice) */
        if (k <= 8 && tb >= sg[i].t0 + sg[i].dur) ONCE(bit, "se/pb_bounce_1");
      }
    }
    /* the shakes */
    const float ts = t - S;
    B.ball.angle = 0;
    for (int k = 1; k <= w; k++) {
      if (ts < k * 1000.0f) continue;
      const int mbit = CS_MOVE0 << (k - 1);
      ONCE(mbit, "se/pb_move");
      const float local = ts - k * 1000.0f;
      if (local >= 500) continue;
      const float p = local < 250 ? local / 250 : 1 - (local - 250) / 250;
      const float v = ease(EASE_CUBIC_OUT, p) * (k % 2 ? 1 : -1);
      B.ball.x = B.ball.tx + v * 4;
      B.ball.angle = v * 27.5f;
    }
    if (caught) {
      if (ts >= 4000) {
        if (!(B.step & CS_LOCK))
          for (int k = 0; k < 3; k++) B.ball.star_dx[k] = rand_gauss(25);
        ONCE(CS_LOCK, "se/pb_lock");
        /* pbTint: a black copy fades in to 0.375 and out again, 200 ms each */
        const float f = ts - 4000;
        B.ball.stars_t = f;
        B.ball.flash = f < 200 ? 0.375f * ease(EASE_SINE_OUT, f / 200) : f < 400 ? 0.375f * (1 - ease(EASE_SINE_IN, (f - 200) / 200)) : 0;
      }
      if (ts < 4500) return true;
      B.ball.stars_t = -1;
      B.ball.flash = 0;
      B.ball.x = B.ball.tx;
      B.ball.y = 72;
      B.ball.angle = 0;
      return false;
    }
    return true;
  }
  /* failCatch: the ball opens, the Pokemon is back at its place and grows to its size, the tint fades off it */
  const float tf = t - t_fail;
  ONCE(CS_FAIL, "se/pb_rel");
  if (!(B.step & CS_FAIL_DONE)) {
    B.step |= CS_FAIL_DONE;
    if (sd->mon.status != STATUS_EFFECT_SLEEP) cry(&sd->mon);
    sd->sub_on = B.ball.sub;
    B.ball.fading = true;
    B.ball.fade_t = 0;
  }
  B.ball.x = B.ball.tx;
  B.ball.y = 72;
  B.ball.angle = 0;
  B.ball.look = tf < 17 ? 1 : 2;
  sd->sprite_on = true;
  sd->catch_dy = 0;
  const float u = ease(EASE_SINE_OUT, tf >= 250 ? 1 : tf / 250);
  sd->scale = 0.25f + (mon_scale - 0.25f) * u;
  sd->catch_untint = u;
  if (tf < 250) return true;
  sd->catch_on = false;
  sd->scale = mon_scale;
  return false;
}

static void run_event(float dt) {
  bv_event* e = &B.ev[B.cur];
  side* sd = side_of(e->battler);
  bool player = e->battler < 2;
  B.t += dt;
  switch (e->type) {
    case BEV_MESSAGE:
      if (B.step == 0) {
        msgbox_show(&B.msg, e->text ? e->text : "", -1, e->prompt);
        B.msg.wait_ms = e->a ? e->a : e->prompt ? 0 : 1500; /* callbackDelay || (prompt ? 0 : 1500) */
        B.step = 1;
      }
      if (msgbox_done(&B.msg)) finish_event();
      break;

    case BEV_LEVEL_UP:
      /* showLevelUpMessages: the fanfare with the text; its prompt's A opens the stat window (gains); A shows the totals and A
       * closes it (bv_input) */
      if (B.step == 0) {
        ui_play_sound("se/level_up_fanfare");
        msgbox_show(&B.msg, e->text ? e->text : "", -1, true);
        B.msg.wait_ms = 0;
        B.step = 1;
      }
      if (B.step == 1 && msgbox_done(&B.msg)) {
        if (!e->stats) {
          finish_event();
          break;
        }
        B.lvl.on = true;
        B.lvl.totals = false;
        B.lvl.stats = e->stats;
        B.step = 2;
      }
      if (B.step == 2 && !B.lvl.on) finish_event();
      break;

    case BEV_INTRO_WILD: {
      /* a = 1: a trainer battle, nobody on the enemy side yet and `text` is the message */
      const bool trainer = e->a == 1;
      if (B.step == 0) {
        side* en = &B.s[1];
        en->present = !trainer && en->mon.species > 0;
        en->black = en->present;
        en->info_on = false;
        tween_start(&B.tw, 0, 300, 2000, EASE_LINEAR);
        B.step = 1;
      }
      if (B.step == 1) {
        tween_update(&B.tw, dt);
        if (!B.setup.player_present) B.arena_player_x = 300 - B.tw.value; /* a run's first wave: the platform arrives under nobody yet */
        B.arena_enemy_x = -280 + B.tw.value;
        if (B.arena_old_on) B.arena_old_x = 20 + B.tw.value;
        B.enemy_slide = -300 + B.tw.value;
        if (tween_done(&B.tw)) {
          B.arena_old_on = false;
          side* en = &B.s[1];
          char s[256];
          if (trainer) {
            snprintf(s, sizeof s, "%s", e->text ? e->text : "");
            tween_start(&B.tw2, 0, 0, 0, EASE_LINEAR);
          } else {
            en->black = false; /* untint(100) */
            update_field_scale(false);
            cry(&en->mon);
            en->info_on = true;
            en->info_dx = -150;
            tween_start(&B.tw2, -150, 0, 1000, EASE_CUBIC_OUT);
            i18n_t(s, sizeof s, "battle:singleWildAppeared", "pokemonName", en->mon.name, NULL);
          }
          msgbox_show(&B.msg, s, -1, false);
          B.msg.wait_ms = 1500;
          B.step = 2;
        }
      }
      if (B.step == 2) {
        tween_update(&B.tw2, dt);
        if (!trainer) B.s[1].info_dx = B.tw2.value;
        if (msgbox_done(&B.msg) && tween_done(&B.tw2)) finish_event();
      }
      break;
    }

    case BEV_SUMMON:
      if (B.step == 0) {
        if (e->mon) set_mon(sd, e->mon, player);
        sd->present = true;
        sd->scale = 0;
        sd->info_on = false;
        if (player) {
          char s[128];
          if (e->text)
            snprintf(s, sizeof s, "%s", e->text);
          else
            i18n_t(s, sizeof s, "battle:playerGo", "pokemonName", sd->mon.name, NULL);
          msgbox_show(&B.msg, s, -1, false);
          B.msg.wait_ms = 0;
        }
        B.step = 1;
        B.t = 0;
      }
      if (B.step == 1 && B.t >= (player ? 750 : 250)) {
        /* the ball opens: the Pokemon grows in, cries, and its info box slides in */
        ui_play_sound("se/pb_rel");
        update_field_scale(false);
        tween_start(&B.tw, 0, get_mon_scale(&sd->mon), 250, EASE_SINE_IN);
        B.step = 2;
      }
      if (B.step == 2) {
        tween_update(&B.tw, dt);
        sd->scale = B.tw.value;
        if (tween_done(&B.tw)) {
          cry(&sd->mon);
          sd->info_on = true;
          sd->info_dx = player ? 150 : -150;
          tween_start(&B.tw2, sd->info_dx, 0, 1000, EASE_CUBIC_OUT);
          B.step = 3;
        }
      }
      if (B.step == 3) {
        tween_update(&B.tw2, dt);
        sd->info_dx = B.tw2.value;
        if (tween_done(&B.tw2)) finish_event();
      }
      break;

    case BEV_WITHDRAW:
      if (B.step == 0) {
        if (!player && B.ball.on && !B.ball.fading) B.ball.fading = true, B.ball.fade_t = 0; /* removePb() */
        tween_start(&B.tw, 1, 0, 250, EASE_SINE_IN);
        tween_start(&B.tw2, 0, player ? 150 : -150, 500, EASE_CUBIC_IN);
        B.step = 1;
      }
      tween_update(&B.tw, dt);
      tween_update(&B.tw2, dt);
      sd->scale = B.tw.value;
      sd->info_dx = B.tw2.value;
      if (tween_done(&B.tw) && tween_done(&B.tw2)) {
        sd->present = false;
        update_field_scale(false); /* ReturnPhase.onEnd */
        finish_event();
      }
      break;

    case BEV_MOVE_ANIM: {
      ba_mon field[4];
      anim_field(field);
      if (B.step == 0) {
        B.step = 1;
        if (!ba_play((ba_kind)e->b, e->a, e->battler, e->target, field)) {
          finish_event();
          break;
        }
      }
      ba_update(dt, field);
      if (!ba_active()) finish_event();
      break;
    }

    case BEV_DAMAGE:
      if (B.step == 0) {
        /* DamageAnimPhase. HitResult: 1 EFFECTIVE, 2 EXTREMELY_, 3 SUPER_, 4 NOT_VERY_, 5 MOSTLY_INEFFECTIVE,
         * 6 ONE_HIT_KO, 12 INDIRECT, 14 CONFUSION, 15 INDIRECT_KO. A KO waits 1 s first (upstream inverts
         * the colours meanwhile; not done here). */
        B.ko_wait = e->b == 6 || e->b == 15 ? 1000 : 0;
        B.step = 1;
        B.t = 0;
      }
      if (B.step == 1 && B.t >= B.ko_wait) {
        int r = e->b;
        ui_play_sound(r == 1 || r == 14                        ? "se/hit"
                      : r == 2 || r == 3 || r == 6 || r == 15  ? "se/hit_strong"
                      : r == 4 || r == 5                       ? "se/hit_weak"
                                                               : NULL);
        /* the sprite blinks (hidden, shown... six times 100 ms apart) unless indirect or no damage */
        B.blink = r != 12 && e->a < sd->mon.hp;
        B.step = 2;
        B.t = 0;
      }
      if (B.step == 2) {
        int k = (int)(B.t / 100);
        if (B.blink) sd->sprite_on = k % 2 == 1 || k >= 5;
        if (!B.blink || B.t >= 500) {
          sd->sprite_on = true;
          tween_start(&B.tw, sd->hp_shown, (float)e->a, hp_tween_ms(sd->hp_shown, (float)e->a), EASE_SINE_OUT);
          sd->mon.hp = e->a;
          B.step = 3;
        }
      }
      if (B.step == 3) {
        tween_update(&B.tw, dt);
        sd->hp_shown = B.tw.value;
        if (tween_done(&B.tw)) finish_event();
      }
      break;

    case BEV_HEAL:
      if (B.step == 0) {
        tween_start(&B.tw, sd->hp_shown, (float)e->a, hp_tween_ms(sd->hp_shown, (float)e->a), EASE_SINE_OUT);
        sd->mon.hp = e->a;
        B.step = 1;
      }
      tween_update(&B.tw, dt);
      sd->hp_shown = B.tw.value;
      if (tween_done(&B.tw)) finish_event();
      break;

    case BEV_STATUS:
      sd->mon.status = e->a;
      if (e->a > 0) {
        sd->status_banner.active = true;
        sd->status_banner.battler = e->battler;
        sd->status_banner.status = e->a;
        sd->status_banner.timer = 1000.0f;
        char fr[24];
        gm_enum_lower(GM_E_STATUS_EFFECT, e->a, fr, sizeof fr);
        for (char* p = fr; *p; p++) if (*p >= 'a' && *p <= 'z') *p -= 32;
        snprintf(sd->status_banner.text, sizeof sd->status_banner.text, "%s", fr);
      } else {
        sd->status_banner.active = false;
      }
      finish_event();
      break;

    case BEV_STAT_STAGES:
      if (B.step == 0) {
        ui_play_sound(e->b > 0 ? "se/stat_up" : "se/stat_down");
        if (e->a >= 1 && e->a <= 7) {
          int v = sd->mon.stat_stages[e->a - 1] + e->b;
          sd->mon.stat_stages[e->a - 1] = (int8_t)(v > 6 ? 6 : v < -6 ? -6 : v);
          sd->stat_banner.active = true;
          sd->stat_banner.battler = e->battler;
          sd->stat_banner.stat = e->a;
          sd->stat_banner.stage_delta = e->b;
          sd->stat_banner.new_stage = sd->mon.stat_stages[e->a - 1];
          sd->stat_banner.timer = 600.0f;
          static const char* const STAT_NAMES[] = {"", "ATK", "DEF", "SPATK", "SPDEF", "SPD", "ACC", "EVA"};
          snprintf(sd->stat_banner.text, sizeof sd->stat_banner.text, "%s %s%d",
                   STAT_NAMES[e->a], e->b > 0 ? "+" : "", e->b);
        }
        B.step = 1;
      }
      if (B.t >= 600) {
        sd->stat_banner.active = false;
        finish_event();
      }
      break;

    case BEV_ABILITY:
      if (B.step == 0) {
        char s[64];
        snprintf(s, sizeof s, "%s", i18n_ability_name(e->a));
        i18n_t(B.ability_text, sizeof B.ability_text, "fightUiHandler:abilityFlyInText", "pokemonName",
               sd->mon.name, "passive", "", "abilityName", s, NULL);
        for (char* p = B.ability_text; *p; p++)
          if (*p == '\n') *p = ' ';
        B.ability_t = 1500;
        B.step = 1;
      }
      if (B.t >= 500) finish_event();
      break;

    case BEV_FAINT:
      if (B.step == 0) {
        cry(&sd->mon);
        B.step = 1;
        B.t = 0;
      }
      if (B.step == 1 && B.t >= 250) {
        ui_play_sound("se/faint");
        tween_start(&B.tw, 0, 150, 500, EASE_SINE_IN);
        tween_start(&B.tw2, sd->info_dx, sd->info_dx + (player ? 150 : -150), 500, EASE_CUBIC_IN);
        B.step = 2;
      }
      if (B.step == 2) {
        tween_update(&B.tw, dt);
        tween_update(&B.tw2, dt);
        sd->drop = B.tw.value;
        sd->info_dx = B.tw2.value;
        if (tween_done(&B.tw) && tween_done(&B.tw2)) {
          sd->present = false;
          sd->info_on = false;
          finish_event();
        }
      }
      break;

    case BEV_EXP:
      if (B.step == 0) {
        float to = e->a / 1000.0f;
        if (e->b > 0) to = 1;
        tween_start(&B.tw, sd->exp_shown, to, 1650 * fabsf(to - sd->exp_shown), EASE_SINE_IN);
        if (B.tw.active) ui_play_sound("se/exp");
        B.step = 1;
      }
      if (B.step == 1) {
        tween_update(&B.tw, dt);
        sd->exp_shown = B.tw.value;
        if (tween_done(&B.tw)) {
          if (e->b > 0) {
            ui_play_sound("se/level_up");
            sd->mon.level = e->b;
            if (e->mon) {
              /* the level up raised the stats: the HP bar's numbers follow */
              sd->mon.max_hp = e->mon->max_hp;
              sd->mon.hp = e->mon->hp;
              sd->hp_shown = (float)e->mon->hp;
            }
            sd->exp_shown = 0;
            tween_start(&B.tw, 0, e->a / 1000.0f, 1650 * (e->a / 1000.0f), EASE_SINE_IN);
            B.step = 2;
          } else {
            sd->mon.exp_ratio = sd->exp_shown;
            finish_event();
          }
        }
      }
      if (B.step == 2) {
        tween_update(&B.tw, dt);
        sd->exp_shown = B.tw.value;
        if (tween_done(&B.tw)) {
          sd->mon.exp_ratio = sd->exp_shown;
          finish_event();
        }
      }
      break;

    case BEV_WAIT:
      if (B.t >= e->a) finish_event();
      break;

    case BEV_POKEMON_ANIM:
      if (!substitute_anim(sd, e->a, B.t)) finish_event();
      break;

    case BEV_VISIBLE:
      if (e->a) {
        /* onRemove: visible after getFrameMs(2), without holding up what follows */
        if (sd->mon.hidden) sd->show_ms = 2000 / 60.f;
      } else {
        sd->mon.hidden = true;
        sd->show_ms = 0;
      }
      finish_event();
      break;

    case BEV_MAX_HP:
      sd->mon.max_hp = e->a;
      finish_event();
      break;

    case BEV_CATCH:
      if (!catch_anim(sd, e, B.t)) finish_event();
      break;

    case BEV_FORM_CHANGE:
    case BEV_TRANSFORM:
    case BEV_ILLUSION:
      if (B.step == 0) {
        if ((e->type == BEV_TRANSFORM || e->type == BEV_ILLUSION) && e->mon) sd->mon = *e->mon;
        else update_mon_form(&sd->mon, e->a, e->b);
        if (e->type == BEV_FORM_CHANGE && e->c) {
          sd->mon.gender = e->c - 2;
          sd->mon.female = sd->mon.gender == 1;
        }
        load_mon_sprite(sd, player);
        float target_scale = get_mon_scale(&sd->mon);
        update_field_scale(false);
        if ((e->type != BEV_TRANSFORM && e->type != BEV_ILLUSION) || e->a) ui_play_sound("battle_anims/PRSFX- Transform");
        if (fabsf(sd->scale - target_scale) > 0.01f) {
          tween_start(&B.tw, sd->scale, target_scale, 300, EASE_SINE_IN_OUT);
          B.step = 1;
        } else {
          sd->scale = target_scale;
          finish_event();
          break;
        }
      }
      if (B.step == 1) {
        tween_update(&B.tw, dt);
        sd->scale = B.tw.value;
        if (tween_done(&B.tw)) finish_event();
      }
      break;
  }
}

/* ------------------------------------------------------------------ the screen */

static bool bv_input(ui_screen* s, ui_button b) {
  (void)s;
  if (B.mode == M_EVENTS && B.lvl.on) {
    /* promptLevelUpStats' onActionInput: the gains, then the totals, then the window closes */
    if (b != BTN_ACTION) return false;
    if (!B.lvl.totals) B.lvl.totals = true;
    else B.lvl.on = false;
    return true;
  }
  if (B.mode == M_EVENTS) return msgbox_input(&B.msg, b);
  if (B.mode == M_COMMAND) {
    int c = B.cmd_cursor;
    switch (b) {
      case BTN_UP:
        if (c >= 2) B.cmd_cursor -= 2;
        break;
      case BTN_DOWN:
        if (c < 2) B.cmd_cursor += 2;
        break;
      case BTN_LEFT:
        if (c % 2) B.cmd_cursor--;
        break;
      case BTN_RIGHT:
        if (!(c % 2)) B.cmd_cursor++;
        break;
      case BTN_ACTION:
        ui_play_select();
        if (c == 0) {
          if (B.opts.struggle) {
            /* every move is unusable: FIGHT uses Struggle (upstream's CommandPhase.handleFightCommand) */
            B.mode = M_IDLE;
            msgbox_clear(&B.msg);
            if (B.on_command) B.on_command((bv_command){0, 0}, B.command_arg);
            return true;
          }
          B.mode = M_FIGHT;
          B.reason_ms = 0;
          return true;
        }
        /* POKEMON always opens the party (upstream's CommandPhase does): with nobody to send in it is still the way to the summaries,
         * and a trapped Pokemon is refused when a member is picked, with the reason. opts.can_switch only says a switch is possible. */
        if ((c == 1 && !B.opts.can_ball) || (c == 3 && !B.opts.can_run)) {
          ui_play_error();
          return false;
        }
        if (c == 3 || c == 1 || c == 2) {
          /* BALL and POKEMON open the ball and party screens (the menus thread); RUN answers */
          B.mode = M_IDLE;
          msgbox_clear(&B.msg);
          if (B.on_command) B.on_command((bv_command){c, 0}, B.command_arg);
          return true;
        }
        return true;
      case BTN_MENU:
        if (!B.opts.on_menu) return false;
        B.opts.on_menu(B.command_arg);
        return true;
      default:
        return false;
    }
    if (B.cmd_cursor != c) ui_play_select();
    return B.cmd_cursor != c;
  }
  if (B.mode == M_FIGHT) {
    int c = B.move_cursor;
    switch (b) {
      case BTN_UP:
        if (c >= 2) B.move_cursor -= 2;
        break;
      case BTN_DOWN:
        if (c < 2) B.move_cursor += 2;
        break;
      case BTN_LEFT:
        if (c % 2) B.move_cursor--;
        break;
      case BTN_RIGHT:
        if (!(c % 2)) B.move_cursor++;
        break;
      case BTN_CANCEL:
        ui_play_select();
        B.mode = M_COMMAND;
        B.reason_ms = 0;
        return true;
      case BTN_ACTION: {
        const bv_mon* m = &B.s[0].mon;
        if (c >= m->n_moves || m->pp_left[c] <= 0 || !B.opts.move_ok[c]) {
          ui_play_error();
          B.reason[0] = 0;
          if (c < m->n_moves && B.opts.move_why[c]) {
            snprintf(B.reason, sizeof B.reason, "%s", B.opts.move_why[c]);
            B.reason_ms = 2500;
          }
          return false;
        }
        ui_play_select();
        B.mode = M_IDLE;
        msgbox_clear(&B.msg);
        if (B.on_command) B.on_command((bv_command){0, c}, B.command_arg);
        return true;
      }
      default:
        return false;
    }
    if (B.move_cursor != c) ui_play_select();
    return B.move_cursor != c;
  }
  return false;
}

static void bv_update(ui_screen* s, float dt) {
  (void)s;
  float frame_dt = dt;
  msgbox_update(&B.msg, dt);
  if (B.ability_t > 0) B.ability_t -= dt;
  if (B.reason_ms > 0) B.reason_ms -= dt;
  if (!tween_done(&B.field_tw)) {
    tween_update(&B.field_tw, dt);
    B.field_scale = B.field_tw.value;
  }
  if (B.ball.on && B.ball.fading) {
    /* removePb(): delay 250, Sine.easeIn to alpha 0 over 250 */
    B.ball.fade_t += dt;
    B.ball.alpha = B.ball.fade_t < 250 ? 1 : 1 - ease(EASE_SINE_IN, (B.ball.fade_t - 250) / 250);
    if (B.ball.fade_t >= 500) B.ball.on = false;
  }
  for (int k = 0; k < 2; k++) {
    if (B.s[k].show_ms > 0 && (B.s[k].show_ms -= dt) <= 0) {
      B.s[k].show_ms = 0;
      B.s[k].mon.hidden = false;
    }
    if (B.s[k].stat_banner.active) {
      B.s[k].stat_banner.timer -= dt;
      if (B.s[k].stat_banner.timer <= 0) {
        B.s[k].stat_banner.active = false;
        B.s[k].stat_banner.timer = 0;
      }
    }
    if (B.s[k].status_banner.active) {
      B.s[k].status_banner.timer -= dt;
      if (B.s[k].status_banner.timer <= 0) {
        B.s[k].status_banner.active = false;
        B.s[k].status_banner.timer = 0;
      }
    }
  }
  if (B.mode == M_EVENTS) {
    while (B.cur < B.n_ev) {
      int before = B.cur;
      run_event(dt);
      dt = 0; /* only the first event this frame gets the time */
      if (B.cur == before) break;
    }
    if (B.cur >= B.n_ev) {
      B.mode = M_IDLE;
      if (B.on_done) {
        void (*fn)(void*) = B.on_done;
        B.on_done = NULL;
        fn(B.done_arg);
      }
    }
  }
  B.fps_frames++;
  B.fps_t += frame_dt;
  if (B.fps_t >= 1000) {
    B.fps = B.fps_frames * 1000.0f / B.fps_t;
    B.fps_frames = 0;
    B.fps_t = 0;
  }
}

static void bv_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP) {
    if (ui_space(UI_FIELD)) draw_field();
    if (ui_space(UI_FIELD_UI)) {
      draw_enemy_info(&B.s[1]);
      draw_player_info(&B.s[0]);
    }
    if (ui_space(UI_TOP_CANVAS_UI)) draw_bottom(true);
    /* the field is 320x180 on a 400x240 screen: keep sprites that slide past it off the border */
    if (ui_space(UI_PHYS_TOP)) {
      rr_rect(0, 0, 40, 240, RR_RGBA(0, 0, 0, 255));
      rr_rect(360, 0, 40, 240, RR_RGBA(0, 0, 0, 255));
      rr_rect(0, 0, 400, 30, RR_RGBA(0, 0, 0, 255));
      rr_rect(0, 210, 400, 30, RR_RGBA(0, 0, 0, 255));
    }
  } else {
    if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
    if (ui_space(UI_BOTTOM_UI)) draw_bottom(false);
    draw_debug();
  }
}

static ui_screen g_battle = {"battle", 0, NULL, NULL, NULL, bv_input, bv_update, bv_draw, NULL};

void bv_begin(const bv_setup* setup) {
  menu_sprite_free(&B.s[0].sprite);
  menu_sprite_free(&B.s[1].sprite);
  memset(&B, 0, sizeof B);
  B.setup = *setup;
  gm_enum_lower(GM_E_BIOME_ID, setup->biome, B.biome, sizeof B.biome);
  set_mon(&B.s[0], &setup->player, true);
  set_mon(&B.s[1], &setup->enemy, false);
  B.begun = true;
  B.arena_player_x = setup->player_present ? 0 : 300;
  B.arena_enemy_x = -280;
  B.enemy_slide = -300;
  B.arena_old_on = setup->next_encounter;
  B.arena_old_x = 20;
  B.s[0].present = B.s[0].info_on = setup->player_present;
  B.s[1].present = B.s[1].info_on = false;
  B.field_scale = 1;
  update_field_scale(true);
  for (int i = 0; i < 4; i++) B.opts.move_ok[i] = true;
  B.opts.can_ball = B.opts.can_switch = B.opts.can_run = true;
  msgbox_init(&B.msg, TS_MESSAGE, 1780 / 6.0f, 2);
  B.mode = M_IDLE;
  ui_set(&g_battle);
}

void bv_play(const bv_event* ev, int n, void (*on_done)(void*), void* arg) {
  B.n_ev = 0;
  B.cur = 0;
  B.step = 0;
  B.t = 0;
  for (int i = 0; i < n && i < EV_MAX; i++) {
    B.ev[i] = ev[i];
    if (ev[i].text) {
      snprintf(B.ev_text[i], EV_TEXT, "%s", ev[i].text);
      B.ev[i].text = B.ev_text[i];
    }
    if (ev[i].mon) {
      B.ev_mon[i] = *ev[i].mon;
      B.ev[i].mon = &B.ev_mon[i];
    }
    B.n_ev++;
  }
  B.on_done = on_done;
  B.done_arg = arg;
  B.mode = M_EVENTS;
  B.msg.opts.wrap_w = 1780 / 6.0f;
}

bool bv_playing(void) { return B.mode == M_EVENTS; }

void bv_ask_command(const bv_command_opts* opts, void (*on_command)(bv_command, void*), void* arg) {
  B.on_command = on_command;
  B.command_arg = arg;
  if (opts) {
    B.opts = *opts;
  } else {
    memset(&B.opts, 0, sizeof B.opts);
    B.opts.can_ball = B.opts.can_switch = B.opts.can_run = true;
    for (int i = 0; i < 4; i++) B.opts.move_ok[i] = true;
  }
  B.n_foe = 0;
  if (opts && opts->foe_items)
    for (int i = 0; i < opts->n_foe_items && B.n_foe < BV_MAX_FOE_ITEMS; i++) B.foe[B.n_foe++] = opts->foe_items[i];
  B.opts.foe_items = NULL;
  B.reason_ms = 0;
  B.mode = M_COMMAND;
  if (B.cmd_cursor == 2) B.cmd_cursor = 0; /* upstream resets POKEMON to FIGHT */
  /* "What will X do?", wrapped to the space left of the command window (1110 / 6) */
  B.msg.opts.wrap_w = 1110 / 6.0f;
  char s[128];
  i18n_t(s, sizeof s, "commandUiHandler:actionMessage", "pokemonName", B.s[0].mon.name, NULL);
  msgbox_show(&B.msg, s, 0, false);
  B.msg.wait_ms = 1e9f;
  (void)name_with_affix;
}
