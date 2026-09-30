/* upstream's battle animation player (see battle_anim.h), following BattleAnim.play in
 * src/data/battle-anims.ts line by line where it matters: the frame timing, the positions
 * (getGraphicFrameData), which sprites are made and dropped, the priorities (setSpritePriority)
 * and the timed events. */
#include "battle_anim.h"

#include <jansson.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "gm_enum_names.h"
#include "pak.h"
#include "pr_data.h"
#include "pr_enums.h"
#include "snd.h"
#include "tween.h"
#include "ui_widgets.h"

/* upstream getFrameMs(n): floor(1000 / 60 * n) */
static float frame_ms(int n) { return floorf(1000.0f / 60.0f * (float)n); }
#define STEP_MS 50.0f /* getFrameMs(3): one animation frame */

/* ------------------------------------------------------------------ animation files */

enum { TGT_USER, TGT_TARGET, TGT_GRAPHIC };                 /* AnimFrameTarget */
enum { FOCUS_TARGET = 1, FOCUS_USER, FOCUS_USER_TARGET };   /* AnimFocus (SCREEN = 4) */
enum { EV_SOUND, EV_ADD_BG, EV_UPDATE_BG };

typedef struct {
  float x, y, angle;
  float zoom_x, zoom_y;
  int16_t tone[4];
  uint8_t opacity, target, focus, priority, blend;
  uint16_t graphic_frame;
  bool visible, mirror, locked;
} anim_frame;

typedef struct {
  uint8_t type;
  int frame;
  char res[64]; /* resourceName, "" = none */
  int volume, pitch;
  bool has_x, has_y, has_opacity;
  float bg_x, bg_y;
  int opacity, duration;
} anim_event;

typedef struct {
  char graphic[64];
  int n_frames;
  int* start; /* sprite frames of frame i: spr[start[i] .. start[i + 1]) */
  anim_frame* spr;
  int n_events;
  anim_event* ev; /* by frame, in file order within a frame */
} anim_config;

typedef struct {
  char file[64];
  bool keep, missing;
  int n; /* 1, or 2 for a list [player's, enemy's] */
  anim_config* cfg[2];
} anim_file;

#define FILES_MAX 64
static anim_file g_files[FILES_MAX];
static int g_nfiles;

static double num(json_t* o, const char* k, double def) {
  json_t* v = json_object_get(o, k);
  return json_is_number(v) ? json_number_value(v) : def;
}

static void config_free(anim_config* c) {
  if (!c) return;
  free(c->start);
  free(c->spr);
  free(c->ev);
  free(c);
}

/* AnimConfig(source) with ImportedAnimFrame's defaults (missing numbers are 0, focus TARGET) */
static anim_config* config_parse(json_t* o) {
  json_t* frames = json_object_get(o, "frames");
  if (!json_is_object(o) || !json_is_array(frames)) return NULL;
  anim_config* c = calloc(1, sizeof *c);
  json_t* g = json_object_get(o, "graphic");
  snprintf(c->graphic, sizeof c->graphic, "%s", json_is_string(g) ? json_string_value(g) : "");
  size_t nf = json_array_size(frames), ns = 0;
  for (size_t i = 0; i < nf; i++) ns += json_array_size(json_array_get(frames, i));
  c->n_frames = (int)nf;
  c->start = calloc(nf + 1, sizeof *c->start);
  c->spr = calloc(ns ? ns : 1, sizeof *c->spr);
  size_t k = 0;
  for (size_t i = 0; i < nf; i++) {
    json_t* fr = json_array_get(frames, i);
    c->start[i] = (int)k;
    for (size_t j = 0; j < json_array_size(fr); j++) {
      json_t* s = json_array_get(fr, j);
      anim_frame* a = &c->spr[k++];
      a->x = (float)num(s, "x", 0);
      a->y = (float)num(s, "y", 0);
      a->zoom_x = (float)num(s, "zoomX", 0);
      a->zoom_y = (float)num(s, "zoomY", 0);
      a->angle = (float)num(s, "angle", 0);
      a->mirror = json_is_true(json_object_get(s, "mirror"));
      a->visible = json_is_true(json_object_get(s, "visible"));
      a->blend = (uint8_t)num(s, "blendType", 0);
      a->target = (uint8_t)num(s, "target", 0);
      a->graphic_frame = (uint16_t)num(s, "graphicFrame", 0);
      a->opacity = (uint8_t)num(s, "opacity", 0);
      json_t* tone = json_object_get(s, "tone");
      for (int t = 0; t < 4; t++) a->tone[t] = (int16_t)(json_is_array(tone) ? json_number_value(json_array_get(tone, t)) : 0);
      a->locked = json_is_true(json_object_get(s, "locked"));
      a->priority = (uint8_t)num(s, "priority", 0);
      int focus = (int)num(s, "focus", 0);
      a->focus = (uint8_t)(focus ? focus : FOCUS_TARGET);
    }
  }
  c->start[nf] = (int)k;

  json_t* fte = json_object_get(o, "frameTimedEvents");
  size_t ne = 0;
  const char* key;
  json_t* list;
  json_object_foreach(fte, key, list) ne += json_array_size(list);
  c->ev = calloc(ne ? ne : 1, sizeof *c->ev);
  json_object_foreach(fte, key, list) {
    for (size_t j = 0; j < json_array_size(list); j++) {
      json_t* e = json_array_get(list, j);
      const char* type = json_string_value(json_object_get(e, "eventType"));
      if (!type) continue;
      anim_event* v = &c->ev[c->n_events];
      if (!strcmp(type, "AnimTimedSoundEvent"))
        v->type = EV_SOUND;
      else if (!strcmp(type, "AnimTimedAddBgEvent"))
        v->type = EV_ADD_BG;
      else if (!strcmp(type, "AnimTimedUpdateBgEvent"))
        v->type = EV_UPDATE_BG;
      else
        continue;
      v->frame = atoi(key);
      json_t* r = json_object_get(e, "resourceName");
      snprintf(v->res, sizeof v->res, "%s", json_is_string(r) ? json_string_value(r) : "");
      v->volume = (int)num(e, "volume", 100);
      v->pitch = (int)num(e, "pitch", 100);
      v->has_x = json_is_number(json_object_get(e, "bgX"));
      v->has_y = json_is_number(json_object_get(e, "bgY"));
      v->has_opacity = json_is_number(json_object_get(e, "opacity"));
      v->bg_x = (float)num(e, "bgX", 0);
      v->bg_y = (float)num(e, "bgY", 0);
      v->opacity = (int)num(e, "opacity", 0);
      v->duration = (int)num(e, "duration", 0);
      c->n_events++;
    }
  }
  /* by frame, keeping file order within a frame (insertion sort is stable) */
  for (int i = 1; i < c->n_events; i++) {
    anim_event t = c->ev[i];
    int j = i - 1;
    for (; j >= 0 && c->ev[j].frame > t.frame; j--) c->ev[j + 1] = c->ev[j];
    c->ev[j + 1] = t;
  }
  return c;
}

static void kebab(const char* name, char* out, int n) {
  /* toKebabCase of an enum member name: lower case, words joined by single dashes */
  int k = 0;
  bool dash = false;
  for (const char* p = name; *p && k < n - 1; p++) {
    if (*p == '_') {
      dash = k > 0;
      continue;
    }
    if (dash && k < n - 2) out[k++] = '-';
    dash = false;
    out[k++] = (*p >= 'A' && *p <= 'Z') ? (char)(*p - 'A' + 'a') : *p;
  }
  out[k] = 0;
}

static anim_file* file_get(const char* file, bool keep) {
  for (int i = 0; i < g_nfiles; i++)
    if (!strcmp(g_files[i].file, file)) return &g_files[i];
  if (g_nfiles == FILES_MAX) return NULL;
  anim_file* f = &g_files[g_nfiles++];
  memset(f, 0, sizeof *f);
  snprintf(f->file, sizeof f->file, "%s", file);
  f->keep = keep;
  char key[96];
  snprintf(key, sizeof key, "battle-anims/%s.json", file);
  size_t len = 0;
  char* text = pak_read(key, &len);
  json_t* js = text ? json_loadb(text, len, 0, NULL) : NULL;
  free(text);
  if (json_is_array(js)) {
    f->n = json_array_size(js) > 1 ? 2 : 1;
    for (int i = 0; i < f->n; i++) f->cfg[i] = config_parse(json_array_get(js, i));
  } else if (json_is_object(js)) {
    f->n = 1;
    f->cfg[0] = config_parse(js);
  }
  f->missing = !f->cfg[0];
  json_decref(js);
  return f;
}

static anim_file* move_file(int move) {
  const char* name = gm_enum_name(GM_E_MOVE_ID, move);
  char file[64];
  kebab(name ? name : "", file, sizeof file);
  anim_file* f = file_get(file, false);
  if (f && !f->missing) return f;
  /* initMoveAnim's default: TACKLE for AttackMove (and ChargingAttackMove), FOCUS_ENERGY for
   * SelfStatusMove (and ChargingSelfStatusMove), TAIL_WHIP otherwise */
  const char* kind = move >= 0 && move < PR_MOVE_COUNT ? pr_moves[move].kind : "";
  const char* def = strstr(kind, "AttackMove") ? "tackle" : strstr(kind, "SelfStatusMove") ? "focus-energy" : "tail-whip";
  return file_get(def, false);
}

static anim_file* kind_file(ba_kind kind, int id) {
  if (kind == BA_MOVE) return move_file(id);
  const char* name = gm_enum_name(kind == BA_COMMON ? GM_E_COMMON_ANIM : GM_E_CHARGE_ANIM, id);
  if (!name) return NULL;
  char file[64];
  int k = 0;
  if (kind == BA_COMMON) k = snprintf(file, sizeof file, "common-");
  kebab(name, file + k, (int)sizeof file - k);
  return file_get(file, kind == BA_COMMON);
}

static void graphic_key(const char* name, char* out, int n) { snprintf(out, n, "images/battle_anims/%s", name); }

static void sound_key(const char* res, char* out, int n) {
  /* resourceName keeps upstream's file extension; snd.pak keys don't */
  snprintf(out, n, "audio/battle_anims/%s", res);
  char* dot = strrchr(out, '.');
  if (dot && !strchr(dot, '/')) *dot = 0;
}

static void preload_config(const anim_config* c) {
  if (!c) return;
  char k[128];
  if (c->graphic[0]) {
    graphic_key(c->graphic, k, sizeof k);
    img_get(k);
  }
  for (int i = 0; i < c->n_events; i++) {
    const anim_event* e = &c->ev[i];
    if (!e->res[0]) continue;
    if (e->type == EV_ADD_BG) {
      graphic_key(e->res, k, sizeof k);
      img_get(k);
    } else if (e->type == EV_SOUND) {
      sound_key(e->res, k, sizeof k);
      snd_sfx_preload(k);
    }
  }
}

void ba_preload_move(int move) {
  anim_file* f = move_file(move);
  if (!f) return;
  for (int i = 0; i < f->n; i++) preload_config(f->cfg[i]);
}

void ba_drop_moves(void) {
  int k = 0;
  for (int i = 0; i < g_nfiles; i++) {
    anim_file* f = &g_files[i];
    if (f->keep) {
      g_files[k++] = *f;
      continue;
    }
    for (int j = 0; j < f->n; j++) config_free(f->cfg[j]);
  }
  g_nfiles = k;
  img_drop_prefix("images/battle_anims/");
}

/* ------------------------------------------------------------------ playing */

#define SPR_MAX 40

typedef struct {
  float x, y, angle, sx, sy;
  int graphic_frame;
  uint8_t alpha, blend;
  int16_t tone[3];
  bool visible, locked;
  int prio;      /* graphics: the priority last applied */
  ba_layer layer;
  float z;       /* order within the layer */
} spr_t;

typedef struct {
  spr_t s[SPR_MAX];
  int n;
} spr_list;

static struct {
  bool active;
  const anim_config* cfg;
  ba_kind kind;
  int id;
  int this_user, this_target; /* upstream's this.user / this.target */
  int user, target;           /* swapped for an opponent's list animation (isOppAnim) */
  bool hiding;                /* the real sprites are hidden (from the first frame) */
  float t;
  int f, r;
  bool counted;               /* the frame counter finished (onComplete ran) */
  float end_ms;               /* when cleanUpAndComplete runs */
  float src[4], dst[4];       /* srcLine, dstLine */
  spr_list g, u, tg;          /* GRAPHIC, USER, TARGET sprite caches */
  char gkey[96];
  /* bgSprite */
  bool bg;
  char bg_key[96];            /* "" = the black rectangle */
  float bg_x, bg_y, bg_alpha;
  ba_layer bg_layer;
  float bg_z;
  int bg_above;               /* sprites moved above the bg so far */
  tween tw_x, tw_y, tw_a;
  float seq, low;
} A;

static bool g_hidden_after[4];

bool ba_active(void) { return A.active; }

bool ba_hidden(int b) {
  if (b < 0 || b > 3) return false;
  return g_hidden_after[b] || (A.active && A.hiding && (b == A.user || b == A.target));
}

void ba_show(int b) {
  if (b >= 0 && b < 4) g_hidden_after[b] = false;
}

/* ---- priorities: where a sprite sits in the field container ---- */

static void to_top(spr_t* s) { s->layer = BA_LAYER_TOP, s->z = ++A.seq; }

/* moveBelow / moveAbove a Pokemon: the enemy is drawn before the player (BELOW_ENEMY, enemy,
 * MID, player, TOP) */
static void below_mon(spr_t* s, const ba_mon* m) {
  s->layer = m->player ? BA_LAYER_MID : BA_LAYER_BELOW_ENEMY;
  s->z = ++A.seq;
}
static void above_mon(spr_t* s, const ba_mon* m) {
  s->layer = m->player ? BA_LAYER_TOP : BA_LAYER_MID;
  s->z = -(++A.low);
}

static void set_priority(spr_t* s, int prio, int focus, const ba_mon* field) {
  if (prio == 0) {
    /* in front of the field: below the first enemy on the field, else the first player */
    const ba_mon* m = field[2].on_field ? &field[2] : field[3].on_field ? &field[3]
                    : field[0].on_field ? &field[0] : field[1].on_field ? &field[1] : NULL;
    if (m) below_mon(s, m);
    else to_top(s);
  } else if (prio == 2 && A.bg) {
    s->layer = A.bg_layer;
    s->z = A.bg_z + 0.5f / (float)(++A.bg_above);
  } else if (prio == 2 || prio == 3) {
    int b = focus == FOCUS_USER ? A.this_user : focus == FOCUS_TARGET ? A.this_target : -1;
    if (b >= 0 && field[b].on_field) {
      if (prio == 2) below_mon(s, &field[b]);
      else above_mon(s, &field[b]);
    } else {
      to_top(s);
    }
  } else {
    to_top(s);
  }
}

/* ---- getGraphicFrameData ---- */

static void transform_point(const float* s, const float* d, float px, float py, float* ox, float* oy) {
  float dx = s[2] - s[0], dy = s[3] - s[1];
  float tx = dx == 0 ? 0 : (px - s[0]) / dx;
  float ty = dy == 0 ? 0 : (py - s[1]) / dy;
  *ox = d[0] + tx * (d[2] - d[0]);
  *oy = d[1] + ty * (d[3] - d[1]);
}

static bool is_reversed(float s1, float s2, float d1, float d2) {
  if (s1 == s2) return false;
  return s1 < s2 ? d1 > d2 : d1 < d2;
}

static void frame_pos(const anim_frame* fr, const ba_mon* field, float* x, float* y, float* sx, float* sy) {
  const ba_mon* user = &field[A.user];
  const ba_mon* target = &field[A.target];
  float px = fr->x + 106, py = fr->y + 116;
  *sx = fr->zoom_x / 100 * (fr->mirror ? -1 : 1);
  *sy = fr->zoom_y / 100;
  switch (fr->focus) {
    case FOCUS_TARGET:
      px += A.dst[2] - 234;
      py += A.dst[3] - target->half_h - (84 - 32);
      break;
    case FOCUS_USER:
      px += A.dst[0] - 106;
      py += A.dst[1] - user->half_h - (148 - 32);
      break;
    case FOCUS_USER_TARGET: {
      float d[4] = {A.dst[0], A.dst[1] - user->half_h, A.dst[2], A.dst[3] - target->half_h};
      transform_point(A.src, d, px, py, &px, &py);
      if (fr->target == TGT_GRAPHIC && is_reversed(A.src[0], A.src[2], A.dst[0], A.dst[2])) *sx = -*sx;
      break;
    }
    default:
      break;
  }
  *x = px;
  *y = py;
}

/* ---- timed events ---- */

static int cry_frames(int battler, const ba_mon* field, float volume, float rate) {
  char k[48];
  snprintf(k, sizeof k, "audio/cry/%d", field[battler].species);
  if (!snd_sfx_play_ex(k, volume, rate)) return 0;
  return (int)ceilf(snd_sfx_seconds(k) * 1000.0f / 33.33f);
}

static int run_event(const anim_event* e, const ba_mon* field) {
  switch (e->type) {
    case EV_SOUND: {
      float vol = e->volume * 0.01f, rate = e->pitch * 0.01f;
      if (!e->res[0]) return cry_frames(A.this_user, field, vol, rate);
      char k[128];
      sound_key(e->res, k, sizeof k);
      snd_sfx_play_ex(k, vol, rate);
      float s = snd_sfx_seconds(k);
      return s > 0 ? (int)ceilf(s * 1000.0f / 33.33f) : 0;
    }
    case EV_ADD_BG: {
      A.bg = true;
      if (e->res[0]) graphic_key(e->res, A.bg_key, sizeof A.bg_key);
      else A.bg_key[0] = 0;
      A.bg_x = e->bg_x - 320;
      A.bg_y = e->bg_y - 284;
      A.bg_alpha = e->opacity / 255.0f;
      memset(&A.tw_x, 0, sizeof A.tw_x);
      memset(&A.tw_y, 0, sizeof A.tw_y);
      memset(&A.tw_a, 0, sizeof A.tw_a);
      A.bg_above = 0;
      /* moveBelow(the enemy on the field, else the player) */
      spr_t s;
      const ba_mon* m = field[2].on_field ? &field[2] : field[0].on_field ? &field[0] : NULL;
      if (m) below_mon(&s, m);
      else to_top(&s);
      A.bg_layer = s.layer;
      A.bg_z = s.z;
      return e->duration * 2;
    }
    case EV_UPDATE_BG:
      if (A.bg) {
        float ms = frame_ms(e->duration * 3);
        if (e->has_x) tween_start(&A.tw_x, A.bg_x, e->bg_x * 0.5f - 320, ms, EASE_LINEAR);
        if (e->has_y) tween_start(&A.tw_y, A.bg_y, e->bg_y * 0.5f - 284, ms, EASE_LINEAR);
        if (e->has_opacity) tween_start(&A.tw_a, A.bg_alpha, e->opacity / 255.0f, ms, EASE_LINEAR);
      }
      return e->duration * 2;
  }
  return 0;
}

/* ---- one frame (the counter's onRepeat) ---- */

static void drop_extra(spr_list* l, int count) {
  /* sprites past this frame's count go, unless locked */
  int k = count;
  for (int i = count; i < l->n; i++)
    if (l->s[i].locked) l->s[k++] = l->s[i];
  l->n = k;
}

static void apply_frame(const ba_mon* field) {
  const anim_config* c = A.cfg;
  int f = A.f;
  if (!f) A.hiding = true;
  int g = 0, u = 0, t = 0;
  for (int i = c->start[f]; i < c->start[f + 1]; i++) {
    const anim_frame* fr = &c->spr[i];
    float x, y, sx, sy;
    frame_pos(fr, field, &x, &y, &sx, &sy);
    spr_t* s;
    if (fr->target == TGT_GRAPHIC) {
      if (g == A.g.n) {
        if (A.g.n == SPR_MAX) continue;
        s = &A.g.s[A.g.n++];
        memset(s, 0, sizeof *s);
        to_top(s); /* field.add */
        s->prio = 1;
      }
      s = &A.g.s[g++];
      if (s->prio != fr->priority) {
        s->prio = fr->priority;
        set_priority(s, fr->priority, fr->focus, field);
      }
      s->graphic_frame = fr->graphic_frame;
      s->tone[0] = s->tone[1] = s->tone[2] = 0;
    } else {
      bool is_user = fr->target == TGT_USER;
      if (is_user && A.target == A.user) continue;
      spr_list* l = is_user ? &A.u : &A.tg;
      int* n = is_user ? &u : &t;
      if (*n == l->n) {
        if (l->n == SPR_MAX) continue;
        s = &l->s[l->n++];
        memset(s, 0, sizeof *s);
        to_top(s);
      }
      s = &l->s[(*n)++];
      s->locked = fr->locked;
      for (int k = 0; k < 3; k++) s->tone[k] = fr->tone[k];
    }
    s->x = x;
    s->y = y;
    s->angle = -fr->angle;
    s->sx = sx;
    s->sy = sy;
    s->alpha = fr->opacity;
    s->visible = fr->visible;
    s->blend = fr->blend;
  }
  int base = c->n_frames - f;
  for (int i = 0; i < c->n_events; i++)
    if (c->ev[i].frame == f) {
      int r = base + run_event(&c->ev[i], field);
      if (r > A.r) A.r = r;
    }
  /* graphics are dropped whatever their lock (only Pokemon copies keep the data) */
  for (int i = g; i < A.g.n; i++) A.g.s[i].locked = false;
  drop_extra(&A.g, g);
  drop_extra(&A.u, u);
  drop_extra(&A.tg, t);
  A.f++;
  A.r--;
}

static void finish(void) {
  /* cleanUpAndComplete: the real sprites come back, except after a move that hides them */
  bool hide_user = false, hide_target = false;
  if (A.kind == BA_MOVE && A.id >= 0 && A.id < PR_MOVE_COUNT) {
    hide_user = pr_moves[A.id].flags & MOVE_FLAGS_HIDE_USER;
    hide_target = pr_moves[A.id].flags & MOVE_FLAGS_HIDE_TARGET;
  }
  if (A.hiding) {
    if (hide_user) g_hidden_after[A.this_user] = true;
    if (hide_target && (A.this_target != A.this_user || hide_user)) g_hidden_after[A.this_target] = true;
  }
  A.active = false;
  A.g.n = A.u.n = A.tg.n = 0;
  A.bg = false;
}

bool ba_play(ba_kind kind, int id, int user, int target, const ba_mon field[4]) {
  A.active = false;
  if (user < 0 || user > 3) return false;
  /* no target: the user (MoveAnim's getField()[target] ?? user, CommonBattleAnim's target ||
   * user); nothing plays when the target isn't on the field */
  if (target < 0 || target > 3) target = user;
  if (!field[target].on_field) return false;
  anim_file* f = kind_file(kind, id);
  bool player = user < 2;
  /* MoveAnim / MoveChargeAnim: a list holds [player's, enemy's]; isOppAnim when the enemy uses
   * a list animation. CommonBattleAnim: never. */
  bool list = f && f->n == 2 && kind != BA_COMMON;
  const anim_config* cfg = f ? f->cfg[list && !player ? 1 : 0] : NULL;
  bool opp = list && !player;
  memset(&A, 0, sizeof A);
  A.kind = kind;
  A.id = id;
  A.cfg = cfg;
  A.this_user = user;
  A.this_target = target;
  A.user = opp ? target : user;
  A.target = opp ? user : target;
  A.src[0] = 106, A.src[1] = 148 - 32, A.src[2] = 234, A.src[3] = 84 - 32;
  A.dst[0] = field[A.user].x, A.dst[1] = field[A.user].y;
  A.dst[2] = field[A.target].x, A.dst[3] = field[A.target].y;
  A.r = cfg ? cfg->n_frames : 0;
  if (cfg && cfg->graphic[0]) graphic_key(cfg->graphic, A.gkey, sizeof A.gkey);
  A.active = true;
  return true;
}

void ba_update(float dt, const ba_mon field[4]) {
  if (!A.active) return;
  A.t += dt;
  int n = A.cfg ? A.cfg->n_frames : 0;
  /* the counter: frame f shows at 50 (f + 1) ms, onComplete at 50 (n + 1) ms */
  while (A.f < n && A.t >= STEP_MS * (float)(A.f + 1)) apply_frame(field);
  if (A.bg) {
    tween* tw[3] = {&A.tw_x, &A.tw_y, &A.tw_a};
    float* v[3] = {&A.bg_x, &A.bg_y, &A.bg_alpha};
    for (int i = 0; i < 3; i++)
      if (!tween_done(tw[i])) {
        tween_update(tw[i], dt);
        *v[i] = tw[i]->value;
      }
  }
  if (!A.counted && A.f >= n && A.t >= STEP_MS * (float)(n + 1)) {
    A.counted = true;
    int k = 0;
    A.g.n = 0; /* graphics are never locked */
    for (int i = 0; i < A.u.n; i++)
      if (A.u.s[i].locked) A.u.s[k++] = A.u.s[i];
    A.u.n = k;
    k = 0;
    for (int i = 0; i < A.tg.n; i++)
      if (A.tg.s[i].locked) A.tg.s[k++] = A.tg.s[i];
    A.tg.n = k;
    A.end_ms = A.t + (A.r > 0 ? frame_ms(A.r) : 0);
  }
  if (A.counted && A.t >= A.end_ms) finish();
}

/* ---- drawing ---- */

typedef struct {
  float z;
  int kind; /* 0 bg, 1 graphic, 2 user copy, 3 target copy */
  const spr_t* s;
} item;

static void draw_bg(void) {
  rr_color tint = RR_RGBA(255, 255, 255, (int)(A.bg_alpha * 255 + 0.5f));
  if (!A.bg_key[0]) {
    rr_rect(A.bg_x, A.bg_y, 896 * 1.25f, 576 * 1.25f, RR_RGBA(0, 0, 0, (int)(A.bg_alpha * 255 + 0.5f)));
    return;
  }
  const rr_frame* f = img_frame(A.bg_key, "__BASE");
  if (!f) f = img_frame(A.bg_key, NULL);
  if (!f || f->src_w <= 0 || f->src_h <= 0) return;
  /* a 896x576 tile sprite at scale 1.25: the image repeats every 1.25 x its size */
  float tw = f->src_w * 1.25f, th = f->src_h * 1.25f;
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.scale_x = o.scale_y = 1.25f;
  o.tint = tint;
  int i0 = A.bg_x < 0 ? (int)floorf(-A.bg_x / tw) : 0;
  int j0 = A.bg_y < 0 ? (int)floorf(-A.bg_y / th) : 0;
  for (int j = j0; (float)j * th < 576 * 1.25f && A.bg_y + (float)j * th < 180; j++)
    for (int i = i0; (float)i * tw < 896 * 1.25f && A.bg_x + (float)i * tw < 320; i++)
      rr_draw(f, A.bg_x + (float)i * tw, A.bg_y + (float)j * th, &o);
}

static void draw_spr(const spr_t* s, const rr_frame* f) {
  if (!f || !s->visible || !s->alpha) return;
  rr_draw_opts o = RR_DEFAULT_OPTS;
  o.origin_x = o.origin_y = 0.5f;
  o.scale_x = s->sx;
  o.scale_y = s->sy;
  o.angle = s->angle * (float)M_PI / 180.0f;
  o.tint = RR_RGBA(255, 255, 255, s->alpha);
  /* BlendModes.ADD for ADD; SUBTRACT maps to DIFFERENCE, which WebGL draws as NORMAL */
  rr_set_blend(s->blend == 1 ? RR_BLEND_ADD : RR_BLEND_NORMAL);
  rr_set_tone(s->tone[0], s->tone[1], s->tone[2]);
  rr_draw(f, s->x, s->y, &o);
}

void ba_draw(ba_layer layer, const ba_mon field[4]) {
  if (!A.active) return;
  item it[1 + 3 * SPR_MAX];
  int n = 0;
  if (A.bg && A.bg_layer == layer) it[n++] = (item){A.bg_z, 0, NULL};
  for (int i = 0; i < A.g.n; i++)
    if (A.g.s[i].layer == layer) it[n++] = (item){A.g.s[i].z, 1, &A.g.s[i]};
  for (int i = 0; i < A.u.n; i++)
    if (A.u.s[i].layer == layer) it[n++] = (item){A.u.s[i].z, 2, &A.u.s[i]};
  for (int i = 0; i < A.tg.n; i++)
    if (A.tg.s[i].layer == layer) it[n++] = (item){A.tg.s[i].z, 3, &A.tg.s[i]};
  if (!n) return;
  for (int i = 1; i < n; i++) {
    item t = it[i];
    int j = i - 1;
    for (; j >= 0 && it[j].z > t.z; j--) it[j + 1] = it[j];
    it[j + 1] = t;
  }
  for (int i = 0; i < n; i++) {
    const spr_t* s = it[i].s;
    switch (it[i].kind) {
      case 0:
        rr_set_blend(RR_BLEND_NORMAL);
        rr_set_tone(0, 0, 0);
        draw_bg();
        break;
      case 1: {
        char name[8];
        snprintf(name, sizeof name, "%d", s->graphic_frame);
        draw_spr(s, A.gkey[0] ? img_frame(A.gkey, name) : NULL);
        break;
      }
      case 2:
        if (field[A.user].on_field) draw_spr(s, field[A.user].frame);
        break;
      case 3:
        if (field[A.target].on_field) draw_spr(s, field[A.target].frame);
        break;
    }
  }
  rr_set_blend(RR_BLEND_NORMAL);
  rr_set_tone(0, 0, 0);
}
