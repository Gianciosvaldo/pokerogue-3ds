#ifndef FOOTBALL_PLAYBOOK_H
#define FOOTBALL_PLAYBOOK_H
#include "playcall_ui.h"

#define FB_ROUTE_POINTS 4
typedef struct { float x, y; } FootballRoutePoint;
typedef struct {
    FootballPlayId id;
    FootballFormation formation;
    FootballPlayType type;
    const char *name, *description;
    /* Small additive balance modifiers, applied before probability/yard clamps. */
    int8_t gain, completion, sack, interception, speed_weight;
    uint8_t route_count;
    FootballRoutePoint route[FB_ROUTE_POINTS];
} FootballPlayDefinition;

typedef struct { const FootballRoutePoint *point; unsigned count; } FootballRouteView;
/* Actor rank 1 RB, 2/3/4 WR1/2/3, 5 TE. Primary geometry remains in route[]. */
FootballRoutePoint football_playbook_sample_path(FootballRouteView route,float progress);
FootballRouteView football_playbook_receiver_path(const FootballPlayDefinition *play,unsigned rank);
unsigned football_playbook_primary_receiver(const FootballPlayDefinition *play);
unsigned football_playbook_runner(const FootballPlayDefinition *play);
bool football_playbook_screen(const FootballPlayDefinition *play);
bool football_playbook_settles(const FootballPlayDefinition *play);
/* How long the CPU quarterback gives the routes before he looks for the open man, in seconds (the live engine; the older six-player model
 * does not use it): longer for deep concepts and for under-center sets, from the play's `gain`; a screen waits for the wall, a bubble throws at once. */
float football_playbook_window(const FootballPlayDefinition *play);
/* Play action: the back's route runs through the mesh point beside the QB, and for the first FB_LIVE_FAKE seconds the linebackers (zone) and
 * safeties step up to meet it. */
bool football_playbook_fake(const FootballPlayDefinition *play);
/* Where the five eligible players line up. Spots are yards behind the line of scrimmage (depth <= 0) and the lateral y of a 53.3-yard
 * field in the unmirrored version; the live engine mirrors a whole alignment about the middle of the field at random, so nothing may
 * assume a side. The RB stands rb_dy from the QB's line (depth is the base RB depth plus rb_back). wr[] is WR1, WR2, WR3, TE. */
typedef struct { float depth,y; } FootballAlignSpot;
typedef enum { FB_ALIGN_SPREAD,FB_ALIGN_TRIPS,FB_ALIGN_TIGHT,FB_ALIGN_JET,FB_ALIGN_COUNT } FootballAlignmentId;
typedef struct { const char *name;float rb_dy,rb_back;FootballAlignSpot wr[4]; } FootballAlignment;
const FootballAlignment *football_playbook_alignment(FootballAlignmentId id);
/* The alignment a play uses: option 0 is its usual one, option 1 its alternate. */
FootballAlignmentId football_playbook_alignment_for(const FootballPlayDefinition *play,unsigned option);
/* Who escorts the runner: nobody, the RB (the sweep) or the wing, WR3 (power, counter, toss). */
typedef enum { FB_LEAD_NONE,FB_LEAD_RB,FB_LEAD_WING } FootballLead;
FootballLead football_playbook_lead(const FootballPlayDefinition *play);
/* How the QB gives the ball over. `side` says where the runner takes it: where he lined up, toward the side the run goes to (strong), or
 * the other way (the counter step); `across` is how far to that side of the QB; `delay` is how long the runner waits before he starts
 * for the mesh (the draw shows pass first). */
typedef enum { FB_MESH_LINED,FB_MESH_STRONG,FB_MESH_WEAK } FootballMeshSide;
typedef struct { uint8_t side;float across,delay; } FootballHandoff;
FootballHandoff football_playbook_handoff(const FootballPlayDefinition *play);
/* True for the runs that go to the strong side, the side the tight end lines up on (the engine picks run_side from it). */
bool football_playbook_strong_side(const FootballPlayDefinition *play);
/* Immutable ROM data, stable legacy IDs 0..7, no allocation or RNG. */
const FootballPlayDefinition *football_playbook_get(FootballPlayId id);
unsigned football_playbook_count(FootballFormation formation, FootballPlayType type);
const FootballPlayDefinition *football_playbook_option(FootballFormation formation,
                                                       FootballPlayType type, unsigned option);
bool football_playbook_valid(PlayInfo play);
/* Sample receiver-relative path by segment progress. y is inside-positive.
 * Callers mirror y for the lower receiver and x for the away offense. */
FootballRoutePoint football_playbook_route(const FootballPlayDefinition *play, float progress);
#endif
