/*
 * battle_anim: upstream's battle animation player (BattleAnim in src/data/battle-anims.ts).
 *
 * Plays the animation files upstream plays (battle-anims/<move>.json, common-<anim>.json,
 * <charge anim>.json, read from data.pak): every 50 ms the next frame places the animation's
 * graphic cells (images/battle_anims/<graphic>, 96x96 cells "0", "1"...) and copies of the
 * user's and target's sprites, relative to where the user and target stand; timed events play
 * sounds (audio/battle_anims/..., or the user's cry) and add or move a background. While it
 * plays, the real user and target sprites are hidden.
 *
 * Moves missing a file fall back as upstream does (TACKLE for attacks, FOCUS_ENERGY for
 * self-status moves, TAIL_WHIP otherwise). Not done yet: the tone's grey part (tone[3]),
 * Substitute, and the frames' color/flash fields (upstream ignores them too).
 */
#ifndef GM_BATTLE_ANIM_H
#define GM_BATTLE_ANIM_H

#include <stdbool.h>

#include "rr.h"

/* A field position as the battle screen shows it (upstream's Pokemon container and sprite). */
typedef struct {
  bool on_field;
  bool player;
  float x, y;             /* the Pokemon's position (its sprite's bottom centre) */
  float half_h;           /* half the sprite's displayed height */
  const rr_frame* frame;  /* the sprite's current frame (copies follow its animation) */
  int species;            /* for the cry */
} ba_mon;

typedef enum { BA_MOVE, BA_COMMON, BA_CHARGE } ba_kind;

/* Where the battle screen draws each layer (upstream's field container order):
 * BELOW_ENEMY after the arena, MID between the enemy and the player, TOP over both. */
typedef enum { BA_LAYER_BELOW_ENEMY, BA_LAYER_MID, BA_LAYER_TOP } ba_layer;

/* Reads a move's animation (and its fallback) and loads its graphics, backgrounds and sounds.
 * Call between scenes: this reads the SD card. */
void ba_preload_move(int move);
/* Frees the move and charge animations and their images (common animations stay). */
void ba_drop_moves(void);

/* Starts an animation: kind and id (MoveId, CommonAnim or ChargeAnim), the user's and the
 * target's battler index (target < 0: the user), the field. Returns false when nothing plays
 * (no animation, or the target isn't on the field); then ba_active() is false. */
bool ba_play(ba_kind kind, int id, int user, int target, const ba_mon field[4]);
bool ba_active(void);
/* Advances the animation; `field` gives the current sprite frames. */
void ba_update(float dt, const ba_mon field[4]);
/* Draws one layer (in the field's coordinates). */
void ba_draw(ba_layer layer, const ba_mon field[4]);
/* Whether the battler's real sprite is hidden: during an animation it takes part in, and after a
 * move with HIDE_USER / HIDE_TARGET (until the screen shows a new Pokemon there). */
bool ba_hidden(int battler);
void ba_show(int battler);

#endif
