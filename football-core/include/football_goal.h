#ifndef FOOTBALL_GOAL_H
#define FOOTBALL_GOAL_H
/* The goal posts. A kick is aimed at the plane FB_GOAL_POST_INSET yards inside each end line, at the middle of the field's width; a good
 * kick ends within a yard and a half or so of the middle, a miss starts FB_GOAL_MISS_MIN yards out (football_live_special.c), and the posts stand
 * exactly there so the picture says what the engine decided: the stadium (game/source/football_stadium.c) draws them from these numbers. */
#define FB_GOAL_POST_INSET 1.0f      /* yards inside each end line */
#define FB_GOAL_MIDDLE 26.67f        /* the middle of the width of the field, in yards from the near sideline */
#define FB_GOAL_HALF_WIDTH 3.1f      /* yards from the middle to each upright (18 ft 6 in apart) */
/* How the kick looks from the side camera. The ball is drawn FB_BALL_LIFT_PX pixels higher for every unit of ball_z, and a goal kick passes
 * the posts at FB_GOAL_CROSS_Z of them, so the screen position of a kick over the posts is its spot along the width (a yard is
 * FB_CAMERA_SCALE * FB_CAMERA_SQUASH pixels) minus that lift. The stadium puts the two upright caps where a ball FB_GOAL_HALF_WIDTH yards
 * either side of the middle would pass, so a good kick (inside them) goes between the caps and a wide one (at least FB_GOAL_MISS_MIN yards out,
 * clear of the upright and the ball's own size) goes outside one. */
#define FB_BALL_LIFT_PX 5.0f
#define FB_GOAL_CROSS_Z 9.0f
#define FB_GOAL_MISS_MIN 5.6f
/* The height of a goal kick t (0..1) of the way to the posts: it rises to its top 70% of the way and is still well up as it passes them. */
static inline float football_goal_ball_z(float t){return 2.5f*FB_GOAL_CROSS_Z*t*(1.4f-t);}
#endif
