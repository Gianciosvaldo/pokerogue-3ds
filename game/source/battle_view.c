/* The battle screen (see battle_view.h). Positions are upstream's: the field container for the
 * arena and sprites, fieldUI for the info boxes, the ui container for the bottom strip. */
#include "battle_view.h"
#include "dream_anim.h"
#include "luck_meter.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "battle_anim.h"
#include "char_sprite.h"
#include "dialogue_view.h"
#include "field_border.h"
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
#define BV_DLG_MAX 2 /* dialogues one chunk of events can hold */

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
  float sparkle_ms; /* 0 inactive; native atlas: 35 frames at 32 fps */
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
  /* Terastallized (PokemonSpriteTeraSparkleHandler): a sparkle on a random opaque pixel every 400 ms, each playing the 13 frames of tera_sparkle at
   * 18 fps. x, y are px from the Pokemon's feet; ms since it started */
  float tera_next_ms;
  struct { bool on; float x, y, age; } tera_sp[4];
} side;

typedef enum { M_IDLE, M_EVENTS, M_COMMAND, M_FIGHT, M_TARGET } mode;

static struct {
  bv_setup setup;
  char biome[32];
  bool stats_shown;
  float stats_alpha;
  tween stats_tw;
  side s[4]; /* primary player/enemy, secondary player/enemy */
  bool late_double;   /* BEV_DOUBLES: the player's side went to two columns in the middle of the fight (the final boss' second phase) while
                       * the foe's side did not: setup.doubles says both sides are doubles from the start */
  float split;        /* how far the lead has stepped from the centre to its left place (0..1), tweened by BEV_DOUBLES */
  bool centred;       /* a double in which only one of the player's Pokemon can fight: it stands in the centre and its info box is the single
                       * battle's (bv_setup.player_centred from the start, or BEV_PLAYER_CENTRE once a faint has left it alone) */
  float centre;       /* how far the player's Pokemon are from their columns to the centre (0..1), tweened by BEV_PLAYER_CENTRE */
  bool flipped;       /* BEV_FIELD_SWAP in two columns: the survivor of slot 1 became slot 0 but keeps standing (and its box) where it was until
                       * BEV_PLAYER_CENTRE moves it, so a swap in the middle of a turn is not seen */
  float arena_player_x, arena_enemy_x, enemy_slide;
  float arena_old_x;  /* the previous wave's enemy platform, sliding out (setup.next_encounter) */
  bool arena_old_on;
  bool begun;         /* bv_begin has run */
  float field_scale;  /* the field container's scale (updateFieldScale), tweened by field_tw */
  tween field_tw;
  msgbox msg;
  float msg_wrap;
  mode mode;
  int cmd_cursor, move_cursor;   /* cmd_cursor 4 = the Tera button (upstream Command.TERA) */
  bool tera_fight;               /* the fight menu was opened from the Tera button: a move picked is a Terastallize */
  struct {                       /* bv_ask_target (TargetSelectUiHandler) */
    bool ok[4];                  /* the targets, by field index */
    int slot;                    /* the attacker's field index */
    int cursor;                  /* the field index pointed at */
    float t;                     /* ms since the cursor last moved: the flash's and the box's phase (setCursor restarts both tweens) */
    int last[2];                 /* cursor0 / cursor1: what each slot picked last; a player index (0, 1) means none, so 0 from a memset is none */
    char title[48];
    void (*on_target)(int, void*);
    void* arg;
  } tg;
  /* events */
  bv_event ev[EV_MAX];
  char ev_text[EV_MAX][EV_TEXT];
  bv_mon ev_mon[EV_MAX];
  int n_ev, cur;
  int step;       /* the current event's stage */
  float t;        /* ms in the current stage */
  float form_scale; /* step_quiet_form: the sprite's scale as the change began */
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
  float heaven_box_t;     /* ms the Made in Heaven box stays up (its text is ability_text) */
  struct { int stage; float t; bool swapped; } heaven;   /* BEV_HEAVEN's own steps (step_dialogue owns B.step) */
  float heaven_dark;      /* 0..1: the field fades to black and back while the arena becomes space */
  struct {                /* BEV_WORLD (DIO's time stop) */
    int stage;            /* the event's own steps (step_dialogue owns B.step) */
    float t;
    bool on;              /* DIO's second sprite is drawn */
    float slide;          /* 0 off screen at the right .. 1 beside his Pokemon */
    float inv_r;          /* the inverted circle's radius (field px), 0 = none */
    float bulge;          /* the background's extra zoom about DIO while the circle grows */
    float grey;           /* 0..1: the touched layers in grey blue */
    float hold;           /* ms the grey stage waits past its own 300 for the time-stop sound to end */
    char key[64];         /* his sprite */
  } world;
  struct {                /* BEV_PERFECT (Kars's Perfect Being) */
    int stage;
    float t;
    bool on;              /* his second sprite is drawn beside his Pokemon */
    float slide;          /* 0 off screen at the right .. 1 beside his Pokemon */
    char key[64];         /* his sprite */
    char line[160];       /* the line on screen */
  } perfect;
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
  bv_item key[BV_MAX_FOE_ITEMS]; /* the player's own items that no Pokemon holds (Mega Bracelet, Dynamax Band...) */
  int n_key;
  int bar;       /* which item bar is showing: 0 the foe's, 1 the player's (L and R) */
  int bar_sel;   /* the item the cursor is on (Up from the top row or Down from the bottom row of the command menu), -1 = none */
  bool bar_open; /* A opened its description (in the message window) */
  bool items_on;  /* the standard view's item panel is open (its Items button, L / R or Up from FIGHT); bar_sel is its cursor */
  bool hold_cmd; /* BALL or POKEMON was chosen and its screen is open: the command strip, the item bar and the tray stay as they were */
  /* an item's description the summary screen asks for (bv_set_info), shown in the message window while no command is asked */
  bool info_on;
  char info_name[64], info_desc[512];
  bool trainer_visible;
  me_intro_sprite me_sp[ME_MAX_INTRO];   /* bv_me_intro */
  int me_n;
  float trainer_show_ms; /* BEV_TRAINER_SHOW: ms into the defeated trainer's slide back in, 0 = not sliding */
  bool player_trainer_visible, opening_summon;
  /* BEV_DIALOGUE: the talk's copies of its strings (one per event of a chunk), the darkening of the field behind the portrait
   * (upstream's fieldOverlay) and the name box that sits on the message window while someone talks */
  char dlg_text[BV_DLG_MAX][BV_DIALOGUE_TEXT];
  char dlg_name[BV_DLG_MAX][48];
  char dlg_atlas[BV_DLG_MAX][64];
  int n_dlg;
  float overlay;
  tween overlay_tw;
  const char* speaker;   /* the name box's text while someone talks, NULL = hidden */
  float player_throw_ms; /* -1 before the opening SummonPhase */
  bool has_balls;
  uint8_t balls[6]; /* the party tray: BV_BALL_* of each slot */
  /* promptLevelUpStats' window (BEV_LEVEL_UP) */
  struct {
    bool on, totals, ivs;
    const int* stats;
  } lvl;
} B;

/* the command menu is on screen: asked, or answered with BALL / POKEMON and waiting for that screen */
static bool cmd_shown(void) { return B.mode == M_COMMAND || (B.mode == M_IDLE && B.hold_cmd); }

static int field_of(const side *sd){int i=(int)(sd-B.s);return (i%2)*2+i/2;}
static side* side_of(int battler) { if(battler<0||battler>=4)return &B.s[0];return &B.s[(battler%2)*2+(battler>=2)]; }
static side* command_side(void){return side_of(B.opts.command_slot==1?1:0);}
bv_mon* bv_mon_of(int battler) { return &side_of(battler)->mon; }
int bv_biome(void) { return B.begun ? B.setup.biome : -1; }

/* ------------------------------------------------------------------ the target choice (TargetSelectUiHandler) */

/* The first target on a side in upstream's getMoveTargets order (the foes in field order, then the user, then its ally): the lowest field
 * index of that side, -1 when the side has none */
static int tg_first(bool foes) {
  for (int f = foes ? 2 : 0; f < (foes ? 4 : 2); f++)
    if (B.tg.ok[f]) return f;
  return -1;
}
/* resetCursor: the slot's last pick if it was a foe that is still a target, else targets[0] */
static int tg_default(void) {
  const int last = B.tg.last[B.tg.slot & 1];
  if (last >= 2 && last < 4 && B.tg.ok[last]) return last;
  const int foe = tg_first(true);
  return foe >= 0 ? foe : tg_first(false);
}
static void tg_set(int field) {
  B.tg.cursor = field;
  B.tg.t = 0;
}
/* targetFlashTween: alpha 1 -> 0.25, Sine.easeInOut (Phaser: 0.5 * (1 - cos(pi v))), 450 ms, yoyo, loopDelay 150 */
static float tg_sine_in_out(float v) { return 0.5f * (1 - cosf(3.14159265f * v)); }
static float tg_flash(float t) {
  const float p = fmodf(t, 1050);
  if (p < 450) return 1 - 0.75f * tg_sine_in_out(p / 450);
  if (p < 900) return 0.25f + 0.75f * tg_sine_in_out((p - 450) / 450);
  return 1;
}
/* targetBattleInfoMoveTween: y base -> base + 1, Linear, 250 ms, yoyo, no rest */
static float tg_bob(float t) {
  const float p = fmodf(t, 500);
  return p < 250 ? p / 250 : (500 - p) / 250;
}
static bool tg_pointed(const side* sd) { return B.mode == M_TARGET && field_of(sd) == B.tg.cursor; }
static float target_bob(const side* sd) { return tg_pointed(sd) ? tg_bob(B.tg.t) : 0; }

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

/* Sprite sheets that did not load even after everything idle was freed (the stand-in box shows): how many, and the last one, for the debug line */
static int g_sprite_fails, g_sprite_fail_species;
static bool g_sprite_fail_back;

static void load_mon_sprite(side* sd, bool player) {
  const bv_mon* m = &sd->mon;
  menu_sprite_free(&sd->sprite);
  menu_mon visual = {.species = m->species, .form = m->form, .gender = m->gender,
                     .shiny = m->shiny, .variant = m->variant};
  if (m->fusion_species) {
    visual.fusion = true;
    visual.fusion_species = (uint16_t)m->fusion_species;
    visual.fusion_form = (uint8_t)m->fusion_form;
    visual.fusion_gender = (int8_t)m->fusion_gender;
    visual.fusion_shiny = m->fusion_shiny;
    visual.fusion_variant = (uint8_t)m->fusion_variant;
  }
  sd->sprite_key[0]=0;
  if(!m->species)return;
  /* The shared loader owns the atlas. Do not load a second copy into img's cache. */
  if(menu_sprite_load(&sd->sprite,&visual,player))return;
  /* A sheet can be 1 to 3 MB in one piece, and texture memory may hold the idle animation images: free them (they load again when a
   * move plays) and try once more before showing the stand-in box. */
  if(ba_release_graphics()&&menu_sprite_load(&sd->sprite,&visual,player))return;
  /* Then the menus' idle Pokemon icon sheets (the party screen, the shop, the starter select leave up to a dozen MB of them loaded; they
   * load again the next time a screen draws an icon), and once more. */
  if(mui_reclaim_images()>0&&menu_sprite_load(&sd->sprite,&visual,player))return;
  g_sprite_fails++;
  g_sprite_fail_species = m->species;
  g_sprite_fail_back = player;
  sprite_key(m,player,sd->sprite_key,sizeof sd->sprite_key);
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
  ba_show(field_of(sd));
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
  if(!sd->sprite_key[0])return NULL;
  int frame = (int)(ui_now() * 10 / 1000); /* upstream anims run at 10 fps */
  return img_anim_frame(sd->sprite_key, frame);
}

/* The field as battle animations see it: where each Pokemon stands and its current frame. */
static void mon_pos(const side* sd, bool player, float* x, float* y);

static void anim_field(ba_mon out[4]) {
  memset(out, 0, 4 * sizeof *out);
  for (int k = 0; k < 4; k++) {
    side* sd = &B.s[k];
    ba_mon* m = &out[field_of(sd)];
    m->on_field = sd->present;
    m->player = !(k%2);
    mon_pos(sd, !(k%2), &m->x, &m->y);
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

/* Whether the player's side stands in two columns: a doubles encounter, or the final boss' second phase (BEV_DOUBLES), which turns a
 * single fight into a double with the one foe where it was. The foe's side follows setup.doubles alone. */
static bool player_doubles(void) { return (B.setup.doubles || B.late_double) && !B.centred; }
/* How far the player's Pokemon are from the centre towards their column: 1 in a doubles encounter, BEV_DOUBLES' tween in the middle of one;
 * BEV_PLAYER_CENTRE takes it back to 0 when only one of them can fight */
static float player_split(void) { return (B.setup.doubles ? 1.0f : B.late_double ? B.split : 0.0f) * (1.0f - B.centre); }
static bool second_column(const side *sd){return field_of(sd)%2!=0;}
/* The column a player's Pokemon stands in (and its box): its slot's, except after a BEV_FIELD_SWAP that has not been followed by the move to the centre */
static bool player_right(const side *sd){return second_column(sd)!=B.flipped;}
static bool player_mini(const side *sd){(void)sd;return player_doubles();}

/* BEV_DOUBLES, one frame of it; true when it is over. Pokemon.setFieldPosition(LEFT, 500): the boxes change at once (setMini, setOffset) and the
 * sprite slides over 500 ms, Sine.easeOut. A screen that already has two player columns has nothing to do. */
static void (*music_hook)(int a);   /* where BEV_MUSIC goes */
/* The battle screen's two hotkeys (lorenzo, 2026-11-16): X changes what the bottom screen shows and Y steps the game speed (the app's hook).
 * The view and the held keys are statics, not B, which bv_begin clears: the view stays through the waves. The debug view is the screen as it
 * was (the strip and the debug rows, the item bar on the top screen); the standard one is lorenzo's Black 2 design (2026-11-17): a strip with the
 * wave, biome, money and battery, and the command screen as big buttons over the whole bottom screen (the "standard view" block). A held key repeats: the repeats are swallowed until its release. */
typedef enum { BV_VIEW_DEBUG, BV_VIEW_STANDARD, BV_VIEW_COUNT } bottom_view;
/* The standard view is lorenzo's Black 2 design (2026-11-17) and the default; X still reaches the debug view. */
static bottom_view g_view = BV_VIEW_STANDARD;
static bool sv(void) { return g_view == BV_VIEW_STANDARD; }
static bool g_view_held, g_speed_held;
static const char* (*speed_hook)(bool step);
static bool step_doubles(float dt) {
  if (B.step == 0) {
    if (B.setup.doubles || B.late_double) return true;
    B.late_double = true;
    B.split = 0;
    tween_start(&B.tw, 0, 1, 500, EASE_SINE_OUT);
    B.step = 1;
  }
  tween_update(&B.tw, dt);
  B.split = B.tw.value;
  return tween_done(&B.tw);
}

/* BEV_PLAYER_CENTRE, one frame of it; true when it is over. Pokemon.setFieldPosition(CENTER or LEFT, 500): the boxes change at once (setMini,
 * setOffset), the sprite slides over 500 ms, Sine.easeOut. Going to the centre it starts from where the Pokemon stands (a survivor of slot 1 that a
 * BEV_FIELD_SWAP promoted still stands in the right column); a Pokemon that takes its column again does so from the centre. Nothing to do on a screen
 * that is already there (the swap's hold, if any, ends at once). */
static bool step_centre(float dt, bool centre, bool at_once) {
  if (B.step == 0) {
    if (B.centred == centre) {
      B.flipped = false;
      return true;
    }
    B.centred = centre;
    if (!centre || at_once) B.flipped = false;
    if (at_once) {
      B.centre = centre ? 1.0f : 0.0f;
      return true;
    }
    tween_start(&B.tw, B.centre, centre ? 1.0f : 0.0f, 500, EASE_SINE_OUT);
    B.step = 1;
  }
  tween_update(&B.tw, dt);
  B.centre = B.tw.value;
  if (!tween_done(&B.tw)) return false;
  B.flipped = false;
  return true;
}

/* Doubles: the whole foe side (its platform, trainers and Pokemon) stands FOE_RAISE field pixels higher than in a single battle. The
 * player's boxes are upstream's mini ones in a double (field y 77 and down), so the foes clear them with no raise at all: they keep the
 * singles platform and stand LEFT (-32, -8) and RIGHT (+32, 0) of its mark as upstream's do. Raised, the Pokemon float over a platform left
 * behind and the tallest are cut off by the top of the screen (lorenzo, 2026-11-09, twice), so what is raised goes up together: the
 * platform, the trainers and the Pokemon (y grows downwards: the platform's y is minus the raise). */
#define FOE_RAISE 0.0f
static float foe_raise_px = FOE_RAISE;   /* (a variable so the host test can run the raise's geometry with a raise that is not 0) */
static float foe_raise(void) { return B.setup.doubles ? foe_raise_px : 0; }

/* Where a side's Pokemon stands in its column: the field position alone, before a Substitute, a catch or the sprite's empty rows move it. */
static void field_pos(const side* sd, bool player, float* x, float* y) {
  *x = player ? 106 : 236 + B.enemy_slide;
  *y = player ? PLAYER_Y : 84 - foe_raise();
  const bool right = field_of(sd) % 2 != 0;
  if (player) {
    const float k = player_split(); /* FieldPosition.LEFT is (-32, -8), RIGHT (32, 0) */
    const bool pright = player_right(sd);
    *x += (pright ? 32 : -32) * k;
    *y += pright ? 0 : -8 * k;
  } else if (B.setup.doubles) {
    *x += right ? 32 : -32;
    *y += right ? 0 : -8;
  }
}

static void mon_pos(const side* sd, bool player, float* x, float* y) {
  field_pos(sd, player, x, y);
  *x += sd->sub_off * (player ? -30 : 30);
  *y += sd->sub_off * (player ? 10 : -10) + sd->catch_dy + foot_gap(sd) * sd->scale;
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
  float x, y;
  field_pos(sd, player, &x, &y);
  rr_draw(f, x, y, &o);
  if (tint) rr_set_tone(0, 0, 0);
}

/* The cached sparkle atlases are transient. Keep them until every overlapping
 * animation finishes, then return their texture memory to Pokemon/move art. */
static void update_sparkles(float dt) {
  bool had=false,active=false;
  for(int k=0;k<4;k++)if(B.s[k].sparkle_ms>0){
    had=true;B.s[k].sparkle_ms+=dt;
    if(B.s[k].sparkle_ms>=35000.0f/32)B.s[k].sparkle_ms=0;
    else active=true;
  }
  if(had&&!active)img_drop_prefix("images/effects/shiny");
}

static void draw_sparkle(const side* sd,bool player) {
  if(sd->sparkle_ms<=0||!sd->present||sd->mon.hidden||sd->scale<=0)return;
  int frame=(int)(sd->sparkle_ms*32/1000);if(frame>=35)return;
  char atlas[48],name[16];int variant=sd->mon.variant;
  if(variant>=1&&variant<=2)snprintf(atlas,sizeof atlas,"images/effects/shiny_%d",variant+1);else snprintf(atlas,sizeof atlas,"images/effects/shiny");
  snprintf(name,sizeof name,"%d.png",frame);const rr_frame*f=img_frame(atlas,name);
  if(!f){snprintf(name,sizeof name,"%d",frame);f=img_frame(atlas,name);}
  if(!f)return;
  float x,y;mon_pos(sd,player,&x,&y);rr_draw_opts o=RR_DEFAULT_OPTS;o.origin_x=.5f;o.origin_y=1;o.scale_x=o.scale_y=sd->scale;rr_draw(f,x,y,&o);
}

/* getTypeRgb (data/type.ts): the colour of a type's Tera crystal, which also fills the Tera icon beside the name. Stellar is white. */
static void tera_type_rgb(int type, int rgb[3]) {
  static const uint8_t RGB[19][3] = {
      {168, 168, 120}, {192, 48, 40},   {168, 144, 240}, {160, 64, 160},  {224, 192, 104}, {184, 160, 56}, {168, 184, 32},
      {112, 88, 152},  {184, 184, 208}, {240, 128, 48},  {104, 144, 240}, {120, 200, 80},  {248, 208, 48}, {248, 88, 136},
      {152, 216, 216}, {112, 56, 248},  {112, 88, 72},   {232, 136, 200}, {255, 255, 255}};
  const int t = type >= 0 && type < 19 ? type : -1;
  for (int i = 0; i < 3; i++) rgb[i] = t < 0 ? 0 : RGB[t][i];
}

/* The Terastallized look. Upstream's sprite shader pulls the sprite's hue 62.5% toward the Tera type's colour (keeping its light and
 * saturation) and lays an animated crystal pattern over it; the 3DS draws the sprite through a multiply tint that does the first (the type's
 * colour at full brightness, 70% of the way from white) and a slow additive pulse of the same colour over the sprite's shape for the second.
 * Stellar (white) has no colour to pull toward: its pulse cycles the hue. */
#define TERA_PULSE_MS 1400.0f
static void tera_colour(int type, int rgb[3]) {
  if (type == POKEMON_TYPE_STELLAR) {
    /* full-saturation HSV, the hue turning once every 2 s */
    const float h = fmodf(ui_now() / 2000.0f, 1.0f) * 6, f = h - floorf(h);
    float c[3];
    switch ((int)h % 6) {
      case 0: c[0] = 1, c[1] = f, c[2] = 0; break;
      case 1: c[0] = 1 - f, c[1] = 1, c[2] = 0; break;
      case 2: c[0] = 0, c[1] = 1, c[2] = f; break;
      case 3: c[0] = 0, c[1] = 1 - f, c[2] = 1; break;
      case 4: c[0] = f, c[1] = 0, c[2] = 1; break;
      default: c[0] = 1, c[1] = 0, c[2] = 1 - f; break;
    }
    for (int i = 0; i < 3; i++) rgb[i] = (int)(c[i] * 255 + 0.5f);
    return;
  }
  tera_type_rgb(type, rgb);
}

static rr_color tera_tint(int type, int alpha) {
  int rgb[3];
  tera_type_rgb(type, rgb);
  const int top = rgb[0] > rgb[1] ? (rgb[0] > rgb[2] ? rgb[0] : rgb[2]) : (rgb[1] > rgb[2] ? rgb[1] : rgb[2]);
  if (type == POKEMON_TYPE_STELLAR || top <= 0) return RR_RGBA(255, 255, 255, alpha);
  int c[3];
  for (int i = 0; i < 3; i++) c[i] = (int)(255 - 0.7f * (255 - rgb[i] * 255.0f / top) + 0.5f);
  return RR_RGBA(c[0], c[1], c[2], alpha);
}

/* the pulse over a Terastallized sprite (or the selected Tera button): the frame again, tinted by the colour and added, 0 to 0.3 alpha */
static void draw_tera_pulse(const rr_frame* f, float x, float y, const rr_draw_opts* base, int type) {
  int rgb[3];
  tera_colour(type, rgb);
  const float phase = fmodf(ui_now(), TERA_PULSE_MS) / TERA_PULSE_MS;
  const float k = 0.5f - 0.5f * cosf(phase * 6.2831853f);
  rr_draw_opts o = *base;
  o.tint = RR_RGBA(rgb[0], rgb[1], rgb[2], (int)(k * 77 + 0.5f));
  rr_set_blend(RR_BLEND_ADD);
  rr_draw(f, x, y, &o);
  rr_set_blend(RR_BLEND_NORMAL);
}

static void update_tera(float dt) {
  for (int k = 0; k < 4; k++) {
    side* sd = &B.s[k];
    for (int i = 0; i < 4; i++)
      if (sd->tera_sp[i].on && (sd->tera_sp[i].age += dt) >= 1000.0f * 13 / 18) sd->tera_sp[i].on = false;
    if (sd->mon.terastallized && sd->present) sd->tera_next_ms -= dt;
    else sd->tera_next_ms = 0;
  }
}

/* onLapse: a random pixel of the frame; a sparkle only where it is opaque */
static void tera_spawn(side* sd, const rr_frame* f, float scale) {
  if (sd->tera_next_ms > 0 || !sd->mon.terastallized) return;
  sd->tera_next_ms += 400;
  if (sd->tera_next_ms <= 0) sd->tera_next_ms = 400;
  static unsigned seed = 0x9e3779b9u;
  seed = seed * 1664525u + 1013904223u + (unsigned)ui_now();
  const int px = (int)((seed >> 8) % (unsigned)f->src_w);
  seed = seed * 1664525u + 1013904223u;
  const int py = (int)((seed >> 8) % (unsigned)f->src_h);
  if (!rr_frame_alpha(f, px, py)) return;
  for (int i = 0; i < 4; i++)
    if (!sd->tera_sp[i].on) {
      sd->tera_sp[i].on = true;
      sd->tera_sp[i].x = (px - f->src_w * 0.5f) * scale;
      sd->tera_sp[i].y = (py - f->src_h) * scale;
      sd->tera_sp[i].age = 0;
      return;
    }
}

static void draw_tera_sparkles(const side* sd, bool player) {
  if (!sd->present || sd->mon.hidden || sd->scale <= 0) return;
  float x, y;
  mon_pos(sd, player, &x, &y);
  for (int i = 0; i < 4; i++) {
    if (!sd->tera_sp[i].on) continue;
    char name[8];
    snprintf(name, sizeof name, "%d", (int)(sd->tera_sp[i].age * 18 / 1000));
    const rr_frame* f = img_frame("images/effects/tera_sparkle", name);
    if (!f) continue;
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_x = o.origin_y = 0.5f;
    rr_draw(f, x + sd->tera_sp[i].x, y + sd->tera_sp[i].y, &o);
  }
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
  /* the pointed target flashes: upstream's tween sets the alpha outright (a Pokemon behind its doll too) */
  const float alpha = tg_pointed(sd) ? tg_flash(B.tg.t) : sd->mon_alpha;
  if (sd->black) o.tint = RR_RGBA(0, 0, 0, 255);
  else if (sd->catch_on) {
    /* pokemon.tint(color) is a flat fill: black through the multiply, then the colour added back as the tone */
    const int v = (int)(255 * sd->catch_untint + 0.5f);
    o.tint = RR_RGBA(v, v, v, 255);
    rr_set_tone((int)(sd->catch_rgb[0] * (1 - sd->catch_untint)), (int)(sd->catch_rgb[1] * (1 - sd->catch_untint)),
                (int)(sd->catch_rgb[2] * (1 - sd->catch_untint)));
  } else if (sd->mon.terastallized) o.tint = tera_tint(sd->mon.tera_type, (int)(alpha * 255 + 0.5f));
  else if (alpha < 1) o.tint = RR_RGBA(255, 255, 255, (int)(alpha * 255 + 0.5f));
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
  if (sd->mon.terastallized && !sd->black && !sd->catch_on) {
    draw_tera_pulse(f, x, y, &o, sd->mon.tera_type);
    tera_spawn(sd, f, sd->scale);
    draw_tera_sparkles(sd, player);
  }
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
  for (int k = 0; k < 4; k++)
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

/* The top screen is 400x240 around upstream's 320x180 field, so the field container (platforms, Pokemon, animations) is drawn FIELD_ZOOM
 * (field_border.h) times bigger about the arena's bottom centre. The G-Max zoom-out (field_scale) comes on top of it. */
/* A Pokemon drawn bigger than its sheet (Eternamax is 1.5x) can stand taller than the field has room for: the zoom about (160, 132) pushes its
 * top out of the screen (the 30 px top band is the last that shows). Then the whole field is drawn smaller, just enough for its top to stay in:
 * a sprite top at field y t lands at 132 + (t - 132) * z, and the screen's top is field y -30. Sprites at their normal size never change it. */
#define FIELD_TOP_Y (-30.0f)
static float fit_zoom(float z) {
  for (int k = 0; k < 4; k++) {
    side* sd = &B.s[k];
    if (!sd->present || !sd->sprite_on || sd->scale <= 1.0f || sd->mon.hidden) continue;
    const rr_frame* f = sprite_frame(sd);
    if (!f) continue;
    float x, y;
    mon_pos(sd, !(k % 2), &x, &y);
    const float top = y - (f->src_h - f->off_y) * sd->scale;
    if (top >= 132) continue;
    const float fit = (132 - FIELD_TOP_Y) / (132 - top);
    if (fit < z) z = fit;
  }
  return z;
}
static float field_zoom_cur = 1;   /* the zoom field_zoom set last (draw_trainers draws a JoJo challenger without it) */
static float field_zoom_on = FIELD_ZOOM;   /* the field's zoom when it was last on */
static void field_zoom(bool on) {
  field_zoom_cur = on ? fit_zoom(B.field_scale * FIELD_ZOOM) : 1;
  if (on) field_zoom_on = field_zoom_cur;
  rr_set_zoom(field_zoom_cur, 160, 132);
}

/* Trainer starts at (-72,80) and slides +300 with the enemy platform. Paired
 * trainers use the native -4/+28 offsets; double-only art is one sprite. */
static void draw_trainers(void) {
  if (!B.trainer_visible) return;
  bool pair=B.setup.trainer_keys[1][0]!=0;
  for (int k=0;k<2;k++) {
    if (!B.setup.trainer_keys[k][0]) continue;
    const rr_frame* frame=img_anim_frame(B.setup.trainer_keys[k],0);
    if (!frame) continue;
    rr_draw_opts o=RR_DEFAULT_OPTS;o.origin_x=0.5f;o.origin_y=1;
    float x=228+B.enemy_slide+(pair?(k?28:-4):0);
    float y=80-foe_raise();
    if (B.trainer_show_ms>0) { /* BEV_TRAINER_SHOW: in from 16 px up and right */
      float k=1-ease(EASE_CUBIC_OUT,B.trainer_show_ms/750);
      x+=16*k;y-=16*k;
    }
    if (!strncmp(B.setup.trainer_keys[k],"images/trainer/custom/",22)) {
      /* a JoJo challenger (lorenzo, 2026-11-15) is drawn 1:1, not FIELD_ZOOM times bigger: the zoom (about (160, 132)) left room for only
       * 83 px above the feet and doubled every fifth row and column of the art. The feet go where the zoom puts them, so he still stands
       * on the platform; at 1:1 a sprite up to 99 px tall fits under the top of the screen (field y -30). */
      const float z=field_zoom_cur;
      if (z!=1) rr_set_zoom(1,160,132);
      rr_draw(frame,160+(x-160)*z,132+(y-132)*z,&o);
      if (z!=1) rr_set_zoom(z,160,132);
      continue;
    }
    rr_draw(frame,x,y,&o);
  }
}

/* The encounter's intro sprites: the container is at (228, 76) once it has slid in with the enemy platform (+300 from -72, EncounterPhase), the sprites
 * at their own offsets from it. me_flow places them for a 320 wide field with the feet at y 164 and the container's x at 160 (its +4 origin included). */
static void draw_me_intro(void) {
  for (int i = 0; i < B.me_n; i++) {
    const me_intro_sprite* s = &B.me_sp[i];
    const rr_frame* f = s->frame[0] ? mui_frame(s->key, s->frame) : img_anim_frame(s->key, 0);
    if (!f && s->alt[0]) f = img_anim_frame(s->alt, 0);
    if (!f) continue;
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_x = 0.5f;
    o.origin_y = 1;
    o.scale_x = o.scale_y = s->scale > 0 ? s->scale : 1;
    const unsigned v = (unsigned)(255.0f * (1.0f - s->dark));
    o.tint = RR_RGBA(v, v, v, 255);
    rr_draw(f, 228 + B.enemy_slide + (s->x - 160), 76 + (s->y - 164), &o);
  }
}

void bv_me_intro(const me_intro_sprite* sprites, int n) {
  B.me_n = n < 0 ? 0 : n > ME_MAX_INTRO ? ME_MAX_INTRO : n;
  if (B.me_n) memcpy(B.me_sp, sprites, sizeof B.me_sp[0] * (size_t)B.me_n);
}

/* EncounterPhase's player back sprite, then SummonPhase's three throw poses.
 * Keep this separate from the capture ball and ordinary switch summons. */
static void draw_player_trainer(void) {
  if (!B.player_trainer_visible) return;
  char key[64];
  bool throwing=B.player_throw_ms>=0;
  snprintf(key,sizeof key,"images/trainer/trainer_%c_back%s",
           B.setup.player_female?'f':'m',throwing?"_pb":"");
  int frame=throwing?(B.player_throw_ms>=626?2:B.player_throw_ms>=562?1:0):0;
  const rr_frame* f=img_anim_frame(key,frame);
  if (!f) return;
  rr_draw_opts o=RR_DEFAULT_OPTS;o.origin_x=0.5f;o.origin_y=1;
  float x=throwing?106-142*ease(EASE_LINEAR,B.player_throw_ms/1000):106+B.arena_player_x;
  rr_draw(f,x,186,&o);
}

static void draw_opening_ball(void) {
  const float t=B.player_throw_ms-750;
  if (!B.opening_summon || t<0 || t>=650) return;
  static const char* const names[]={"pb","gb","ub","rb","mb"};
  int type=B.setup.player.pokeball;
  const rr_frame* f=img_frame("images/pb",names[type>=0 && type<5?type:0]);
  if (!f) return;
  rr_draw_opts o=RR_DEFAULT_OPTS;o.origin_x=0.5f;o.origin_y=0.625f;
  o.angle=t<150?0:8*3.14159265f*ease(EASE_LINEAR,(t-150)/500);
  float y=t<150?80-10*ease(EASE_CUBIC_OUT,t/150):70+62*ease(EASE_CUBIC_IN,(t-150)/500);
  rr_draw(f,36+64*t/650,y,&o);
}

/* BEV_WORLD: DIO's feet beside his Pokemon (field x 284 against the trainer's 228), drawn 1:1 like a JoJo trainer; slide 0 puts him
 * 140 px further right, off the screen */
#define WORLD_FOE_X 284.0f
#define WORLD_SLIDE_PX 140.0f
static void stand_pos(float slide, float* x, float* y) {
  const float z = field_zoom_on;   /* the field's zoom, whether or not it is on right now */
  *x = 160 + (WORLD_FOE_X - 160) * z + (1 - slide) * WORLD_SLIDE_PX;
  *y = 132 + (80 - foe_raise() - 132) * z;
}
static void world_dio_pos(float* x, float* y) { stand_pos(B.world.slide, x, y); }

/* a trainer's second sprite at the stand position (DIO's The World, Kars's Perfect Being), 1:1 whatever the field's zoom */
static void draw_stand_sprite(const char* key, float slide) {
  const rr_frame* f = img_anim_frame(key, 0);
  if (!f) return;
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = 0.5f;
  o.origin_y = 1;
  float x, y;
  stand_pos(slide, &x, &y);
  const float z = field_zoom_cur;
  if (z != 1) rr_set_zoom(1, 160, 132);
  rr_draw(f, x, y, &o);
  if (z != 1) rr_set_zoom(z, 160, 132);
}
static void draw_perfect_kars(void) {
  if (B.perfect.on && B.perfect.key[0]) draw_stand_sprite(B.perfect.key, B.perfect.slide);
}
static void draw_world_dio(void) {
  if (B.world.on && B.world.key[0]) draw_stand_sprite(B.world.key, B.world.slide);
}

/* The time stop's look on what is already drawn (the touched layers): the inverted circle with its colour fringes (each channel is
 * inverted inside its own circle: red 3 px wider, blue 3 px narrower), then the grey blue's tint. */
static void draw_world_filters(void) {
  if (B.world.inv_r > 0) {
    float x, y;
    world_dio_pos(&x, &y);
    y -= 40;
    rr_set_blend(RR_BLEND_INVERT);
    rr_circle(x, y, B.world.inv_r + 3, RR_RGBA(255, 0, 0, 255));
    rr_circle(x, y, B.world.inv_r, RR_RGBA(0, 255, 0, 255));
    rr_circle(x, y, B.world.inv_r - 3, RR_RGBA(0, 0, 255, 255));
    rr_set_blend(RR_BLEND_NORMAL);
  }
  if (B.world.grey > 0) {
    const float g = B.world.grey;
    rr_set_blend(RR_BLEND_MULTIPLY);
    rr_rect(-40, -30, 400, 240, RR_RGBA((int)(255 - 60 * g), (int)(255 - 44 * g), 255, 255));
    rr_set_blend(RR_BLEND_NORMAL);
  }
}

static void draw_field_back(void) {
  char k[64];
  snprintf(k, sizeof k, "images/arenas/%s_bg", B.biome);
  if (B.world.bulge > 0) {   /* the time stop's warp: the background swells about DIO */
    float x, y;
    world_dio_pos(&x, &y);
    rr_set_zoom(1 + B.world.bulge, x, y - 40);
  }
  if (img_has(k))
    img_draw(k, NULL, 0, 0, 0, 0);
  else
    rr_rect(0, 0, 320, 180, RR_RGBA(0x88, 0xc8, 0xf0, 255));
  field_zoom(true);
  snprintf(k, sizeof k, "images/arenas/%s_b", B.biome);
  /* doubles: the foes stand FOE_RAISE higher (field_pos), so their platform goes up with them (y grows downwards) */
  const float raise = -foe_raise();
  if (B.arena_old_on) img_draw(k, NULL, B.arena_old_x, raise, 0, 0);
  img_draw(k, NULL, B.arena_enemy_x, raise, 0, 0);
  snprintf(k, sizeof k, "images/arenas/%s_a", B.biome);
  img_draw(k, NULL, B.arena_player_x, 0, 0, 0);
  field_zoom(false);
}

static void draw_field_player(void) {
  side *pl = &B.s[0];
  draw_player_trainer();
  if (pl->present && pl->sub_on) draw_sub(pl, true, false);
  if (pl->present && pl->tint_on && !pl->tint_above) draw_sub(pl, true, true);
  draw_sprite(pl, true);draw_sparkle(pl,true);
  if (pl->present && pl->tint_on && pl->tint_above) draw_sub(pl, true, true);
  side *ally=&B.s[2];
  if(ally->present&&ally->sub_on)draw_sub(ally,true,false);
  if(ally->present&&ally->tint_on&&!ally->tint_above)draw_sub(ally,true,true);
  draw_sprite(ally,true);draw_sparkle(ally,true);
  if(ally->present&&ally->tint_on&&ally->tint_above)draw_sub(ally,true,true);
}

static void draw_field_foes(void) {
  side *en = &B.s[1];
  if (B.ball.behind) draw_ball();
  draw_trainers();
  draw_me_intro();
  draw_world_dio();
  draw_perfect_kars();
  for(int i=0;i<2;i++){
    side *foe=&B.s[1+2*i];draw_sprite(foe,false);draw_sparkle(foe,false);
    if(i&&foe->present&&foe->sub_on)draw_sub(foe,false,false);
    if(i&&foe->present&&foe->tint_on)draw_sub(foe,false,true);
  }
  if (en->present && en->sub_on) draw_sub(en, false, false);
  if (en->present && en->tint_on) draw_sub(en, false, true);
}

static void draw_field(void) {
  ba_mon field[4];
  anim_field(field);
  if (!B.world.on && B.world.inv_r <= 0 && B.world.grey <= 0) {
    draw_field_back();
    field_zoom(true);
    ba_draw(BA_LAYER_BELOW_ENEMY, field);
    /* the enemy's doll is added over it; the player's Pokemon is brought over its doll */
    draw_field_foes();
    ba_draw(BA_LAYER_MID, field);
    draw_field_player();
  } else {
    /* DIO's time stop (lorenzo, 2026-11-16): the background, the platforms and the player's side first, in grey blue (drawn in colour and
     * again in grey over it while the colour drains or comes back) and under the inverted circle; then, untouched, DIO, his Pokemon and
     * every animation. While it lasts an animation of the middle layer is drawn over the player's Pokemon. */
    const int grey = (int)(B.world.grey * 255 + 0.5f);
    for (int pass = grey >= 255 ? 1 : 0; pass < (grey > 0 ? 2 : 1); pass++) {
      rr_set_grey(pass ? grey : 0);
      draw_field_back();
      field_zoom(true);
      draw_field_player();
      field_zoom(false);
    }
    rr_set_grey(0);
    rr_set_zoom(1, 0, 0);
    draw_world_filters();
    field_zoom(true);
    ba_draw(BA_LAYER_BELOW_ENEMY, field);
    draw_field_foes();
    ba_draw(BA_LAYER_MID, field);
  }
  ba_draw(BA_LAYER_TOP, field);
  if (!B.ball.behind) draw_ball();
  draw_opening_ball();
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

static void draw_types(const bv_mon* m, bool player, bool mini, float bx, float by) {
  char k[48], fr[24];
  const char* who = player ? "player" : "enemy";
  float x1 = player ? -139 : -15, y1 = player ? -17 : -15.5f, y2 = player ? -1 : -2.5f;
  if (player && mini) { x1 += 4; y1 -= 8; y2 -= 8; }   /* PlayerBattleInfo.setMini */
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

/* BattleInfo.teraIcon: icon_tera (12 x 15) at half size, filled with the Tera type's colour (setTintFill), 1 px after the name and the gender
 * sign (BattleInfo.updateIconDisplay: nameText.displayWidth + genderText.displayWidth + 1). Returns the width it takes there, 0 if absent. */
#define TERA_ICON_W 6.0f
static float draw_tera_icon(const bv_mon* m, float x, float y) {
  if (!m->terastallized) return 0;
  const rr_frame* f = img_frame("images/ui/icon_tera", NULL);
  if (!f) return 0;
  int rgb[3];
  tera_type_rgb(m->tera_type, rgb);
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.scale_x = o.scale_y = 0.5f;
  o.tint = RR_RGBA(0, 0, 0, 255); /* a flat fill: black through the multiply, then the colour added back as the tone */
  rr_set_tone(rgb[0], rgb[1], rgb[2]);
  rr_draw(f, x + 1, y - 0.5f, &o);
  rr_set_tone(0, 0, 0);
  return TERA_ICON_W + 1;
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
    w += name_text(x + w, y, m->gender == 0 ? "♂" : "♀", m->gender == 0 ? RR_RGBA(0x40, 0xc8, 0xf8, 255) : RR_RGBA(0xf8, 0x90, 0x90, 255),
                   m->gender == 0 ? RR_RGBA(0x00, 0x60, 0x90, 255) : RR_RGBA(0x98, 0x40, 0x38, 255));
  }
  draw_tera_icon(m, x + w, y);
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

/* BattleInfo.constructStatContainer/updateStats: native two-row labels and
 * signed number frames over each health box; HP reserves the enemy's first column. */
static void draw_stat_panel(side *sd,bool player,float bx,float by){
  if(B.stats_alpha<=0)return;
  const bool mini=player&&player_mini(sd);
  const char *key=player?(mini?"images/ui/pbinfo_player_mini_stats":"images/ui/pbinfo_player_stats"):sd->mon.boss?"images/ui/pbinfo_enemy_boss_stats":"images/ui/pbinfo_enemy_mini_stats";
  const rr_frame *box=img_frame(key,NULL);if(!box)return;
  rr_draw_opts o=RR_DEFAULT_OPTS;o.origin_x=1;o.origin_y=.5f;
  o.tint=RR_RGBA(255,255,255,(int)(255*B.stats_alpha));rr_draw(box,bx,by,&o);
  static const int player_order[]={1,2,3,4,6,7,5},enemy_order[]={0,1,2,3,4,6,7,5};
  static const char *labels[]={"HP","ATK","DEF","SPATK","SPDEF","SPD","ACC","EVA"};
  const int *order=player?player_order:enemy_order;int n=player?7:8;
  float number_x[8]={0},number_w[8]={0};
  char atlas[128];snprintf(atlas,sizeof atlas,"images/ui/text_images/%s/battle_ui/pbinfo_stat",i18n_language());
  for(int i=0;i<n;i++){
    int stat=order[i],prev=i>=2?i-2:0;
    float x=i>(player?1:0)?number_x[prev]+number_w[prev]+(player?4:2):bx-box->src_w+(player?8:5);
    float y=by-box->src_h/2.0f+4;
    if(stat==0||stat==5)y+=5;else if(player==(bool)(i%2))y+=10;
    const rr_frame *label=img_frame(atlas,labels[stat]);
    if(!label)label=img_frame("images/ui/text_images/en/battle_ui/pbinfo_stat",labels[stat]);
    char value[12];snprintf(value,sizeof value,"%d",stat?sd->mon.stat_stages[stat-1]:0);
    const rr_frame *number=img_frame("images/ui/pbinfo_stat_numbers",stat?value:"empty");
    const rr_frame *initial=img_frame("images/ui/pbinfo_stat_numbers",stat?"3":"empty");
    number_x[i]=x+(label?label->src_w:12);number_w[i]=initial?initial->src_w:6;
    if(!stat)continue;
    o.origin_x=o.origin_y=0;
    /* PlayerBattleInfo.initInfo positions the full-size values container at (8,7); setMini moves it by (2,-7). */
    float parent_x=player?(mini?10:8):0,parent_y=player?(mini?0:7):0;
    if(label)rr_draw(label,x+parent_x,y+parent_y,&o);
    if(number)rr_draw(number,number_x[i]+parent_x,y+parent_y,&o);
  }
}

/* Pokemon.setFieldPosition: in a double both player boxes are the mini ones (setMini: 12 up, no HP numbers or EXP bar) and the one of the
 * RIGHT position is offset (setOffset: 27 down, 10 to the right for the player and 10 to the left for the foe). */
static float player_info_y(const side *sd){
  if(!player_doubles())return -72;
  return -72-12+(player_right(sd)?27:0);
}

/* A boss's box rests 48 px to the right of a mini one (Pokemon.showInfo slides it in by 246 px from -198, the others by 150 from -150):
 * the wide box (178 px against 130) keeps the same left margin. */
static float info_base_x(const side *sd,bool player){
  if(player)return player_doubles()&&player_right(sd)?320:310;
  const float base=B.setup.doubles&&second_column(sd)?130:140;
  return base+(sd->mon.boss?48:0);
}

/* how far a box slides in from (and out to) the side: Pokemon.showInfo / hideInfo */
static float info_slide(const side *sd,bool player){return player?150:sd->mon.boss?246:150;}

static void draw_player_info(side* sd) {
  if (!sd->present || !sd->info_on) return;
  const bv_mon* m = &sd->mon;
  float bx = info_base_x(sd,true) + sd->info_dx, by = player_info_y(sd) + target_bob(sd); /* PlayerBattleInfo container */
  /* a double: pbinfo_player_mini, the name, the status and the level 1.5 higher (PlayerBattleInfo.setMini), no HP numbers, no EXP bar */
  const bool mini = player_mini(sd);
  const float lift = mini ? -1.5f : 0;
  img_draw(mini ? "images/ui/pbinfo_player_mini" : "images/ui/pbinfo_player", NULL, bx, by, 1, 0.5f);
  draw_types(m, true, mini, bx, by);
  draw_name(m, true, bx - 115, by - 15.2f + lift);
  draw_level(bx - 41, by - 10 + lift, m->level, false);
  float ratio = m->max_hp ? fminf(sd->hp_shown / m->max_hp, 1.0f) : 0;
  const rr_frame* hf = img_frame("images/ui/overlay_hp", hp_frame(ratio));
  int w = (int)floorf(48 * ratio + 0.5f);
  if (w < 1 && sd->hp_shown > 0) w = 1;
  if (hf && w > 0) rr_draw_cropped(hf, bx - 61, by - 1, w, hf->src_h, NULL);
  img_draw("images/ui/text_images/en/battle_ui/overlay_hp_label", NULL, bx - 62, by - 4, 1, 0);
  if (!mini) {
    /* HP numbers: right to left from (-15, 10), "hp/max" */
    char s[24];
    snprintf(s, sizeof s, "%d/%d", (int)ceilf(sd->hp_shown - 0.001f), m->max_hp);
    draw_numbers("images/ui/numbers", s, bx - 15, by + 10, -1);
    /* EXP bar: overlay_exp at (-98, 18) masked to 85 px */
    img_draw("images/ui/text_images/en/battle_ui/overlay_exp_label", NULL, bx - 91, by + 20, 1, 1);
    const rr_frame* ef = img_frame("images/ui/overlay_exp", NULL);
    int ew = (int)floorf(85 * sd->exp_shown + 0.5f);
    if (ef && ew > 0) rr_draw_cropped(ef, bx - 98, by + 18, ew, ef->src_h, NULL);
  }
  draw_stat_indicators(sd, true, bx, by);
  draw_stat_panel(sd,true,bx,by);
}

/* EnemyBattleInfo.updateBossSegmentDividers: the boss bar's dividers, one fewer than the segments, 1 px wide and 1 px under the bar's top,
 * as tall as the bar less that pixel; at round(maxHp / segments * s) / maxHp of the bar's width, white while the shield s is up
 * (bossSegmentIndex >= s) and 0x404040 once it is broken. A 1 px rectangle centred between two pixels takes the left one. */
static void draw_boss_dividers(const bv_mon* m, float bar_x, float bar_y, float bar_w, float bar_h) {
  const int segments = m->boss_segments;
  if (segments < 2 || m->max_hp <= 0) return;
  const double segment_hp = (double)m->max_hp / segments;
  for (int s = 1; s < segments; s++) {
    const double at = floor(segment_hp * s + 0.5) / m->max_hp * bar_w; /* Math.round */
    const bool up = m->boss_segment_index >= s;
    rr_rect(floorf(bar_x + (float)at), bar_y + 1, 1, bar_h - 1, up ? RR_RGBA(0xff, 0xff, 0xff, 255) : RR_RGBA(0x40, 0x40, 0x40, 255));
  }
}

/* A boss (EnemyBattleInfo.updateBossSegments) has the wide pbinfo_enemy_boss (178 px; the mini box is 130): the name with its icons and the
 * stat values sit 48 px further left, the level 2, the HP bar and its label 38 (the bar 2 px higher, the label 1) and the bar is
 * overlay_hp_boss, 86 px instead of 48, with the label overlay_hp_label_boss. */
static void draw_enemy_info(side* sd) {
  if (!sd->present || !sd->info_on) return;
  const bv_mon* m = &sd->mon;
  const bool boss = m->boss;
  float bx = info_base_x(sd,false) + sd->info_dx, by = (B.setup.doubles&&second_column(sd)?-114:-141) + target_bob(sd); /* EnemyBattleInfo container (setOffset: 27 down) */
  const float name_x = bx - (boss ? 172 : 124), bar_x = bx - (boss ? 109 : 71), bar_y = by + (boss ? 2.5f : 4.5f), bar_w = boss ? 86 : 48;
  img_draw(boss ? "images/ui/pbinfo_enemy_boss" : "images/ui/pbinfo_enemy_mini", NULL, bx, by, 1, 0.5f);
  draw_types(m, false, false, bx, by);
  draw_name(m, false, name_x, by - 11.2f);
  if (m->caught_before) img_draw("images/ui/icon_owned", NULL, name_x, by - 11.2f + 11.75f, 0, 0);
  draw_level(bx - (boss ? 52 : 50), by - 5, m->level, false);
  float ratio = m->max_hp ? fminf(sd->hp_shown / m->max_hp, 1.0f) : 0;
  const rr_frame* hf = img_frame(boss ? "images/ui/overlay_hp_boss" : "images/ui/overlay_hp", hp_frame(ratio));
  int w = (int)floorf(bar_w * ratio + 0.5f);
  if (w < 1 && sd->hp_shown > 0) w = 1;
  if (hf && w > 0) rr_draw_cropped(hf, bar_x, bar_y, w, hf->src_h, NULL);
  img_draw(boss ? "images/ui/text_images/en/battle_ui/overlay_hp_label_boss" : "images/ui/text_images/en/battle_ui/overlay_hp_label", NULL,
           bx - (boss ? 110 : 72), by + (boss ? 0.5f : 1.5f), 1, 0);
  if (boss) draw_boss_dividers(m, bar_x, bar_y, bar_w, hf ? (float)hf->src_h : 4);
  draw_stat_indicators(sd, false, bx, by);
  draw_stat_panel(sd,false,bx,by);
}

/* ------------------------------------------------------------------ drawing: bottom strip */

/* The party tray between the message and the command window: a Poke Ball for each slot (the tray's own art, doubled), a dark one for a
 * fainted member and an empty socket for a free slot. */
static void draw_party_tray(void) {
  static const char* const FRAMES[3] = {"empty", "ball", "faint"};
  ui_window(150, -48, 52, 48, WIN_NORMAL);
  for (int i = 0; i < 6; i++) {
    const rr_frame* f = mui_frame("ui/pb_tray_ball", FRAMES[B.balls[i] <= BV_BALL_FAINTED ? B.balls[i] : 0]);
    if (!f) continue;
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 2;
    rr_draw(f, 155 + (i % 3) * 14, -38 + (i / 3) * 14, &o);
  }
}

/* On the top screen the strip runs the screen's whole width, 40 px past the canvas on each side: the outer windows are longer by that and
 * their contents move 20 px towards the middle of the extra (TOP_PAD), so they keep their margins. */
#define TOP_EXTRA 40
#define TOP_PAD 20
static void draw_command_menu(bool top) {
  const float dx = top ? TOP_PAD : 0;
  ui_window(202, -48, top ? 118 + TOP_EXTRA : 118, 48, WIN_NORMAL);
  static const char* const KEYS[4] = {"commandUiHandler:fight", "commandUiHandler:ball", "commandUiHandler:pokemon",
                                      "commandUiHandler:run"};
  for (int c = 0; c < 4; c++) {
    char s[32];
    i18n_t(s, sizeof s, KEYS[c], NULL);
    text_draw(TS_WINDOW_BATTLE_COMMAND, 217 + dx + (c % 2 ? 55.8f : 0), -38.7f + (c < 2 ? 0 : 16), s, NULL);
  }
  if (B.bar_sel < 0 && B.cmd_cursor < 4) ui_cursor(217 - 5 + dx + (B.cmd_cursor % 2 ? 56 : 0), -38.7f + 8 + (B.cmd_cursor >= 2 ? 16 : 0)); /* the bar has it otherwise */
}

/* The Terastallize button (upstream's teraButton, scale 1.3, its frame named after the Pokemon's Tera type), left of the party tray.
 * Upstream puts it at (185, -23.7) with the message shortened to 910/1110; the tray takes that place here, so it sits left of the tray and
 * the message is shortened by the same 33 px. The selected button (cursor on it, upstream's isTerastallized glow) is drawn larger. */
#define TERA_BTN_X 133.0f
#define TERA_BTN_Y -23.7f
static void draw_tera_at(float x, float y);
static void draw_tera_button(void) { draw_tera_at(TERA_BTN_X, TERA_BTN_Y); }
static void draw_tera_at(float x, float y) {
  if (!B.opts.can_tera) return;
  char fr[24];
  const rr_frame* f = mui_frame("ui/button_tera", gm_enum_lower(GM_E_POKEMON_TYPE, B.opts.tera_type, fr, sizeof fr));
  const bool on = B.cmd_cursor == 4 && B.bar_sel < 0;
  if (f) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.origin_x = o.origin_y = 0.5f;
    o.scale_x = o.scale_y = on ? 1.6f : 1.3f;
    if (on) o.tint = tera_tint(B.opts.tera_type, 255); /* the glow: upstream's toggleTeraButton gives it the Terastallized look */
    rr_draw(f, x, y, &o);
    if (on) draw_tera_pulse(f, x, y, &o, B.opts.tera_type);
  } else {
    rr_rect(x - 10, y - 9, 20, 18, on ? RR_RGBA(0xf8, 0xf8, 0xf8, 255) : RR_RGBA(0x80, 0x80, 0x98, 255));
  }
}

static text_style pp_style(int left, int max) {
  float r = max ? (float)left / max : 1;
  if (r > 0.25f && r <= 0.5f) return TS_MOVE_PP_HALF_FULL;
  if (r > 0 && r <= 0.25f) return TS_MOVE_PP_NEAR_EMPTY;
  if (r == 0) return TS_MOVE_PP_EMPTY;
  return TS_MOVE_PP_FULL;
}

static void draw_fight_menu(bool top) {
  const bv_mon* m = &command_side()->mon;
  const float lx = top ? -TOP_PAD : 0, rx = top ? TOP_PAD : 0; /* the moves' window's contents and the move info's */
  ui_window(top ? -TOP_EXTRA : 0, -48, top ? 243 + TOP_EXTRA : 243, 48, WIN_NORMAL);
  ui_window(240, -48, top ? 80 + TOP_EXTRA : 80, 48, WIN_NORMAL);
  for (int i = 0; i < 4; i++) {
    const char* nm = i < m->n_moves ? i18n_move_name(m->moves[i]) : "-";
    text_draw(TS_WINDOW, 18 + lx + (i % 2 ? 114 : 0), -38.7f + (i < 2 ? 0 : 16), nm, NULL);
  }
  if (B.tera_fight) {
    /* upstream's cursor_tera, scaled 0.7 */
    const rr_frame* cf = img_frame("images/ui/cursor_tera", NULL);
    if (cf) {
      rr_draw_opts o = RR_DEFAULT_OPTS;
      o.origin_x = o.origin_y = 0.5f;
      o.scale_x = o.scale_y = 0.7f;
      rr_draw(cf, 13 + lx + (B.move_cursor % 2 ? 114 : 0), -31 + (B.move_cursor >= 2 ? 15 : 0), &o);
    } else {
      ui_cursor(13 + lx + (B.move_cursor % 2 ? 114 : 0), -31 + (B.move_cursor >= 2 ? 15 : 0));
    }
  } else {
    ui_cursor(13 + lx + (B.move_cursor % 2 ? 114 : 0), -31 + (B.move_cursor >= 2 ? 15 : 0));
  }
  if (B.move_cursor >= m->n_moves) return;
  int mv = m->moves[B.move_cursor];
  const pr_move* pm = mv >= 0 && mv < PR_MOVE_COUNT ? &pr_moves[mv] : NULL;
  char fr[24];
  /* moveInfoContainer at (1, 0) */
  /* upstream scales the type icon by 0.8; at 1:1 pixels that turns its 5 px lettering to mud ("NORMAL" cannot be read), so it is
   * drawn at full size, a little to the left of upstream's centre so it and the category icon (28 px) still fit the window */
  if (pm) {
    img_draw("images/types", gm_enum_lower(GM_E_POKEMON_TYPE, pm->type, fr, sizeof fr), 1 + rx + 320 - 56, -36, 0.5f, 0.5f);
    img_draw("images/categories", gm_enum_lower(GM_E_MOVE_CATEGORY, pm->category, fr, sizeof fr), 1 + rx + 320 - 24, -36,
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
  text_draw(TS_MOVE_LABEL, 1 + rx + 320 - 70, -26, i18n_t(s, sizeof s, "fightUiHandler:pp", NULL), &lo);
  text_draw(TS_MOVE_LABEL, 1 + rx + 320 - 70, -18, i18n_t(s, sizeof s, "fightUiHandler:power", NULL), &lo);
  text_draw(TS_MOVE_LABEL, 1 + rx + 320 - 70, -10, i18n_t(s, sizeof s, "fightUiHandler:accuracy", NULL), &lo);
  int left = m->pp_left[B.move_cursor], max = m->pp_max[B.move_cursor];
  snprintf(s, sizeof s, "%2d/%2d", left, max);
  text_style ps = pp_style(left, max);
  text_opts po = ro;
  po.color = text_color(ps, false);
  po.shadow = text_color(ps, true);
  text_draw(TS_MOVE_LABEL, 1 + rx + 320 - 12, -26, s, &po);
  if (pm) {
    if (pm->power >= 0)
      snprintf(s, sizeof s, "%d", m->move_power[B.move_cursor]>0?m->move_power[B.move_cursor]:pm->power);
    else
      snprintf(s, sizeof s, "---");
    text_draw(TS_MOVE_LABEL, 1 + rx + 320 - 12, -18, s, &ro);
    if (pm->accuracy >= 0)
      snprintf(s, sizeof s, "%d", pm->accuracy);
    else
      snprintf(s, sizeof s, "---");
    text_draw(TS_MOVE_LABEL, 1 + rx + 320 - 12, -10, s, &ro);
  }
}

/* The free part of the bottom screen above the strip (y -240..-48): the move's description in
 * the fight menu (upstream shows it on the info button, MoveInfoOverlay). */
static void draw_move_details(void) {
  const bv_mon* m = &command_side()->mon;
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

/* Made in Heaven's box: red, top left of the top screen (physical px, over the 30 px top band), in place of the ability bar */
static void draw_heaven_box(void) {
  if (B.heaven_box_t <= 0) return;
  const float w = text_width(TS_MOVE_LABEL, B.ability_text, NULL) + 16;
  rr_rect(4, 4, w, 22, RR_RGBA(0x50, 0x00, 0x08, 255));
  rr_rect(5, 5, w - 2, 20, RR_RGBA(0xc8, 0x10, 0x20, 255));
  text_draw(TS_MOVE_LABEL, 12, 11, B.ability_text, NULL);
}

/* BEV_WORLD's timings and sizes */
#define WORLD_GROW_MS 700.0f
#define WORLD_DRAIN_MS 300.0f
#define WORLD_MAX_R 440.0f   /* past the farthest corner of the 400 x 240 screen from DIO */
#define WORLD_BULGE 0.06f
/* The time stop's sounds (lorenzo 2026-11-16): the user's own files, from the builder's extra-audio/se folder; without them it is silent.
 * the_world starts with the inverted circle, and the battle goes on once it has played out (at most WORLD_HOLD_MAX_MS past the grey);
 * the_world_resume starts as the colour comes back. */
#define WORLD_SOUND "se/the_world"
#define WORLD_RESUME_SOUND "se/the_world_resume"
#define WORLD_HOLD_MAX_MS 6000.0f

/* BEV_WORLD's sprite: <id>_world next to the trainer's (dio -> dio_world) if the pack has it, else his own */
static void stand_key(char* out, size_t size, const char* suffix) {
  const char* k = B.setup.trainer_keys[0];
  out[0] = 0;
  if (strncmp(k, "images/trainer/custom/", 22)) return;
  const size_t n = strlen(k), m = strlen(suffix);
  if (n + m < size) {
    memcpy(out, k, n);
    memcpy(out + n, suffix, m + 1);
    if (img_has(out)) return;
  }
  snprintf(out, size, "%s", k);
}
static void world_key(void) { stand_key(B.world.key, sizeof B.world.key, "_world"); }
/* BEV_PERFECT's sprite: <id>_perfect (kars -> kars_perfect) if the pack has it, else his own */
static void perfect_key(void) { stand_key(B.perfect.key, sizeof B.perfect.key, "_perfect"); }

/* BEV_PERFECT's lines (lorenzo picked them, 2026-11-16); "" for no line */
static void perfect_line(int a, int part, int battler, char* out, size_t size) {
  out[0] = 0;
  if (a == 1 && part == 0)
    snprintf(out, size, "%s has become the Ultimate Life Form!", B.setup.trainer_name[0] ? B.setup.trainer_name : "Kars");
  else if (a == 2 && part == 0) snprintf(out, size, "%s's flesh flowed aside like water!", name_with_affix(battler));
  else if (a == 2 && part == 1) snprintf(out, size, "The attack did nothing!");
  else if (a == 3 && part == 0) snprintf(out, size, "%s's perfect body softened the blow!", name_with_affix(battler));
}

/* BEV_HEAVEN's sprite: the stage's own file next to the trainer's (pucci -> pucci_cmoon / pucci_heaven), if the pack has it */
static void heaven_sprite(int stage) {
  char *k = B.setup.trainer_keys[0];
  if (strncmp(k, "images/trainer/custom/", 22)) return;
  char alt[sizeof B.setup.trainer_keys[0]];
  snprintf(alt, sizeof alt, "%s", k);
  char* u = strrchr(alt, '_');
  if (u && (!strcmp(u, "_cmoon") || !strcmp(u, "_heaven"))) *u = 0;
  const char* suffix = stage >= 2 ? "_heaven" : "_cmoon";
  const size_t n = strlen(alt);
  if (n + strlen(suffix) >= sizeof alt) return;
  memcpy(alt + n, suffix, strlen(suffix) + 1);
  if (img_has(alt)) memcpy(B.setup.trainer_keys[0], alt, sizeof alt);
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
  if (B.lvl.ivs) {
    const float width = 200;
    ui_window(320 - width, -100, width, 100, WIN_NORMAL);
    for (int s = 0; s < 6; s++) {
      const int iv = B.lvl.stats[s];
      const char* key = iv == 31 ? "ivBest" : iv == 30 ? "ivFantastic" : iv > 20 ? "ivVeryGood" :
                        iv > 10 ? "ivPrettyGood" : iv > 0 ? "ivDecent" : "ivNoGood";
      char locale[80], value[96];
      snprintf(locale, sizeof locale, "battleMessageUiHandler:%s", key);
      i18n_t(value, sizeof value, locale, NULL);
      text_draw(TS_WINDOW, 320 - width + 6, -94 + s * 14, i18n_stat_name(s), NULL);
      text_opts o = text_defaults(); o.origin_x = 1;
      text_style style = iv > B.lvl.stats[6+s] ? (iv == 31 ? TS_PERFECT_IV : TS_SUMMARY_GREEN) : TS_WINDOW;
      text_draw(style, 313, -94 + s * 14, value, &o);
    }
    return;
  }
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

/* The two item bars (a choice of this port: upstream's ModifierBars are on the top screen at half scale). Bar 0 is the foe's items,
 * bar 1 the player's own items that no Pokemon holds. L and R switch between them. */
static int bar_n(void) { return B.bar ? B.n_key : B.n_foe; }
static const bv_item* bar_it(int i) { return B.bar ? &B.key[i] : &B.foe[i]; }
static const char* bar_title(void) { return B.bar ? "Key items" : "Foe items"; }

static void draw_bar_icon(const bv_item* it, float x, float y) {
  const rr_frame* f = it->icon ? mui_frame("items", it->icon) : NULL; /* the menus' copy of the atlas (shop, summary) */
  if (f) {
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 0.75f;
    rr_draw(f, x, y, &o);
  }
  if (it->count > 1) {
    char n[16];
    snprintf(n, sizeof n, "%d", it->count);
    text_opts t = text_defaults();
    t.origin_x = 1;
    text_draw(TS_INSTRUCTIONS_TEXT, x + 24, y + 14, n, &t);
  }
}

static void draw_cursor_frame(float x, float y, float w, float h) {
  const rr_color c = RR_RGBA(0xf8, 0xe0, 0x58, 255);
  rr_rect(x, y, w, 1, c);
  rr_rect(x, y + h - 1, w, 1, c);
  rr_rect(x, y, 1, h, c);
  rr_rect(x + w - 1, y, 1, h, c);
}

/* The bar on the top screen, the 30 px under the command strip (physical coordinates, the screen's whole width and height there): its
 * title, then the icons (26 px cells, 2 px in from the bar's top and bottom), scrolled so the cursor is always in view. */
#define TBAR_X 0
#define TBAR_Y 210
#define TBAR_W 400
#define TBAR_H 30
#define TBAR_TITLE_X 12
#define TBAR_ICONS_X 76
#define TBAR_ICONS 12
static int tbar_first(void) { return B.bar_sel >= TBAR_ICONS ? B.bar_sel - TBAR_ICONS + 1 : 0; }
static void draw_top_bar(void) {
  if (!B.begun || sv()) return; /* the standard view: "no more item bar" (lorenzo's design); its Items button opens them on the bottom screen */
  char meter[48];int luck=B.opts.party_luck<0?0:B.opts.party_luck>14?14:B.opts.party_luck;
  snprintf(meter,sizeof meter,"Luck: %s (%d/14)",luck_grade(luck),luck);
  text_opts label=text_defaults();label.origin_x=1;
  text_draw(TS_MOVE_LABEL,390,10,meter,&label);
  rr_rect(TBAR_X, TBAR_Y, TBAR_W, TBAR_H, RR_RGBA(0x40, 0x40, 0x48, 255));
  rr_rect(TBAR_X + 1, TBAR_Y + 1, TBAR_W - 2, TBAR_H - 2, RR_RGBA(0x28, 0x28, 0x30, 255));
  text_draw(TS_MOVE_LABEL, TBAR_X + TBAR_TITLE_X, TBAR_Y + 11, bar_title(), NULL);
  const int n = bar_n(), first = tbar_first();
  for (int i = first; i < n && i < first + TBAR_ICONS; i++)
    draw_bar_icon(bar_it(i), TBAR_X + TBAR_ICONS_X + (i - first) * 26 + 1, TBAR_Y + 3);
  if (n == 0) text_draw(TS_MOVE_LABEL, TBAR_X + TBAR_ICONS_X, TBAR_Y + 11, "none", NULL);
  if (B.mode==M_COMMAND && B.bar_sel >= 0 && B.bar_sel < n) draw_cursor_frame(TBAR_X + TBAR_ICONS_X + (B.bar_sel - first) * 26, TBAR_Y + 2, 26, 26);
}

/* An item's name (with its stack) and effect, in the message window in place of its usual text: the bar's selected item ("What will X
 * do?" gives way, and so do the party tray and the command window, to give the text the strip's width), or the one the summary
 * screen asks for. */
static void draw_item_text(const char* name, const char* desc, bool top) {
  text_opts d = text_defaults();
  const float x = top ? 12 - TOP_PAD : 12;
  d.wrap_w = top ? 296 + TOP_EXTRA : 296;
  text_draw(TS_WINDOW, x, -42, name, NULL);
  /* the 8 px pixel font (the fight menu's labels): the longest description is 116 characters, two lines of it fit the window under
   * the name where the 12 px style wraps to three and runs out of it */
  if (desc && *desc) text_draw(TS_MOVE_LABEL, x, -28, desc, &d);
}

static void bar_info_text(char* name, size_t n, const char** desc) {
  const bv_item* it = bar_it(B.bar_sel);
  if (it->count > 1)
    snprintf(name, n, "%s x%d", it->name ? it->name : "?", it->count);
  else
    snprintf(name, n, "%s", it->name ? it->name : "?");
  *desc = it->desc;
}

void bv_set_info(const char* name, const char* desc) {
  B.info_on = name != NULL;
  snprintf(B.info_name, sizeof B.info_name, "%s", name ? name : "");
  snprintf(B.info_desc, sizeof B.info_desc, "%s", desc ? desc : "");
}

/* The target choice on the bottom screen: a tile for each of the four places, foes on the top row and the player's side under them, left and
 * right as they stand on the field; the pointed tile has the yellow frame (the Pokemon on the field flashes), a place the move cannot reach is
 * dimmed, and the strip says "A: choose   B: back" (each a tap). Upstream has no such panel: its field is the screen you point at. */
#define TG_X0 8.0f
#define TG_W 148.0f
#define TG_GAP 8.0f
#define TG_H 56.0f
#define TG_FOE_Y (-192.0f)
#define TG_ALLY_Y (-128.0f)
/* the debug view (X) keeps its rows and the X / Y labels (physical y 2..48) clear: no heading, and two thin rows just above the strip with the name
 * and the HP bar alone (lorenzo, 2026-11-17: "the double battle ui should be kept to a minimum") */
#define TG_MINI_H 24.0f
#define TG_MINI_FOE_Y (-110.0f)
#define TG_MINI_ALLY_Y (-82.0f)
static bool tg_mini(void) { return g_view == BV_VIEW_DEBUG; }
static float tg_h(void) { return tg_mini() ? TG_MINI_H : TG_H; }
#define TG_HINT_X 12.0f
#define TG_HINT_Y (-29.0f)
static const char* const TG_HINTS[2] = {"A: choose", "B: back"};
static void tg_tile(int f, float* x, float* y) {
  *x = TG_X0 + (f % 2) * (TG_W + TG_GAP);
  *y = tg_mini() ? (f >= 2 ? TG_MINI_FOE_Y : TG_MINI_ALLY_Y) : f >= 2 ? TG_FOE_Y : TG_ALLY_Y;
}
static int tg_tile_at(float x, float y) {
  for (int f = 0; f < 4; f++) {
    float tx, ty;
    tg_tile(f, &tx, &ty);
    if (x >= tx && x < tx + TG_W && y >= ty && y < ty + tg_h()) return f;
  }
  return -1;
}
static void draw_target_tile(int f) {
  const side* sd = side_of(f);
  float x, y;
  tg_tile(f, &x, &y);
  const bool mini = tg_mini();
  const float h = tg_h();
  ui_window(x, y, TG_W, h, mini ? WIN_THIN : WIN_NORMAL);
  if (sd->present && sd->mon.species > 0) {
    const bv_mon* m = &sd->mon;
    bv_mon shown = *m;
    if (mini) shown.status = 0;                               /* (its icon would go under the thin row) */
    draw_name(&shown, f < 2, x + 8, y + (mini ? 8 : 12));
    if (!mini) draw_level(x + TG_W - 42, y + 15, m->level, false);
    const float ratio = m->max_hp ? fmaxf(0, fminf(sd->hp_shown / m->max_hp, 1.0f)) : 0;
    const float bar_x = mini ? x + 92 : x + 26, bar_y = mini ? y + 10 : y + 37, bar_w = mini ? TG_W - 100 : TG_W - 38;
    if (!mini) img_draw("images/ui/text_images/en/battle_ui/overlay_hp_label", NULL, bar_x - 2, bar_y - 1, 1, 0);
    rr_rect(bar_x, bar_y, bar_w, 5, RR_RGBA(0x30, 0x30, 0x38, 255));
    const rr_color c = ratio > 0.5f ? RR_RGBA(0x50, 0xd0, 0x60, 255) : ratio > 0.25f ? RR_RGBA(0xf8, 0xc8, 0x30, 255) : RR_RGBA(0xf0, 0x50, 0x48, 255);
    float w = floorf(bar_w * ratio + 0.5f);
    if (w < 1 && sd->hp_shown > 0) w = 1;
    if (w > 0) rr_rect(bar_x, bar_y, w, 5, c);
  }
  if (!B.tg.ok[f]) rr_rect(x + 2, y + 2, TG_W - 4, h - 4, RR_RGBA(0x10, 0x10, 0x18, 160));
  if (f == B.tg.cursor) {
    draw_cursor_frame(x, y, TG_W, h);
    draw_cursor_frame(x + 1, y + 1, TG_W - 2, h - 2);
  }
}
static void draw_target_pad(void) {
  char t[96];
  if (B.tg.title[0]) snprintf(t, sizeof t, "Choose a target for %s", B.tg.title);
  else snprintf(t, sizeof t, "Choose a target");
  if (!tg_mini()) text_draw(TS_MOVE_LABEL, TG_X0, TG_FOE_Y - 12, t, NULL); /* the standard view: no debug rows above it */
  for (int f = 0; f < 4; f++) draw_target_tile(f);
  for (int i = 0; i < 2; i++) text_draw(TS_INSTRUCTIONS_TEXT, hint_x(TS_INSTRUCTIONS_TEXT, TG_HINT_X, TG_HINTS, i), TG_HINT_Y, TG_HINTS[i], NULL);
}

/* Upstream's ui container strip (y -48..0): the message window's bg frame, the command window or the fight windows, and the text.
 * It is drawn twice: on the bottom screen, where the touch-free controls live, and on the top screen over the field's last 48 rows,
 * where upstream puts it (it covers the lower part of the player's Pokemon, as upstream's does). `top` leaves out what upstream
 * draws elsewhere: the move description (MoveInfoOverlay) and the ability bar. */
static bool strip_fight(bool top) { return B.mode == M_FIGHT || (top && B.mode == M_TARGET); }
static void draw_bottom(bool top) {
  /* the strip's background: bg frame <window style> at (0,0), origin (0,1) */
  char fr[4];
  snprintf(fr, sizeof fr, "%d", ui_window_style());
  /* the top screen's strip keeps the fight menu, the move just picked under the cursor, while its target is chosen (lorenzo, 2026-11-17: not an
   * empty grey bar); the bottom strip has the target choice's hints */
  const bool fight = strip_fight(top);
  if (!fight) {
    if (top)
      ui_window(-TOP_EXTRA, -48, 320 + 2 * TOP_EXTRA, 48, WIN_NORMAL); /* across the whole screen */
    else if (img_frame("images/ui/bg", fr))
      img_draw("images/ui/bg", fr, 0, 0, 0, 1);
    else
      rr_rect(0, -48, 320, 48, RR_RGBA(0x30, 0x50, 0x68, 255));
  }
  const bool bar_info = B.mode == M_COMMAND && B.bar_open && B.bar_sel >= 0 && B.bar_sel < bar_n();
  const bool summary_info = B.info_on && (B.mode == M_IDLE || B.mode == M_COMMAND);
  const bool info = bar_info || summary_info;
  /* the standard view has the commands on the bottom screen's own buttons: the top strip keeps "What will X do?" alone */
  const bool strip_cmd = cmd_shown() && !info && !sv();
  if (strip_cmd) draw_command_menu(top);
  if (strip_cmd) draw_tera_button();
  if (fight) {
    draw_fight_menu(top);
    if (!top) draw_move_details();
  } else if (bar_info) {
    char name[64];
    const char* desc;
    bar_info_text(name, sizeof name, &desc);
    draw_item_text(name, desc, top);
  } else if (summary_info) {
    draw_item_text(B.info_name, B.info_desc, top);
  } else {
    msgbox_draw(&B.msg, top ? 12 - TOP_PAD : 12, -39);
    if (B.speaker && B.mode == M_EVENTS) { /* showNameText: (12, -55) over the message window */
      if (top) dv_namebox_at(RR_TOP, 40, 210, 12 - TOP_PAD, -55, B.speaker, ui_window_style());
      else dv_namebox_at(RR_BOTTOM, 0, 240, 12, -55, B.speaker, ui_window_style());
    }
  }
  if (strip_cmd && B.has_balls) draw_party_tray();
  if (!top && B.mode == M_TARGET) draw_target_pad();
  if(!top)draw_level_stats();
  if (!top) draw_ability_bar();
}

/* ------------------------------------------------------------------ the standard view (lorenzo's Black 2 design, 2026-11-17) */

/* The bottom screen in physical px. A strip across the top (wave, biome, money, battery), under it the Items button, the party's balls and the
 * Settings button (START's menu), then FIGHT in a big red hexagon with Ball left, Pokemon right and Run under them, drawn in code (no game
 * art: the hexagons are rects and triangles). While the command is asked these replace the strip's command window; the other modes keep the
 * strip at the bottom under the top strip. */
static struct {
  char wave[24], biome[48], money[32];
} g_hdr;
static int (*battery_hook)(bool* charging);
static int g_batt = -1;
static bool g_batt_charging;
static float g_batt_ms;

void bv_set_header(int wave, const char* biome, const char* money) {
  if (wave > 0) snprintf(g_hdr.wave, sizeof g_hdr.wave, "Wave %d", wave);
  else g_hdr.wave[0] = 0;
  snprintf(g_hdr.biome, sizeof g_hdr.biome, "%s", biome ? biome : "");
  snprintf(g_hdr.money, sizeof g_hdr.money, "%s", money ? money : "");
}
void bv_set_battery_hook(int (*hook)(bool* charging)) {
  battery_hook = hook;
  g_batt = -1;
  g_batt_ms = 0;
}
/* the battery is asked every 5 s, not every frame */
static void sv_battery_update(float dt) {
  if (!battery_hook) return;
  g_batt_ms -= dt;
  if (g_batt_ms > 0) return;
  g_batt_ms = 5000;
  bool c = false;
  g_batt = battery_hook(&c);
  g_batt_charging = c;
}

typedef struct {
  float x, y, w, h, p; /* box and the depth of the left and right points */
} sv_hex;
#define SV_HDR_H 17.0f
/* the commands by their number (FIGHT 0, BALL 1, POKEMON 2, RUN 3) */
static const sv_hex SV_CMD[4] = {
    {56, 46, 208, 92, 36},   /* FIGHT */
    {4, 146, 100, 40, 14},   /* BALL */
    {216, 146, 100, 40, 14}, /* POKEMON */
    {108, 176, 104, 36, 14}, /* RUN */
};
static const sv_hex SV_ITEMS = {4, 21, 76, 21, 9};
static const sv_hex SV_SETTINGS = {240, 21, 76, 21, 9};
#define SV_BALLS_X 118.0f
#define SV_BALLS_Y 25.0f
#define SV_TERA_X 30.0f
#define SV_TERA_Y 92.0f
/* the item panel over the buttons: a title row (bar, luck, hints), a grid of 26 px cells, the pointed item's name and effect */
#define SV_PANEL_X 4.0f
#define SV_PANEL_Y 46.0f
#define SV_PANEL_W 312.0f
#define SV_PANEL_H 166.0f
#define SV_GRID_X 17.0f
#define SV_GRID_Y 66.0f
#define SV_ITEM_COLS 11
#define SV_HINT_X 196.0f
#define SV_TITLE_Y (SV_PANEL_Y + 7)
static const char* const SV_HINTS[2] = {"L/R: switch", "B: close"};
/* where the X / Y line goes in the standard view: under the buttons while they are up, else under the top strip */
#define SV_KEYS_CMD_Y 216.0f
#define SV_KEYS_Y 21.0f

static rr_color sv_edge(int c) {
  static const rr_color E[4] = {RR_RGBA(0xf8, 0x70, 0x60, 255), RR_RGBA(0xe8, 0xb8, 0x20, 255), RR_RGBA(0x60, 0xc8, 0x40, 255),
                                RR_RGBA(0x38, 0x98, 0xe8, 255)};
  return E[c];
}
/* a command that is off is greyed (the error sound plays when it is picked); POKEMON is always on (it opens the party, see bv_input) */
static bool sv_cmd_ok(int c) { return c == 1 ? B.opts.can_ball : c == 3 ? B.opts.can_run : true; }
/* The buttons are up: the command is asked in the standard view (and the ball and party screens opened from it keep them under). */
static bool sv_cmd_shown(void) { return sv() && cmd_shown() && !B.info_on; }

static void sv_hex_fill(float x, float y, float w, float h, float p, rr_color c) {
  if (p > w / 2) p = w / 2;
  rr_rect(x + p, y, w - 2 * p, h, c);
  rr_tri(x, y + h / 2, x + p, y, x + p, y + h, c);
  rr_tri(x + w, y + h / 2, x + w - p, y, x + w - p, y + h, c);
}
/* an outline of b px (the inner hexagon keeps the slopes) */
static void sv_hex_draw(const sv_hex* h, float b, rr_color edge, rr_color fill) {
  sv_hex_fill(h->x, h->y, h->w, h->h, h->p, edge);
  sv_hex_fill(h->x + b, h->y + b, h->w - 2 * b, h->h - 2 * b, h->p * (h->h - 2 * b) / h->h, fill);
}
static bool sv_hex_hit(const sv_hex* h, float x, float y) {
  if (x < h->x || x >= h->x + h->w || y < h->y || y >= h->y + h->h) return false;
  const float dx = fminf(x - h->x, h->x + h->w - x), dy = fabsf(y - (h->y + h->h / 2));
  return dx >= h->p || dy <= h->h / 2 * dx / h->p;
}
static void sv_label(const sv_hex* h, const char* s, int px, rr_color c) {
  text_opts o = text_defaults();
  o.origin_x = o.origin_y = 0.5f;
  if (px) o.font_px = px;
  if (c) o.color = c;
  text_draw(TS_WINDOW, h->x + h->w / 2, h->y + h->h / 2, s, &o);
}

static void sv_draw_battery(float x, float y) {
  if (g_batt < 0) return;
  const rr_color line = RR_RGBA(0xd0, 0xd0, 0xd8, 255);
  rr_rect(x, y, 20, 10, line);
  rr_rect(x + 1, y + 1, 18, 8, RR_RGBA(0x10, 0x10, 0x14, 255));
  rr_rect(x + 20, y + 3, 2, 4, line);
  const int n = g_batt > 5 ? 5 : g_batt;
  const rr_color bar = g_batt_charging ? RR_RGBA(0x60, 0xd0, 0x60, 255) : n <= 1 ? RR_RGBA(0xf0, 0x50, 0x48, 255) : line;
  for (int i = 0; i < n; i++) rr_rect(x + 2 + i * 3.4f, y + 2, 2.6f, 6, bar);
}

static void sv_draw_header(void) {
  rr_rect(0, 0, 320, SV_HDR_H, RR_RGBA(0x10, 0x10, 0x14, 255));
  rr_rect(0, SV_HDR_H, 320, 1, RR_RGBA(0x58, 0x58, 0x68, 255));
  text_opts o = text_defaults();
  o.font_px = 72;
  o.origin_y = 0.5f;
  const float y = SV_HDR_H / 2;
  text_draw(TS_WINDOW, 6, y, g_hdr.wave, &o);
  o.origin_x = 0.5f;
  text_draw(TS_WINDOW, 150, y, g_hdr.biome, &o);
  o.origin_x = 1;
  text_draw(TS_WINDOW, 290, y, g_hdr.money, &o);
  sv_draw_battery(294, 4);
}

static void sv_draw_balls(void) {
  if (!B.has_balls) return;
  static const char* const FRAMES[3] = {"empty", "ball", "faint"};
  for (int i = 0; i < 6; i++) {
    const rr_frame* f = mui_frame("ui/pb_tray_ball", FRAMES[B.balls[i] <= BV_BALL_FAINTED ? B.balls[i] : 0]);
    if (!f) continue;
    rr_draw_opts o = RR_DEFAULT_OPTS;
    o.scale_x = o.scale_y = 2;
    rr_draw(f, SV_BALLS_X + i * 14, SV_BALLS_Y, &o);
  }
}

static void sv_draw_commands(void) {
  static const char* const KEYS[4] = {"commandUiHandler:fight", "commandUiHandler:ball", "commandUiHandler:pokemon", "commandUiHandler:run"};
  const rr_color dark = RR_RGBA(0x28, 0x28, 0x30, 255), dark_on = RR_RGBA(0x48, 0x48, 0x58, 255), off = RR_RGBA(0x60, 0x60, 0x68, 255);
  const bool cursor = !B.items_on && B.mode == M_COMMAND;
  for (int c = 0; c < 4; c++) {
    const bool on = cursor && B.cmd_cursor == c, ok = sv_cmd_ok(c);
    rr_color fill = c == 0 ? (on ? RR_RGBA(0xe8, 0x40, 0x38, 255) : RR_RGBA(0xb8, 0x20, 0x20, 255)) : on ? dark_on : dark;
    if (on) sv_hex_draw(&(sv_hex){SV_CMD[c].x - 2, SV_CMD[c].y - 2, SV_CMD[c].w + 4, SV_CMD[c].h + 4, SV_CMD[c].p + 1}, 2,
                        RR_RGBA(0xf8, 0xf8, 0xf8, 255), RR_RGBA(0xf8, 0xf8, 0xf8, 255));
    sv_hex_draw(&SV_CMD[c], 3, ok ? sv_edge(c) : off, fill);
    char s[32];
    i18n_t(s, sizeof s, KEYS[c], NULL);
    sv_label(&SV_CMD[c], s, c == 0 ? 144 : 0, ok ? 0 : RR_RGBA(0x90, 0x90, 0x98, 255));
  }
  const rr_color grey = RR_RGBA(0xa0, 0xa0, 0xb0, 255);
  sv_hex_draw(&SV_ITEMS, 2, B.items_on ? RR_RGBA(0xf8, 0xe0, 0x58, 255) : grey, dark);
  sv_label(&SV_ITEMS, "Items", 72, 0);
  sv_hex_draw(&SV_SETTINGS, 2, B.opts.on_menu ? grey : off, dark);
  sv_label(&SV_SETTINGS, "Settings", 72, B.opts.on_menu ? 0 : RR_RGBA(0x90, 0x90, 0x98, 255));
  sv_draw_balls();
  if (B.mode == M_COMMAND && !B.items_on) draw_tera_at(SV_TERA_X, SV_TERA_Y);
}

static void sv_draw_items(void) {
  ui_window(SV_PANEL_X, SV_PANEL_Y, SV_PANEL_W, SV_PANEL_H, WIN_NORMAL);
  text_draw(TS_MOVE_LABEL, SV_GRID_X - 5, SV_TITLE_Y, bar_title(), NULL);
  char meter[48];
  const int luck = B.opts.party_luck < 0 ? 0 : B.opts.party_luck > 14 ? 14 : B.opts.party_luck;
  snprintf(meter, sizeof meter, "Luck: %s (%d/14)", luck_grade(luck), luck);
  text_draw(TS_MOVE_LABEL, SV_GRID_X + 66, SV_TITLE_Y, meter, NULL);
  for (int i = 0; i < 2; i++) text_draw(TS_MOVE_LABEL, hint_x(TS_MOVE_LABEL, SV_HINT_X, SV_HINTS, i), SV_TITLE_Y, SV_HINTS[i], NULL);
  const int n = bar_n();
  for (int i = 0; i < n; i++) draw_bar_icon(bar_it(i), SV_GRID_X + (i % SV_ITEM_COLS) * 26 + 1, SV_GRID_Y + (i / SV_ITEM_COLS) * 26 + 1);
  if (n == 0) text_draw(TS_MOVE_LABEL, SV_GRID_X, SV_GRID_Y + 9, "none", NULL);
  if (B.bar_sel >= 0 && B.bar_sel < n) {
    draw_cursor_frame(SV_GRID_X + (B.bar_sel % SV_ITEM_COLS) * 26, SV_GRID_Y + (B.bar_sel / SV_ITEM_COLS) * 26, 26, 26);
    char name[64];
    const char* desc;
    bar_info_text(name, sizeof name, &desc);
    text_draw(TS_WINDOW, SV_GRID_X - 5, SV_PANEL_Y + SV_PANEL_H - 44, name, NULL);
    text_opts d = text_defaults();
    d.wrap_w = SV_PANEL_W - 16;
    if (desc && *desc) text_draw(TS_MOVE_LABEL, SV_GRID_X - 5, SV_PANEL_Y + SV_PANEL_H - 26, desc, &d);
  }
}

static void sv_draw(void) {
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_rect(0, 0, 320, 240, RR_RGBA(0x20, 0x20, 0x28, 255));
  sv_draw_header();
  if (!sv_cmd_shown()) return;
  sv_draw_commands();
  if (B.items_on) sv_draw_items();
}

static void sv_items_open(void) {
  B.items_on = true;
  B.bar_sel = bar_n() > 0 ? 0 : -1;
  B.bar_open = false;
}
static void sv_items_close(void) {
  B.items_on = false;
  B.bar_sel = -1;
  B.bar_open = false;
}
/* the panel's keys: the D-pad moves on the grid, L / R switch the bar, B closes; anything else does nothing (START still opens the menu) */
static bool sv_items_input(ui_button b) {
  const int n = bar_n();
  switch (b) {
    case BTN_LEFT:
    case BTN_RIGHT:
    case BTN_UP:
    case BTN_DOWN: {
      const int d = b == BTN_LEFT ? -1 : b == BTN_RIGHT ? 1 : b == BTN_UP ? -SV_ITEM_COLS : SV_ITEM_COLS;
      const int i = B.bar_sel + d;
      if (B.bar_sel < 0 || i < 0 || i >= n) return false;
      B.bar_sel = i;
      ui_play_select();
      return true;
    }
    case BTN_CYCLE_FORM:
    case BTN_CYCLE_SHINY:
      B.bar ^= 1;
      B.bar_sel = bar_n() > 0 ? 0 : -1;
      ui_play_select();
      return true;
    case BTN_CANCEL:
      sv_items_close();
      ui_play_select();
      return true;
    default:
      return false;
  }
}
/* The D-pad on the buttons by where they stand: FIGHT's neighbours are the Tera button (or Ball) on the left, Pokemon on the right, Run under it
 * and the item panel above it; Ball and Pokemon go to Run sideways and to FIGHT (Ball to the Tera button) upwards. -2 = open the items, -1 = no
 * move. */
static int sv_nav(int c, ui_button b) {
  const bool t = B.opts.can_tera;
  switch (c) {
    case 0: return b == BTN_UP ? -2 : b == BTN_LEFT ? (t ? 4 : 1) : b == BTN_RIGHT ? 2 : b == BTN_DOWN ? 3 : -1;
    case 1: return b == BTN_UP ? (t ? 4 : 0) : b == BTN_RIGHT || b == BTN_DOWN ? 3 : -1;
    case 2: return b == BTN_UP ? 0 : b == BTN_LEFT || b == BTN_DOWN ? 3 : -1;
    case 3: return b == BTN_UP ? 0 : b == BTN_LEFT ? 1 : b == BTN_RIGHT ? 2 : -1;
    case 4: return b == BTN_RIGHT ? 0 : b == BTN_DOWN ? 1 : -1;
    default: return -1;
  }
}
static bool bv_input(ui_screen* s, ui_button b);
/* A tap on the buttons is A on them (as on the strip's command window); on the panel a tap points at an item, on Items or "B: close" closes it */
static bool sv_touch(float x, float y) {
  if (B.items_on) {
    if (sv_hex_hit(&SV_ITEMS, x, y)) return sv_items_input(BTN_CANCEL);
    if (y >= SV_TITLE_Y - 4 && y < SV_TITLE_Y + 12 && x >= SV_HINT_X - 6) {
      const int h = hint_at(TS_MOVE_LABEL, SV_HINT_X, SV_HINTS, 2, x);
      return h == 0 ? sv_items_input(BTN_CYCLE_SHINY) : h == 1 ? sv_items_input(BTN_CANCEL) : false;
    }
    if (x >= SV_GRID_X && y >= SV_GRID_Y && x < SV_GRID_X + SV_ITEM_COLS * 26) {
      const int i = (int)((y - SV_GRID_Y) / 26) * SV_ITEM_COLS + (int)((x - SV_GRID_X) / 26);
      if (i < bar_n() && i != B.bar_sel) {
        B.bar_sel = i;
        ui_play_select();
        return true;
      }
    }
    return false;
  }
  if (sv_hex_hit(&SV_ITEMS, x, y)) {
    sv_items_open();
    ui_play_select();
    return true;
  }
  if (sv_hex_hit(&SV_SETTINGS, x, y)) return bv_input(NULL, BTN_MENU);
  if (B.opts.can_tera && fabsf(x - SV_TERA_X) < 18 && fabsf(y - SV_TERA_Y) < 16) {
    B.cmd_cursor = 4;
    return bv_input(NULL, BTN_ACTION);
  }
  for (int c = 0; c < 4; c++)
    if (sv_hex_hit(&SV_CMD[c], x, y)) {
      B.cmd_cursor = c;
      return bv_input(NULL, BTN_ACTION);
    }
  return false;
}

static void draw_debug(void) {
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  rr_stats st = rr_get_stats();
  char d[160];
  snprintf(d, sizeof d, "%.0f fps  cpu %.1f  gpu %.1f ms  tex %.1f  free %.1f MB", B.fps, st.cpu_ms, st.gpu_ms,
           st.tex_bytes / 1048576.0, st.linear_free / 1048576.0);
  rr_text_sys(2, 2, 0.4f, d, RR_RGBA(0x90, 0x90, 0x98, 255));
  snprintf(d, sizeof d, "heap free %.1f MB  sprite fails %d", st.heap_free / 1048576.0, g_sprite_fails);
  if (g_sprite_fails) {
    const size_t n = strlen(d);
    snprintf(d + n, sizeof d - n, " (last #%d %s)", g_sprite_fail_species, g_sprite_fail_back ? "back" : "front");
  }
  rr_text_sys(2, 12, 0.4f, d, RR_RGBA(0x90, 0x90, 0x98, 255));
  ba_debug_text(d, sizeof d);
  rr_text_sys(2, 22, 0.4f, d, RR_RGBA(0x90, 0x90, 0x98, 255));
}

/* What the bottom screen shows besides the strip: in the debug view the debug rows, and in both what X and Y do now, on one line from the left
 * edge (lorenzo, 2026-11-17: the two labels stacked at the right edge were "out of place"): under the debug rows in the debug view, at the top in
 * the standard one. The target choice's tiles start below it in both views. */
#define BV_KEYS_X 8.0f
#define BV_KEYS_GAP 18.0f
static float bv_keys_y(void) { return g_view == BV_VIEW_DEBUG ? 34.0f : sv_cmd_shown() ? SV_KEYS_CMD_Y : SV_KEYS_Y; }
static void draw_bottom_extras(void) {
  if (!ui_space(UI_PHYS_BOTTOM)) return;
  if (g_view == BV_VIEW_DEBUG) draw_debug();
  char x_label[48], y_label[80], name[40];
  snprintf(x_label, sizeof x_label, "X  Screen: %s", g_view == BV_VIEW_DEBUG ? "Debug" : "Standard");
  text_draw(TS_MOVE_LABEL, BV_KEYS_X, bv_keys_y(), x_label, NULL);
  if (speed_hook) {
    i18n_t(name, sizeof name, speed_hook(false), NULL);
    snprintf(y_label, sizeof y_label, "Y  Speed: %s", name);
    text_draw(TS_MOVE_LABEL, BV_KEYS_X + text_width(TS_MOVE_LABEL, x_label, NULL) + BV_KEYS_GAP, bv_keys_y(), y_label, NULL);
  }
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

/* QuietFormChangePhase.playFormChangeTween (the final boss's Eternamax): the Pokemon turns white over 1000 ms (Cubic.easeIn, the Transform
 * sound at the start), the form changes, the old white shape shrinks to a point (delay 250, 500 ms, Cubic.easeInOut) as the new one grows from a
 * point to its size (500 ms) and the white fades away (delay 250, 1000 ms, Cubic.easeOut). Upstream shrinks and grows at the same time with two
 * sprites; one sprite is in texture memory here, so one follows the other. The silhouette is the catch's: black multiplied, the white added. */
enum { FORM_FADE_IN = 1000, FORM_HOLD = 250, FORM_SHRINK = 500, FORM_GROW = 500, FORM_FADE_OUT = 1000 };
static bool step_quiet_form(const bv_event* e, side* sd, bool player) {
  const float t = B.t;
  const float t_shrink = FORM_FADE_IN + FORM_HOLD, t_grow = t_shrink + FORM_SHRINK, t_out = t_grow + FORM_GROW + FORM_HOLD;
  if (B.step == 0) {
    update_mon_form(&sd->mon, e->a, e->b);   /* its data now; the picture changes at the swap */
    if (e->c) {
      sd->mon.gender = e->c - 2;
      sd->mon.female = sd->mon.gender == 1;
    }
    update_field_scale(false);
    ui_play_sound("battle_anims/PRSFX- Transform");
    B.form_scale = sd->scale;
    sd->catch_on = true;
    sd->catch_rgb[0] = sd->catch_rgb[1] = sd->catch_rgb[2] = 255;
    sd->catch_untint = 1;
    B.step = 1;
  }
  const float target = get_mon_scale(&sd->mon);
  if (t < FORM_FADE_IN) {
    sd->catch_untint = 1 - seg(t, 0, FORM_FADE_IN, EASE_CUBIC_IN);
  } else if (t < t_grow) {
    sd->catch_untint = 0;
    const float k = seg(t, t_shrink, FORM_SHRINK, EASE_CUBIC_IN_OUT);
    sd->scale = B.form_scale + (0.01f - B.form_scale) * k;
  } else if (t < t_out) {
    if (B.step == 1) {   /* the swap, at the point: the new picture's load happens while nothing shows */
      load_mon_sprite(sd, player);
      sd->scale = 0.01f;
      B.step = 2;
    }
    sd->catch_untint = 0;
    sd->scale = 0.01f + (target - 0.01f) * seg(t, t_grow, FORM_GROW, EASE_CUBIC_IN_OUT);
  } else if (t < t_out + FORM_FADE_OUT) {
    sd->scale = target;
    sd->catch_untint = seg(t, t_out, FORM_FADE_OUT, EASE_CUBIC_OUT);
  } else {
    sd->scale = target;
    sd->catch_untint = 1;
    sd->catch_on = false;
    return true;
  }
  return false;
}

/* PokemonAnimPhase's Substitute animations at time t (ms). Returns true while running. */
static bool substitute_anim(side* sd, int type, float t) {
  bool player = field_of(sd)<2;
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

/* getCharVariantFromDialogue: the first @c{expression} of a dialogue, "neutral" without one */
static void first_variant(const char* text, char* out, size_t n) {
  const char* c = text ? strstr(text, "@c{") : NULL;
  const char* end = c ? strchr(c + 3, '}') : NULL;
  snprintf(out, n, "%.*s", end ? (int)(end - (c + 3)) : 7, end ? c + 3 : "neutral");
}

/* ui.showDialogue finishes its callback only after the prompted pages (and
 * optional portrait) are dismissed. Shared by all dialogue owners. */
static bool step_dialogue(const bv_event *e, float dt) {
  /* EncounterPhase / TrainerVictoryPhase: with a portrait, showFieldOverlay(500), the portrait slides in (750), the dialogue is read
   * a page at a time, the portrait goes out (750) and hideFieldOverlay(250); without one, the dialogue alone */
  const bool portrait = e->atlas != NULL;
  if (B.step == 0) {
    B.speaker = NULL;
    if (portrait) {
      tween_start(&B.overlay_tw, B.overlay, 0.5f, 500, EASE_SINE_OUT);
      B.step = 1;
    } else {
      B.step = 3;
    }
  }
  if (B.step == 1) {
    tween_update(&B.overlay_tw, dt);
    B.overlay = B.overlay_tw.value;
    if (tween_done(&B.overlay_tw)) {
      char variant[32];
      first_variant(e->text, variant, sizeof variant);
      chs_show(e->atlas, variant); /* showCharacter(...).then(showDialogue) */
      B.step = 2;
    }
  }
  if (B.step == 2 && !chs_busy()) B.step = 3;
  if (B.step == 3) {
    B.speaker = e->name && *e->name ? e->name : NULL;
    msgbox_show(&B.msg, e->text ? e->text : "", -1, true);
    B.msg.wait_ms = 0;
    B.step = 4;
  }
  if (B.step == 4) {
    const char* v = msgbox_take_variant(&B.msg); /* MessageUiHandler: charSprite.setVariant when the typing passes @c{} */
    if (v && portrait) chs_set_variant(v);
    if (msgbox_done(&B.msg)) {
      B.speaker = NULL; /* hideNameText */
      if (!portrait) {
        return true;
      }
      chs_hide();
      B.step = 5;
    }
  }
  if (B.step == 5 && !chs_busy()) {
    chs_reset();
    tween_start(&B.overlay_tw, B.overlay, 0, 250, EASE_CUBIC_IN);
    B.step = 6;
  }
  if (B.step == 6) {
    tween_update(&B.overlay_tw, dt);
    B.overlay = B.overlay_tw.value;
    if (tween_done(&B.overlay_tw)) return true;
  }
  return false;
}


/* ------------------------------------------------------------------ Weird Dream's transformation */
/* doShowDreamBackground / doPokemonTransformationSequence / doHideDreamBackground (weird-dream-encounter.ts, encounter-transformation-sequence.ts):
 * a container over the whole canvas fades in, each party member's old Pokemon fades up in its middle, goes white, cycles with the new one's
 * silhouette and turns into it; the container fades away. BEV_DREAM_ADD hands over one pair, BEV_DREAM_PLAY plays them all (dream_anim.h has
 * the timeline). The background video (evo_bg.mp4) is not in the packs: a dark gradient with drifting stars stands in for it. Each pair's
 * sprite sheets are loaded only while they show, so a party of six does not hold twelve of them. */
typedef struct {
  bv_mon old_m, new_m;
  menu_sprite old_s, new_s;
  bool old_on, new_on;       /* the sheet is loaded */
  bool old_done, new_done;   /* it was loaded and freed again, or it failed: do not try again */
} dream_mon;

static struct {
  bool on;                   /* the container exists */
  int n;                     /* pairs handed over */
  dream_mon m[DA_MAX];
  float start[DA_MAX];       /* when each sequence begins (ms into the dream) */
  int pos[DA_MAX];
  float all_done;            /* every sequence has resolved */
  float t, t_hide;           /* ms since the dream began; since the fade-out began (< 0 = not yet) */
  bool text;                 /* the cutscene text is up and the play waits for it to be dismissed */
} D;

static void dream_load(menu_sprite* sp, const bv_mon* m, bool* ok) {
  menu_mon visual = {.species = m->species, .form = m->form, .gender = m->gender, .shiny = m->shiny, .variant = m->variant};
  *ok = menu_sprite_load(sp, &visual, false);   /* upstream's getSpriteKey(true): the front sprite */
  if (!*ok && ba_release_graphics()) *ok = menu_sprite_load(sp, &visual, false);
}

static void dream_reset(void) {
  for (int i = 0; i < D.n && i < DA_MAX; i++) {
    menu_sprite_free(&D.m[i].old_s);
    menu_sprite_free(&D.m[i].new_s);
  }
  memset(&D, 0, sizeof D);
  D.t_hide = -1;
}

static void dream_update(float dt) {
  if (!D.on) return;
  D.t += dt;
  if (D.t_hide >= 0 && D.t >= D.t_hide + DA_FADE_MS) {   /* the container is removed */
    dream_reset();
    return;
  }
  for (int i = 0; i < D.n; i++) {
    dream_mon* d = &D.m[i];
    const float local = D.t - D.start[i];
    if (local < 0) continue;
    if (local < da_old_free_ms()) {
      if (!d->old_on && !d->old_done) {
        dream_load(&d->old_s, &d->old_m, &d->old_on);
        d->old_done = !d->old_on;
      }
    } else if (d->old_on) {
      menu_sprite_free(&d->old_s);
      d->old_on = false;
      d->old_done = true;
    }
    if (local >= da_new_load_ms() && local < da_end_ms()) {
      if (!d->new_on && !d->new_done) {
        dream_load(&d->new_s, &d->new_m, &d->new_on);
        d->new_done = !d->new_on;
      }
    } else if (local >= da_end_ms() && d->new_on) {
      menu_sprite_free(&d->new_s);
      d->new_on = false;
      d->new_done = true;
    }
  }
}

/* One of the four sprites of a sequence; silhouette = filled white, whatever its colours. A sheet that did not load draws nothing. */
static void dream_sprite(const menu_sprite* sp, bool loaded, float cx, float cy, float scale, float alpha, bool silhouette) {
  if (!loaded || alpha <= 0 || scale <= 0) return;
  const rr_frame* f = menu_sprite_frame(sp, ui_now());
  if (!f) return;
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = o.origin_y = 0.5f;
  o.scale_x = o.scale_y = scale;
  const int a = (int)(alpha * 255 + 0.5f);
  if (silhouette) {
    o.tint = RR_RGBA(0, 0, 0, a);
    rr_set_tone(255, 255, 255);
  } else {
    o.tint = RR_RGBA(255, 255, 255, a);
  }
  rr_draw(f, cx, cy, &o);
  if (silhouette) rr_set_tone(0, 0, 0);
}

/* the stand-in for evo_bg: a dark violet gradient and stars rising through it, over the whole 400x240 top screen (the field space starts at -40, -30) */
static void dream_background(float ca) {
  static const uint8_t TOP[3] = {4, 6, 22}, BOTTOM[3] = {40, 14, 66};
  enum { BANDS = 9, STARS = 48 };
  for (int b = 0; b < BANDS; b++) {
    const float k = (float)b / (BANDS - 1);
    const int r = (int)(TOP[0] + (BOTTOM[0] - TOP[0]) * k), g = (int)(TOP[1] + (BOTTOM[1] - TOP[1]) * k), bl = (int)(TOP[2] + (BOTTOM[2] - TOP[2]) * k);
    const float y0 = -30.0f + 240.0f * b / BANDS, y1 = -30.0f + 240.0f * (b + 1) / BANDS;
    rr_rect(-40, y0, 400, y1 - y0 + 0.5f, RR_RGBA(r, g, bl, (int)(255 * ca + 0.5f)));
  }
  for (int i = 0; i < STARS; i++) {
    const uint32_t h = (uint32_t)(i + 1) * 2654435761u;
    const float x = (float)((h >> 8) % 400) - 40, speed = 8 + (float)((h >> 4) % 24);   /* px per second */
    float y = fmodf((float)((h >> 12) % 240) - D.t * speed / 1000.0f, 240.0f);
    if (y < 0) y += 240;
    y -= 30;
    const float tw = 0.55f + 0.45f * sinf(D.t / 400.0f + (float)i);
    const int size = (h & 3) == 0 ? 2 : 1;
    rr_rect(x, y, size, size, RR_RGBA(190 + (h & 0x3f), 200 + ((h >> 3) & 0x2f), 255, (int)(255 * ca * tw + 0.5f)));
  }
}

static void dream_draw(void) {
  if (!D.on) return;
  const float ca = da_container_alpha(D.t, D.t_hide);
  if (ca <= 0) return;
  rr_rect(-40, -30, 400, 240, RR_RGBA(0, 0, 0, (int)(255 * ca + 0.5f)));   /* the whole top screen, bands included */
  dream_background(ca);
  const bool have_spark = img_has("images/effects/evo_sparkle");
  for (int i = 0; i < D.n; i++) {
    const dream_mon* d = &D.m[i];
    const float local = D.t - D.start[i];
    if (local < 0) continue;
    float xo, yo;
    da_offset(D.pos[i], &xo, &yo);
    const float cx = 160 + xo, cy = 90 + yo;
    da_state st;
    da_state_at(local, &st);
    if (st.old_vis) dream_sprite(&d->old_s, d->old_on, cx, cy, 1, st.old_alpha * ca, false);
    if (st.otint_vis) dream_sprite(&d->old_s, d->old_on, cx, cy, st.otint_scale, st.otint_alpha * ca, true);
    if (st.new_vis) dream_sprite(&d->new_s, d->new_on, cx, cy, 1, st.new_alpha * ca, false);
    if (st.ntint_vis) dream_sprite(&d->new_s, d->new_on, cx, cy, st.ntint_scale, st.ntint_alpha * ca, true);
    static da_particle ps[DA_MAX_PARTICLES];
    const int n = da_particles(local, D.pos[i], ps, DA_MAX_PARTICLES);
    for (int k = 0; k < n; k++) {
      if (have_spark) {
        const rr_frame* f = img_frame("images/effects/evo_sparkle", NULL);
        if (!f) break;
        rr_draw_opts o = RR_DEFAULT_OPTS;
        o.origin_x = o.origin_y = 0.5f;
        o.scale_x = o.scale_y = ps[k].scale;
        o.tint = RR_RGBA(255, 255, 255, (int)(255 * ca + 0.5f));
        rr_draw(f, ps[k].x, ps[k].y, &o);
      } else {
        const float sz = 5 * ps[k].scale;
        rr_rect(ps[k].x - sz / 2, ps[k].y - sz / 2, sz, sz, RR_RGBA(255, 255, 255, (int)(255 * ca + 0.5f)));
      }
    }
  }
}

static void run_event(float dt) {
  bv_event* e = &B.ev[B.cur];
  side* sd = side_of(e->battler);
  bool player = e->battler < 2;
  B.t += dt;
  switch (e->type) {
    case BEV_FIELD_SWAP: {
      side tmp=B.s[0];B.s[0]=B.s[2];B.s[2]=tmp;
      if(player_doubles())B.flipped=!B.flipped;   /* in two columns each Pokemon keeps its place: only BEV_PLAYER_CENTRE moves it */
      finish_event();break;
    }
    case BEV_DOUBLES:
      if (step_doubles(dt)) finish_event();
      break;
    case BEV_PLAYER_CENTRE:
      if (step_centre(dt, e->a != 0, e->b != 0)) finish_event();
      break;
    case BEV_MESSAGE:
      if (B.step == 0) {
        msgbox_show(&B.msg, e->text ? e->text : "", -1, e->prompt);
        B.msg.wait_ms = e->a ? e->a : e->prompt ? 0 : 1500; /* callbackDelay || (prompt ? 0 : 1500) */
        B.step = 1;
      }
      if (msgbox_done(&B.msg)) finish_event();
      break;

    case BEV_DIALOGUE:
      if (step_dialogue(e, dt)) finish_event();
      break;

    case BEV_DREAM_ADD:
      /* one pair: the new Pokemon is `mon`, the old one is a = species, b = form, c = female | shiny << 1 | variant << 2 */
      if (e->mon && D.n < DA_MAX) {
        if (D.on) dream_reset();   /* a dream that was still fading: the next one begins anew */
        dream_mon* d = &D.m[D.n++];
        memset(d, 0, sizeof *d);
        d->new_m = *e->mon;
        d->old_m = *e->mon;
        d->old_m.species = e->a;
        d->old_m.form = e->b;
        d->old_m.female = e->c & 1;
        d->old_m.gender = d->old_m.female ? 1 : 0;
        d->old_m.shiny = (e->c & 2) != 0;
        d->old_m.variant = (e->c >> 2) & 3;
        d->old_m.fusion_species = (e->c >> 4) & 0x3fff;   /* a fused old Pokemon is shown fused */
        d->old_m.fusion_form = (e->c >> 18) & 63;
        d->old_m.fusion_gender = ((e->c >> 24) & 3) - 1;
        d->old_m.fusion_shiny = (e->c >> 26) & 1;
        d->old_m.fusion_variant = (e->c >> 27) & 3;
        if (!d->old_m.fusion_species) d->old_m.fusion_gender = 0;
        D.t_hide = -1;
      }
      finish_event();
      break;

    case BEV_DREAM_PLAY:
      if (B.step == 0) {
        if (D.n == 0) {
          finish_event();
          break;
        }
        D.on = true;
        D.t = 0;
        D.t_hide = -1;
        D.all_done = da_schedule(D.n, D.start, D.pos);
        /* showEncounterText(cutscene) is started and not awaited: it types out and waits for a button while the sequences play */
        if (e->text && *e->text) {
          msgbox_show(&B.msg, e->text, -1, e->prompt);
          B.msg.wait_ms = 0;
          D.text = true;
        }
        B.step = 1;
      }
      /* then Promise.all of the sequences and `await cutsceneDialoguePromise`: the background starts to fade (the next text comes with it) */
      if (D.t >= D.all_done && (!D.text || msgbox_done(&B.msg))) {
        D.t_hide = D.t;
        D.text = false;
        finish_event();
      }
      break;

    case BEV_MUSIC:
      if (music_hook) music_hook(e->a);
      finish_event();
      break;

    case BEV_HEAVEN: {
      /* lorenzo 2026-11-16. C-Moon (a = 1): he slides back in with the C-Moon look and speaks. Made in Heaven (a = 2): he slides back in still
       * as C-Moon and introduces his new Stand; the field goes dark, the arena becomes space and his look becomes Made in Heaven, it comes
       * back, and only then he names it. "$" starts a new box. */
      const bool mih = e->a >= 2;
      if (B.heaven.stage == 0) {               /* he slides back in (BEV_TRAINER_SHOW's 750 ms) */
        heaven_sprite(1);
        B.trainer_visible = true;
        B.trainer_show_ms = 1;
        B.heaven.stage = 1;
      }
      if (B.heaven.stage == 1) {
        B.trainer_show_ms += dt;
        if (B.trainer_show_ms < 750) break;
        B.trainer_show_ms = 0;
        B.step = 0;
        B.heaven.stage = 2;
      }
      const char* who = B.setup.trainer_name[0] ? B.setup.trainer_name : NULL;
      if (B.heaven.stage == 2) {
        const bv_event d = {.type = BEV_DIALOGUE, .battler = -1, .name = who,
                            .text = mih ? "Allow me to introduce you to my new Stand!$My gift to humanity! I shall call my Stand..."
                                        : "59, 61, 67...$Damn it, the filthy frogs are clinging to my 800 dollar pants$C-MOON!"};
        if (!step_dialogue(&d, dt)) break;
        if (!mih) { B.heaven.stage = 0; finish_event(); break; }
        B.heaven.stage = 3;
        B.heaven.t = 0;
        B.heaven.swapped = false;
      }
      if (B.heaven.stage == 3) {               /* dark (500 ms), space and Made in Heaven, back (500 ms) */
        B.heaven.t += dt;
        B.heaven_dark = B.heaven.t < 500 ? B.heaven.t / 500 : B.heaven.t < 1000 ? 1 - (B.heaven.t - 500) / 500 : 0;
        if (B.heaven.t >= 500 && !B.heaven.swapped) {
          snprintf(B.biome, sizeof B.biome, "space");
          heaven_sprite(2);
          B.heaven.swapped = true;
        }
        if (B.heaven.t < 1000) break;
        B.heaven_dark = 0;
        B.step = 0;
        B.heaven.stage = 4;
      }
      if (B.heaven.stage == 4) {
        const bv_event d = {.type = BEV_DIALOGUE, .battler = -1, .name = who, .text = "MADE IN HEAVEN"};
        if (!step_dialogue(&d, dt)) break;
        B.heaven.stage = 0;
        finish_event();
      }
      break;
    }

    case BEV_WORLD: {
      /* lorenzo 2026-11-16. a = 1: DIO's second sprite slides in beside his Pokemon (750 ms), "THE WORLD!", the inverted circle grows from
       * him over the field while the background swells (700 ms), then the inversion snaps off and the colour drains to grey blue (300 ms).
       * a = 0: "Time has begun to move again.", he slides out while the colour comes back (750 ms). */
      const char* who = B.setup.trainer_name[0] ? B.setup.trainer_name : NULL;
      if (e->a == 1) {
        if (B.world.stage == 0) {
          world_key();
          B.world.on = true;
          B.world.slide = 0;
          B.world.t = 0;
          B.world.stage = 1;
        }
        if (B.world.stage == 1) {
          B.world.t += dt;
          B.world.slide = ease(EASE_CUBIC_OUT, fminf(B.world.t / 750, 1));
          if (B.world.t < 750) break;
          B.step = 0;
          B.world.stage = 2;
        }
        if (B.world.stage == 2) {
          const bv_event d = {.type = BEV_DIALOGUE, .battler = -1, .name = who, .text = "THE WORLD!"};
          if (!step_dialogue(&d, dt)) break;
          B.world.t = 0;
          B.world.stage = 3;
          ui_play_sound(WORLD_SOUND);
          B.world.hold = fminf(fmaxf(ui_sound_seconds(WORLD_SOUND) * 1000 - WORLD_GROW_MS, 0), WORLD_HOLD_MAX_MS);
        }
        if (B.world.stage == 3) {
          B.world.t += dt;
          const float k = fminf(B.world.t / WORLD_GROW_MS, 1);
          B.world.inv_r = WORLD_MAX_R * ease(EASE_SINE_IN, k);
          B.world.bulge = WORLD_BULGE * sinf(3.14159265f * k);
          if (B.world.t < WORLD_GROW_MS) break;
          B.world.inv_r = 0;
          B.world.bulge = 0;
          B.world.t = 0;
          B.world.stage = 4;
        }
        if (B.world.stage == 4) {
          B.world.t += dt;
          B.world.grey = fminf(B.world.t / WORLD_DRAIN_MS, 1);
          if (B.world.t < fmaxf(WORLD_DRAIN_MS, B.world.hold)) break;
          B.world.stage = 0;
          finish_event();
        }
        break;
      }
      if (B.world.stage == 0) {
        B.step = 0;
        B.world.stage = 1;
      }
      if (B.world.stage == 1) {
        const bv_event d = {.type = BEV_DIALOGUE, .battler = -1, .name = who, .text = "Time has begun to move again."};
        if (!step_dialogue(&d, dt)) break;
        B.world.t = 0;
        B.world.stage = 2;
        ui_play_sound(WORLD_RESUME_SOUND);
      }
      if (B.world.stage == 2) {
        B.world.t += dt;
        const float k = fminf(B.world.t / 750, 1);
        B.world.slide = 1 - ease(EASE_CUBIC_IN, k);
        if (B.world.grey > 0) B.world.grey = 1 - k;
        if (B.world.t < 750) break;
        B.world.on = false;
        B.world.grey = 0;
        B.world.stage = 0;
        finish_event();
      }
      break;
    }

    case BEV_PERFECT: {
      /* lorenzo 2026-11-16. a = 1: Kars's second sprite slides in beside his Pokemon (750 ms) and stays, then his line; a = 2, 3: the
       * lines one after the other (stage 1 + n = line n). Each line is a plain message, 1.5 s or a press. */
      if (e->a == 1 && !B.perfect.on) {
        perfect_key();
        B.perfect.on = true;
        B.perfect.slide = 0;
        B.perfect.t = 0;
        B.perfect.stage = -1;
      }
      if (B.perfect.stage == -1) {
        B.perfect.t += dt;
        B.perfect.slide = ease(EASE_CUBIC_OUT, fminf(B.perfect.t / 750, 1));
        if (B.perfect.t < 750) break;
        B.perfect.stage = 0;
        B.step = 0;
      }
      for (;;) {
        if (B.step == 0) {
          perfect_line(e->a, B.perfect.stage, e->battler, B.perfect.line, sizeof B.perfect.line);
          if (!B.perfect.line[0]) {
            B.perfect.stage = 0;
            finish_event();
            break;
          }
          msgbox_show(&B.msg, B.perfect.line, -1, false);
          B.msg.wait_ms = 1500;
          B.step = 1;
        }
        if (!msgbox_done(&B.msg)) break;
        B.perfect.stage++;
        B.step = 0;
      }
      break;
    }

    case BEV_TRAINER_SHOW:
      /* showEnemyTrainer: the sprite is back on the platform and moves in from 16 px up and right, fading in, over 750 ms */
      if (B.step == 0 && B.perfect.on) {
        /* Kars comes back as the Ultimate Life Form: his second sprite moves onto the platform (lorenzo's JoJo content) */
        if (B.perfect.key[0]) snprintf(B.setup.trainer_keys[0], sizeof B.setup.trainer_keys[0], "%s", B.perfect.key);
        B.perfect.on = false;
      }
      if (B.step == 0) {
        B.trainer_visible = B.setup.trainer;
        B.trainer_show_ms = 1;
        B.step = 1;
      }
      B.trainer_show_ms += dt;
      if (B.trainer_show_ms >= 750) {
        B.trainer_show_ms = 0;
        finish_event();
      }
      break;

    case BEV_IVS:
      if (B.step == 0) {
        msgbox_clear(&B.msg);
        B.lvl.on = e->stats != NULL;
        B.lvl.ivs = true;
        B.lvl.stats = e->stats;
        B.step = 1;
      }
      if (!B.lvl.on) finish_event();
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
        B.lvl.ivs = false;
        B.lvl.totals = false;
        B.lvl.stats = e->stats;
        B.step = 2;
      }
      if (B.step == 2 && !B.lvl.on) finish_event();
      break;

    case BEV_INTRO_WILD: {
      /* a = 1: a trainer battle, nobody on the enemy side yet and `text` is the message
       * b = 1: (trainer) the slide only. b = 2: (the Classic final boss) the slide, then `text` with a button, the boss still a silhouette;
       * b = 3: the boss shows itself (untint, cry, its box) once the speech that follows the message has been read;
       * b = 4: (a = 0) a custom rematch's offer: the Nemesis shows itself as a wild Pokemon does, and `text` is the message */
      const bool trainer = e->a == 1;
      if (e->b == 3) {
        side* en = &B.s[1];
        if (B.step == 0) {
          en->black = false;
          en->info_on = true;
          en->info_dx = -info_slide(en, false);
          update_field_scale(false);
          cry(&en->mon);
          tween_start(&B.tw2, en->info_dx, 0, 1000, EASE_CUBIC_OUT);
          B.step = 2;
        }
        tween_update(&B.tw2, dt);
        en->info_dx = B.tw2.value;
        if (tween_done(&B.tw2)) finish_event();
        break;
      }
      if (B.step == 0) {
        side* en = &B.s[1];
        en->present = !trainer && en->mon.species > 0;
        en->black = en->present;
        en->info_on = false;
        side *ally=&B.s[3];ally->present=!trainer&&ally->mon.species>0;
        ally->black=ally->present;ally->info_on=false;
        tween_start(&B.tw, 0, 300, 2000, EASE_LINEAR);
        B.step = 1;
      }
      if (B.step == 1) {
        tween_update(&B.tw, dt);
        if (!B.setup.player_present) B.arena_player_x = 300 - B.tw.value; /* a run's first wave: the platform arrives under nobody yet */
        B.arena_enemy_x = -280 + B.tw.value;
        if (B.arena_old_on) B.arena_old_x = 20 + B.tw.value;
        B.enemy_slide = -300 + B.tw.value;
        if (tween_done(&B.tw) && trainer && e->b == 1) {
          B.arena_old_on = false; /* the dialogue and the message are events of their own */
          finish_event();
          break;
        }
        if (tween_done(&B.tw) && e->b == 2) {
          B.arena_old_on = false;
          msgbox_show(&B.msg, e->text ? e->text : "", -1, true);   /* showText(message, null, cb, 1500, true) */
          B.msg.wait_ms = 1500;
          B.step = 4;
          break;
        }
        if (tween_done(&B.tw)) {
          B.arena_old_on = false;
          side* en = &B.s[1];
          char s[256];
          if (trainer) {
            snprintf(s, sizeof s, "%s", e->text ? e->text : "");
            tween_start(&B.tw2, 0, 0, 0, EASE_LINEAR);
          } else {
            en->black = false; /* untint(100) */
            if(B.s[3].present){B.s[3].black=false;B.s[3].info_on=true;B.s[3].info_dx=-info_slide(&B.s[3],false);}
            update_field_scale(false);
            cry(en->present?&en->mon:&B.s[3].mon);
            en->info_on = true;
            en->info_dx = -info_slide(en, false);
            tween_start(&B.tw2, en->info_dx, 0, 1000, EASE_CUBIC_OUT);
            if (e->b == 4) snprintf(s, sizeof s, "%s", e->text ? e->text : "");   /* a rematch's offer: its own line instead of "A wild X appeared!" */
            else if(B.s[3].present&&en->present)i18n_t(s,sizeof s,"battle:doubleWildAppeared","pokemonName1",en->mon.name,"pokemonName2",B.s[3].mon.name,NULL);
            else i18n_t(s, sizeof s, "battle:singleWildAppeared", "pokemonName", en->present?en->mon.name:B.s[3].mon.name, NULL);
          }
          msgbox_show(&B.msg, s, -1, false);
          B.msg.wait_ms = 1500;
          B.step = 2;
        }
      }
      if (B.step == 4) {
        if (msgbox_done(&B.msg)) finish_event();
        break;
      }
      if (B.step == 2) {
        tween_update(&B.tw2, dt);
        if (!trainer){B.s[1].info_dx = B.tw2.value;B.s[3].info_dx=B.tw2.value;}
        if (msgbox_done(&B.msg) && tween_done(&B.tw2)) finish_event();
      }
      break;
    }

    case BEV_SUMMON:
      if (B.step == 0) {
        B.opening_summon=player && B.player_trainer_visible;
        if (B.opening_summon) B.player_throw_ms=0;
        if (!player) B.trainer_visible=false;
        if (e->mon) set_mon(sd, e->mon, player);
        if (player) B.tg.last[field_of(sd) & 1] = -1; /* a Pokemon just sent out starts its target choice fresh (waveTurnCount 1) */
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
      if (B.step == 1 && B.t >= (B.opening_summon ? 1400 : player ? 750 : 250)) {
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
          sd->info_dx = player ? 150 : -info_slide(sd, false);
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
        tween_start(&B.tw2, 0, player ? 150 : -info_slide(sd, false), 500, EASE_CUBIC_IN);
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

    case BEV_SHINY_SPARKLE:
      if(B.step==0){sd->sparkle_ms=1;ui_play_sound("se/sparkle");B.step=1;}
      if(B.t>=1000)finish_event();
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
        const bool heaven = e->a == BV_ABILITY_MADE_IN_HEAVEN;
        snprintf(s, sizeof s, "%s", heaven ? "Made in Heaven" : i18n_ability_name(e->a));
        i18n_t(B.ability_text, sizeof B.ability_text, "fightUiHandler:abilityFlyInText", "pokemonName",
               e->text&&*e->text?e->text:sd->mon.name, "passive", "", "abilityName", s, NULL);
        for (char* p = B.ability_text; *p; p++)
          if (*p == '\n') *p = ' ';
        if (heaven) { B.heaven_box_t = 1500; B.ability_t = 0; }
        else { B.ability_t = 1500; B.heaven_box_t = 0; }
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
        tween_start(&B.tw2, sd->info_dx, sd->info_dx + (player ? 150 : -info_slide(sd, false)), 500, EASE_CUBIC_IN);
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

    case BEV_BOSS_SHIELD:
      sd->mon.boss_segment_index = e->a;
      finish_event();
      break;

    case BEV_TERA:
      sd->mon.terastallized = e->a >= 0;
      if (e->a >= 0) sd->mon.tera_type = e->a;
      finish_event();
      break;

    case BEV_CATCH:
      if (!catch_anim(sd, e, B.t)) finish_event();
      break;

    case BEV_FORM_CHANGE:
    case BEV_TRANSFORM:
    case BEV_ILLUSION:
      if (e->type == BEV_FORM_CHANGE && e->target == 1) {   /* the quiet form change's white tween (the final boss) */
        if (step_quiet_form(e, sd, player)) finish_event();
        break;
      }
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

static bool stats_input(ui_button b){
  if(b!=BTN_STATS)return false;
  B.stats_shown=true;tween_start(&B.stats_tw,B.stats_alpha,1,125,EASE_SINE_IN_OUT);return true;
}
static bool undo_command(ui_button b){
  if(b!=BTN_CANCEL||B.mode!=M_COMMAND||!B.opts.can_undo)return false;
  B.mode=M_IDLE;ui_play_select();
  if(B.on_command)B.on_command((bv_command){4,0},B.command_arg);
  return true;
}

/* The column a place stands in on the field: 0 left, 1 right (a player's Pokemon keeps its column through a mid-turn swap, player_right) */
static int tg_col(int f) { return f >= 2 ? f % 2 : player_right(side_of(f)) ? 1 : 0; }
/* UP / DOWN: the target straight across on the other side when there is one, else that side's first (upstream always takes the first, which from
 * the right-hand ally is the foe on the left: lorenzo, 2026-11-17, "it moves diagonally") */
static int tg_across(int c, bool foes) {
  for (int f = foes ? 2 : 0; f < (foes ? 4 : 2); f++)
    if (B.tg.ok[f] && tg_col(f) == tg_col(c)) return f;
  return tg_first(foes);
}

/* TargetSelectUiHandler.processInput: A and B answer (and are remembered for the slot's next turn), the D-pad moves by place */
static bool target_input(ui_button b) {
  const int c = B.tg.cursor;
  int to = -1;
  switch (b) {
    case BTN_ACTION:
    case BTN_CANCEL: {
      B.tg.last[B.tg.slot & 1] = c;
      ui_play_select();
      B.mode = M_IDLE;
      void (*fn)(int, void*) = B.tg.on_target;
      B.tg.on_target = NULL;
      if (fn) fn(b == BTN_ACTION ? c : -1, B.tg.arg);
      return true;
    }
    case BTN_UP:
      if (c < 2) to = tg_across(c, true);
      break;
    case BTN_DOWN:
      if (c >= 2) to = tg_across(c, false);
      break;
    case BTN_LEFT:
      if (c % 2 && B.tg.ok[c - 1]) to = c - 1;
      break;
    case BTN_RIGHT:
      if (!(c % 2) && c + 1 < 4 && B.tg.ok[c + 1]) to = c + 1;
      break;
    default:
      return false;
  }
  if (to < 0) return false;
  tg_set(to);
  ui_play_select();
  return true;
}
/* A tap: on a tile it points there, on the pointed tile it picks (the touch rule of the list screens); the strip's hints are their buttons */
static bool target_touch(float x, float y) {
  if (y >= -48) {
    const int h = hint_at(TS_INSTRUCTIONS_TEXT, TG_HINT_X, TG_HINTS, 2, x);
    return h == 0 ? target_input(BTN_ACTION) : h == 1 ? target_input(BTN_CANCEL) : false;
  }
  const int f = tg_tile_at(x, y);
  if (f < 0 || !B.tg.ok[f]) return false;
  if (f == B.tg.cursor) return target_input(BTN_ACTION);
  tg_set(f);
  ui_play_select();
  return true;
}

static bool bv_input(ui_screen* s, ui_button b) {
  (void)s;
  if(stats_input(b))return true;
  if (b == BTN_CYCLE_TERA) { /* X: the next bottom-screen view, in any mode (the events play with it too) */
    if (!g_view_held) {
      g_view_held = true;
      g_view = (bottom_view)((g_view + 1) % BV_VIEW_COUNT);
      sv_items_close(); /* the item bar's cursor and the panel are the view's own */
      ui_play_select();
    }
    return true;
  }
  if (b == BTN_CYCLE_NATURE && speed_hook) { /* Y: the next game speed */
    if (!g_speed_held) {
      g_speed_held = true;
      speed_hook(true);
      ui_play_select();
    }
    return true;
  }
  if (B.mode == M_TARGET) return target_input(b);
  if (B.mode == M_EVENTS && B.lvl.on) {
    /* promptLevelUpStats' onActionInput: the gains, then the totals, then the window closes */
    if (B.lvl.ivs) {
      if (b != BTN_ACTION && b != BTN_CANCEL) return false;
      B.lvl.on = false;
      return true;
    }
    if (b != BTN_ACTION) return false;
    if (!B.lvl.totals) B.lvl.totals = true;
    else B.lvl.on = false;
    return true;
  }
  if (B.mode == M_EVENTS) return msgbox_input(&B.msg, b);
  if (sv() && B.mode == M_COMMAND && B.items_on && b != BTN_MENU) return sv_items_input(b);
  if (sv() && B.mode == M_COMMAND && (b == BTN_CYCLE_FORM || b == BTN_CYCLE_SHINY)) { /* L / R open the item panel */
    sv_items_open();
    ui_play_select();
    return true;
  }
  if (sv() && B.mode == M_FIGHT && (b == BTN_CYCLE_FORM || b == BTN_CYCLE_SHINY)) return false; /* no item bar on screen */
  if ((B.mode == M_COMMAND || B.mode == M_FIGHT) && (b == BTN_CYCLE_FORM || b == BTN_CYCLE_SHINY)) {
    /* L and R switch the item bar; a cursor on the bar goes to the first item of the other one (or off, when that is empty) */
    const bool focused = B.bar_sel >= 0;
    B.bar ^= 1;
    B.bar_sel = focused && bar_n() > 0 ? 0 : -1;
    B.bar_open = false;
    ui_play_select();
    return true;
  }
  if (B.mode == M_COMMAND && B.bar_sel >= 0 && !sv()) {
    /* the cursor is on the item bar: Left/Right move it (an open description follows), A opens its description in the message window
     * (or closes it), B closes the description and then leaves the bar, Up/Down leave it */
    if (b == BTN_LEFT || b == BTN_RIGHT) {
      const int i = B.bar_sel + (b == BTN_RIGHT ? 1 : -1);
      if (i < 0 || i >= bar_n()) return false;
      B.bar_sel = i;
      ui_play_select();
      return true;
    }
    if (b == BTN_ACTION) {
      B.bar_open = !B.bar_open;
      ui_play_select();
      return true;
    }
    if (b == BTN_CANCEL && B.bar_open) {
      B.bar_open = false;
      ui_play_select();
      return true;
    }
    if (b == BTN_UP || b == BTN_DOWN || b == BTN_CANCEL) {
      B.bar_sel = -1;
      B.bar_open = false;
      ui_play_select();
      return true;
    }
    return false;
  }
  if (B.mode == M_COMMAND) {
    if(undo_command(b))return true;
    int c = B.cmd_cursor;
    if (c == 4 && !B.opts.can_tera) c = B.cmd_cursor = 0;
    if (sv() && (b == BTN_UP || b == BTN_DOWN || b == BTN_LEFT || b == BTN_RIGHT)) {
      const int to = sv_nav(c, b);
      if (to == -2) {
        sv_items_open();
        ui_play_select();
        return true;
      }
      if (to < 0) return false;
      B.cmd_cursor = to;
      ui_play_select();
      return true;
    }
    switch (b) {
      case BTN_UP:
        if (c == 4) break;
        if (c >= 2) {
          B.cmd_cursor -= 2;
        } else if (bar_n() > 0) {
          B.bar_sel = 0;
          ui_play_select();
          return true;
        }
        break;
      case BTN_DOWN:
        if (c == 4) break;
        if (c < 2) {
          B.cmd_cursor += 2;
        } else if (bar_n() > 0) { /* the bar under the command window on the top screen */
          B.bar_sel = 0;
          ui_play_select();
          return true;
        }
        break;
      case BTN_LEFT:
        if (c == 4) break;
        if (c % 2) B.cmd_cursor--;
        else if (B.opts.can_tera) B.cmd_cursor = 4; /* FIGHT and POKEMON have the Tera button on their left */
        break;
      case BTN_RIGHT:
        if (c == 4) B.cmd_cursor = 0;
        else if (!(c % 2)) B.cmd_cursor++;
        break;
      case BTN_ACTION:
        ui_play_select();
        if (c == 0 || c == 4) {
          if (B.opts.struggle) {
            /* every move is unusable: FIGHT uses Struggle (upstream's CommandPhase.handleFightCommand); from the Tera button it
             * Terastallizes too */
            B.mode = M_IDLE;
            msgbox_clear(&B.msg);
            if (B.on_command) B.on_command((bv_command){0, 0, c == 4}, B.command_arg);
            return true;
          }
          B.tera_fight = c == 4;
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
          B.hold_cmd = c == 1 || c == 2; /* the screens that open over the battle (and come back to the menu) keep the strip up */
          if (!B.hold_cmd) msgbox_clear(&B.msg);
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
        const bv_mon* m = &command_side()->mon;
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
        if (B.on_command) B.on_command((bv_command){0, c, B.tera_fight}, B.command_arg);
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
  tween_update(&B.stats_tw,dt);B.stats_alpha=B.stats_tw.value;
  (void)s;
  float frame_dt = dt;
  if (B.player_throw_ms>=0 && B.player_throw_ms<1400) {
    B.player_throw_ms=fminf(1400,B.player_throw_ms+dt);
    if (B.player_throw_ms>=1000) B.player_trainer_visible=false;
  }
  msgbox_update(&B.msg, dt);
  chs_update(dt);
  if (B.ability_t > 0) B.ability_t -= dt;
  if (B.heaven_box_t > 0) B.heaven_box_t -= dt;
  if (B.reason_ms > 0) B.reason_ms -= dt;
  if (B.mode == M_TARGET) B.tg.t += dt;
  sv_battery_update(dt);
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
  update_sparkles(dt);
  update_tera(dt);
  dream_update(dt);
  for (int k = 0; k < 4; k++) {
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

/* The 40 px side bands and 30 px top and bottom bands of the 400x240 top screen around upstream's 320x180 field (field_border.h). */
static void draw_border(void) {
  char k[64];
  snprintf(k, sizeof k, "images/arenas/%s_bg", B.biome);
  field_border_draw(k);
}

/* showFieldOverlay: the grey (0x424242) the field is darkened with behind a dialogue's portrait. Upstream's covers its 320x132 above the message
 * strip; here it covers the whole 400x240 top screen, the bands around the field as well (lorenzo, 2026-11-16: "does not fit the whole screen").
 * The message strip and the item bar come after it and are opaque, so the screen looks the same under them as upstream's does. */
static void draw_field_overlay(void) {
  if (B.overlay > 0 && ui_space(UI_PHYS_TOP)) rr_rect(0, 0, 400, 240, RR_RGBA(0x42, 0x42, 0x42, (int)(B.overlay * 255)));
}

static void bv_release(ui_screen *s,ui_button b){
  (void)s;if(b==BTN_STATS){B.stats_shown=false;tween_start(&B.stats_tw,B.stats_alpha,0,125,EASE_SINE_IN_OUT);}
  if(b==BTN_CYCLE_TERA)g_view_held=false;
  if(b==BTN_CYCLE_NATURE)g_speed_held=false;
}

static void bv_draw(ui_screen* s, rr_screen which) {
  (void)s;
  if (which == RR_TOP) {
    /* the field's Pokemon, platforms and animations are drawn bigger than upstream's 320x180 field (FIELD_ZOOM) and run across the
     * border, so the border goes under the field */
    if (ui_space(UI_PHYS_TOP)) draw_border();
    if (ui_space(UI_FIELD)) {
      draw_field();
      if (B.heaven_dark > 0) rr_rect(-40, -30, 400, 240, RR_RGBA(0, 0, 0, (int)(B.heaven_dark * 255)));
    }
    draw_field_overlay();
    if (ui_space(UI_FIELD)) chs_draw();
    if (ui_space(UI_FIELD_UI)) {
      draw_enemy_info(&B.s[1]);
      draw_player_info(&B.s[0]);
      draw_enemy_info(&B.s[3]);
      draw_player_info(&B.s[2]);
    }
    /* the field is 320x180 on a 400x240 screen: keep sprites that slide past it off the border */
    /* the strip and the item bar run across the border (the whole screen's width), so they come after it */
    if (ui_space(UI_TOP_CANVAS)) dream_draw();   /* Weird Dream: over the field and its boxes, under the message strip */
    if (ui_space(UI_TOP_CANVAS_UI)) draw_bottom(true);
    if (ui_space(UI_PHYS_TOP)) draw_top_bar();
    if (ui_space(UI_PHYS_TOP)) draw_heaven_box();
  } else {
    if (ui_space(UI_PHYS_BOTTOM)) rr_rect(0, 0, 320, 240, RR_RGBA(0x10, 0x10, 0x18, 255));
    if (sv()) sv_draw();
    if (!sv_cmd_shown() && ui_space(UI_BOTTOM_UI)) draw_bottom(false);
    if((B.mode==M_COMMAND||B.mode==M_FIGHT)&&ui_space(UI_PHYS_BOTTOM))
      text_draw(TS_MOVE_LABEL,BV_KEYS_X,bv_keys_y()+9,"Hold SELECT: Stat changes",NULL); /* under the X / Y line (it sat on the debug rows) */
    draw_bottom_extras();
  }
}

/* A tap on the bottom screen (physical px). The command and fight menus' cells act as their button does with the cursor on them; in
 * the fight menu a tap above the strip goes back, like B. The item bars are on the top screen: no touch there. */
static bool bv_touch(ui_screen* s, float px, float py) {
  (void)s;
  const float x = px, y = ui_bottom_ui_y(py);
  /* the events play: a tap anywhere is A, for a message that waits for a button and for the level-up window (msgbox_input does
   * nothing while a message is still typing or not waiting) */
  if (B.mode == M_EVENTS) return bv_input(NULL, BTN_ACTION);
  if (B.mode == M_TARGET) return target_touch(x, y);
  if (B.mode == M_COMMAND && sv()) return sv_touch(px, py);
  if (B.mode == M_COMMAND) {
    if (B.opts.can_tera && x >= TERA_BTN_X - 16 && x < TERA_BTN_X + 16 && y >= TERA_BTN_Y - 14 && y < TERA_BTN_Y + 14) {
      B.bar_sel = -1;
      B.bar_open = false;
      B.cmd_cursor = 4; /* a tap on the Tera button is A on it */
      return bv_input(NULL, BTN_ACTION);
    }
    if (x >= 202 && y >= -48) {
      B.bar_sel = -1; /* a tap on a command leaves the item bar (and closes its description) */
      B.bar_open = false;
      B.cmd_cursor = (x < 261 ? 0 : 1) + (y < -24 ? 0 : 2);
      return bv_input(NULL, BTN_ACTION);
    }
    return false;
  }
  if (B.mode == M_FIGHT) {
    if (x < 240 && y >= -48) {
      B.move_cursor = (x < 125 ? 0 : 1) + (y < -24 ? 0 : 2);
      return bv_input(NULL, BTN_ACTION);
    }
    if (y < -48) return bv_input(NULL, BTN_CANCEL);
  }
  return false;
}

static ui_screen g_battle = {"battle", 0, NULL, NULL, NULL, bv_input, bv_update, bv_draw, NULL, bv_touch, bv_release};

void bv_begin(const bv_setup* setup) {
  for(int i=0;i<4;i++)menu_sprite_free(&B.s[i].sprite);
  dream_reset();
  int selected_bar=B.bar;
  memset(&B, 0, sizeof B);
  B.bar=selected_bar;B.bar_sel=-1;
  g_view_held=g_speed_held=false; /* a key still down from the last battle's end has no release to come to this one */
  chs_reset();
  B.setup = *setup;
  gm_enum_lower(GM_E_BIOME_ID, setup->biome, B.biome, sizeof B.biome);
  if (setup->heaven > 0) heaven_sprite(setup->heaven);           /* a reloaded battle: the stage Pucci had reached */
  if (setup->heaven >= 2) snprintf(B.biome, sizeof B.biome, "space");
  set_mon(&B.s[0], &setup->player, true);
  set_mon(&B.s[1], &setup->enemy, false);
  set_mon(&B.s[2],&setup->player_ally,true);
  set_mon(&B.s[3],&setup->enemy_ally,false);
  B.begun = true;
  B.centred = setup->doubles && setup->player_centred;   /* (only a double has a column to leave) */
  B.centre = B.centred ? 1.0f : 0.0f;
  B.arena_player_x = setup->player_present ? 0 : 300;
  B.arena_enemy_x = -280;
  B.enemy_slide = -300;
  B.arena_old_on = setup->next_encounter;
  B.arena_old_x = 20;
  B.trainer_visible=setup->trainer;
  B.player_trainer_visible=!setup->player_present;
  B.player_throw_ms=-1;
  B.s[0].present = B.s[0].info_on = setup->player_present;
  B.s[2].present=B.s[2].info_on=setup->player_ally_present;
  B.s[1].present = B.s[1].info_on = false;
  B.field_scale = 1;
  update_field_scale(true);
  for (int i = 0; i < 4; i++) B.opts.move_ok[i] = true;
  B.opts.can_ball = B.opts.can_switch = B.opts.can_run = true;
  msgbox_init(&B.msg, TS_MESSAGE, 1780 / 6.0f, 2);
  B.mode = M_IDLE;
  ui_set(&g_battle);
}

void bv_set_music_hook(void (*hook)(int a)) { music_hook = hook; }
void bv_set_speed_hook(const char* (*hook)(bool step)) { speed_hook = hook; }

bool bv_player_centred(void) { return B.centred; }

void bv_play(const bv_event* ev, int n, void (*on_done)(void*), void* arg) {
  B.n_ev = 0;
  B.cur = 0;
  B.step = 0;
  B.t = 0;
  B.n_dlg = 0;
  for (int i = 0; i < n && i < EV_MAX; i++) {
    B.ev[i] = ev[i];
    if (ev[i].type == BEV_DIALOGUE) {
      /* a talk is longer than a message: its own, bigger buffers */
      if (B.n_dlg >= BV_DLG_MAX) { /* more talks than one chunk keeps: this one is skipped, the queue keeps its places */
        B.ev[i] = (bv_event){.type = BEV_WAIT, .battler = -1};
        B.n_ev++;
        continue;
      }
      const int d = B.n_dlg++;
      snprintf(B.dlg_text[d], sizeof B.dlg_text[d], "%s", ev[i].text ? ev[i].text : "");
      snprintf(B.dlg_name[d], sizeof B.dlg_name[d], "%s", ev[i].name ? ev[i].name : "");
      snprintf(B.dlg_atlas[d], sizeof B.dlg_atlas[d], "%s", ev[i].atlas ? ev[i].atlas : "");
      B.ev[i].text = B.dlg_text[d];
      B.ev[i].name = B.dlg_name[d];
      B.ev[i].atlas = B.dlg_atlas[d][0] ? B.dlg_atlas[d] : NULL;
      B.n_ev++;
      continue;
    }
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
  B.hold_cmd = false;
  B.msg.opts.wrap_w = 1780 / 6.0f;
}

bool bv_playing(void) { return B.mode == M_EVENTS; }

void bv_set_item_bars(const bv_item *foe,int n_foe,const bv_item *keys,int n_keys,int luck){
  B.n_foe=B.n_key=0;
  for(int i=0;foe&&i<n_foe&&B.n_foe<BV_MAX_FOE_ITEMS;i++)B.foe[B.n_foe++]=foe[i];
  for(int i=0;keys&&i<n_keys&&B.n_key<BV_MAX_FOE_ITEMS;i++)B.key[B.n_key++]=keys[i];
  B.opts.party_luck=luck;
  if(B.bar_sel>=bar_n()){B.bar_sel=-1;B.bar_open=false;}
}

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
  bv_set_item_bars(opts?opts->foe_items:NULL,opts?opts->n_foe_items:0,
                   opts?opts->key_items:NULL,opts?opts->n_key_items:0,opts?opts->party_luck:0);
  sv_items_close();
  B.opts.foe_items = B.opts.key_items = NULL;
  B.has_balls = opts && opts->has_party_balls;
  if (B.has_balls) memcpy(B.balls, opts->party_balls, sizeof B.balls);
  B.reason_ms = 0;
  B.mode = M_COMMAND;
  B.hold_cmd = false;
  if (B.cmd_cursor == 2) B.cmd_cursor = 0; /* upstream resets POKEMON to FIGHT */
  if (B.cmd_cursor == 4 && !B.opts.can_tera) B.cmd_cursor = 0; /* and the Tera button's cursor when it is not offered */
  B.tera_fight = false;
  /* "What will X do?", wrapped to the space left of the command window (1110 / 6) */
  B.msg.opts.wrap_w = B.has_balls ? 128 : 1110 / 6.0f; /* narrower beside the party tray */
  if (B.opts.can_tera) B.msg.opts.wrap_w -= (1110 - 910) / 6.0f; /* and beside the Tera button (upstream: 910 instead of 1110) */
  char s[128];
  i18n_t(s, sizeof s, "commandUiHandler:actionMessage", "pokemonName", command_side()->mon.name, NULL);
  msgbox_show(&B.msg, s, 0, false);
  B.msg.wait_ms = 1e9f;
  (void)name_with_affix;
}

void bv_ask_target(const bv_target_opts* opts, void (*on_target)(int, void*), void* arg) {
  memcpy(B.tg.ok, opts->ok, sizeof B.tg.ok);
  B.tg.slot = opts->slot & 1;
  snprintf(B.tg.title, sizeof B.tg.title, "%s", opts->title ? opts->title : "");
  B.tg.on_target = on_target;
  B.tg.arg = arg;
  B.hold_cmd = false;
  sv_items_close();
  msgbox_clear(&B.msg);
  B.mode = M_TARGET;
  tg_set(tg_default());
}
