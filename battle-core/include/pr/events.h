/* What a turn shows: the battle events the game screen plays back.
 *
 * pr_run_turn, pr_resume_switch and pr_continue each start a fresh list in e->events and append one
 * event for every presentation step upstream takes, in the order upstream takes them.
 * The screen plays them one by one, then reads bt for anything else it shows. Each event
 * carries the field as it stands once the event has happened (pr_event_field), so the
 * screen can move HP bars and status icons to it without knowing the rules.
 *
 * Status (2026-09-30): every message the engine shows has its text; the turn test checks
 * the texts of each covered turn against upstream's (identical on all but a setup
 * artifact). The other events follow upstream's code in the same order but are not
 * compared with upstream by the test. */
#ifndef PR_EVENTS_H
#define PR_EVENTS_H
#include <stdbool.h>
#include <stdint.h>
#include "pr/text.h"

typedef enum {
    PR_EV_NONE,
    /* A text box (MessagePhase, or a phase's own ui.showText). msg; battler = -1.
     * upstream shows it for 1.5 s or until a button press when prompt is set */
    PR_EV_TEXT,
    /* A move's animation (MoveAnim in MoveEffectPhase, each hit, when the move succeeded
     * against a target or has a POST_TARGET effect): id = MoveId, battler = user, target =
     * the target it plays against (one event per target on the field; the user itself
     * when none is left). Comes before the hit's DAMAGE and texts */
    PR_EV_MOVE_ANIM,
    /* A charge animation (MoveChargeAnim: a two-turn move's first turn, Future Sight and
     * Doom Desire): id = ChargeAnim, battler = user, target = its target or -1 */
    PR_EV_CHARGE_ANIM,
    /* CommonAnimPhase: id = CommonAnim (weather, status, Leech Seed, stat up/down...),
     * battler = where it plays, target = second position when it has one, else -1 */
    PR_EV_COMMON_ANIM,
    /* DamageAnimPhase: battler hit; value = damage; id = HitResult (effective, super
     * effective, not very effective, critical... picks the sound and flashes) */
    PR_EV_DAMAGE,
    /* HP changed with no damage animation: a heal (value = HP gained, as upstream's
     * damage number shows it) or status damage (value < 0): battler */
    PR_EV_HP,
    /* Stat stage arrows (StatStageChangePhase): battler; value = a bit per Stat changed;
     * id = 1 rising, 0 falling */
    PR_EV_STAT_STAGES,
    /* The ability bar (ShowAbilityPhase / HideAbilityPhase): battler; id = AbilityId;
     * value = 1 when it is the passive. HIDE has battler -1 */
    PR_EV_ABILITY_SHOW,
    PR_EV_ABILITY_HIDE,
    /* PokemonAnimPhase: id = PokemonAnimType (Substitute doll added, moved, removed...) */
    PR_EV_POKEMON_ANIM,
    /* The Pokemon faints (cry, sprite drops): battler */
    PR_EV_FAINT,
    /* A Pokemon leaves the field into its ball (withdrawn, dragged out, fled): battler */
    PR_EV_SWITCH_OUT,
    /* A Pokemon is sent out: battler; value = its party slot. field[battler] already shows it */
    PR_EV_SWITCH_IN,
    /* ShinySparklePhase: battler */
    PR_EV_SHINY_SPARKLE,
    /* A trainer's switch (SwitchSummonPhase.showEnemyTrainer): the enemy trainer and its
     * party's ball tray slide in; id = TrainerSlot. HIDE: they slide out again, right
     * before the send-out text */
    PR_EV_TRAINER_SHOW,
    PR_EV_TRAINER_HIDE,
    PR_EV_TRANSFORM,           /* battler; value = play sound (Imposter only); transform = display state */
    PR_EV_COUNT_
} pr_event_type;

/* One field position after the event. species 0 = nobody there. */
typedef struct {
    uint16_t species;
    uint8_t form;
    int8_t party_slot;         /* index in bt->party[side] */
    int32_t hp, max_hp;
    uint8_t status;            /* StatusEffect */
    bool fainted;
    bool substitute;           /* a Substitute doll stands in front of it */
    bool visible;              /* drawn: on the field and not in the air, underground or underwater (a
                                * SemiInvulnerableTag: Fly, Bounce, Dig, Dive, Shadow Force, Phantom Force,
                                * Sky Drop). Upstream shows it again two frames after the tag goes. A
                                * fainting Pokemon is still visible in the FAINT event (its animation
                                * starts there) and hidden in the events after it. */
    int8_t gender, types[3];
    uint8_t variant, n_types;
    float sprite_scale;
} pr_event_field;

typedef struct {
    uint8_t type;              /* pr_event_type */
    int8_t battler;            /* BattlerIndex 0-3 (player 0/1, enemy 2/3), -1 = none */
    int8_t target;             /* a second BattlerIndex, -1 = none */
    uint16_t id;               /* see the type */
    int32_t value;             /* see the type */
    pr_msg msg;                /* PR_EV_TEXT */
    uint8_t weather, terrain;  /* WeatherType and TerrainType after the event */
    pr_event_field field[4];   /* by BattlerIndex, after the event */
    struct {
        int8_t gender, types[3], stat_stages[7];
        uint8_t n_types, n_moves;
        uint16_t moves[4];
        int16_t pp_max[4], pp_left[4];
        float sprite_scale;
    } transform;
} pr_event;

#define PR_MAX_EVENTS 192

#endif
